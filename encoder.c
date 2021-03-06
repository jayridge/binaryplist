#include "binaryplist.h"


/*
 * Routines for writing the binary plist.
 *
 */

static void write_byte(binaryplist_encoder *encoder, uint8_t b)
{
    utstring_bincpy(encoder->output, &b, sizeof(uint8_t));
}

static void write_bytes(binaryplist_encoder *encoder, void *bytes, size_t len)
{
    utstring_bincpy(encoder->output, bytes, len);
}

static void write_multi_be(binaryplist_encoder *encoder, long value, int nbytes)
{
    int i;

    for (i = (nbytes - 1); i >= 0; i--) {
        write_byte(encoder, (uint8_t)(value >> (8 * i)));
    }
}

static void write_long(binaryplist_encoder *encoder, long value)
{
    write_multi_be(encoder, value, 8);
}

static void write_int_header(binaryplist_encoder *encoder, int kind, int value)
{
    if (value < 15) {
        write_byte(encoder, (kind << 4) + value);
    } else if (value < 256) {
        write_byte(encoder, (kind << 4) + 15);
        write_byte(encoder, 0x10);
        write_multi_be(encoder, value, 1);
    } else if (value < 65536) {
        write_byte(encoder, (kind << 4) + 15);
        write_byte(encoder, 0x11);
        write_multi_be(encoder, value, 2);
    } else {
        write_byte(encoder, (kind << 4) + 15);
        write_byte(encoder, 0x12);
        write_multi_be(encoder, value, 4);
    }
}

static void write_id(binaryplist_encoder *encoder, long val)
{
    write_multi_be(encoder, val, encoder->ref_id_sz);
}

static void write_int(binaryplist_encoder *encoder, long val)
{
    if (val < 0) {
        write_byte(encoder, 0x13);
        write_multi_be(encoder, val, 8);
    } else if (val <= 0xff) {
        write_byte(encoder, 0x10);
        write_multi_be(encoder, val, 1);
    } else if (val <= 0xffff) {
        write_byte(encoder, 0x11);
        write_multi_be(encoder, val, 2);
    } else if (val <= 0xffffffffL) {
        write_byte(encoder, 0x12);
        write_multi_be(encoder, val, 4);
    } else {
        write_byte(encoder, 0x13);
        write_multi_be(encoder, val, 8);
    }
}

static long get_reference_id(binaryplist_encoder *encoder, PyObject *obj)
{
    long ref;
    PyObject *ol = PyLong_FromVoidPtr((void *)obj);
    ref =  PyLong_AsLong(PyDict_GetItem(encoder->ref_table, ol));
    Py_DECREF(ol);
    return ref;
}

static void write_dict(binaryplist_encoder *encoder, PyObject *obj)
{
    Py_ssize_t i = 0;
    PyObject *key, *value;
    Py_ssize_t size = PyDict_Size(obj);
    PyDictObject *mp = (PyDictObject*) obj;

    write_int_header(encoder, 0xD, size);
    while (PyDict_Next((PyObject *)mp, &i, &key, &value)) {
        write_id(encoder, get_reference_id(encoder, key));
    }
    i = 0;
    while (PyDict_Next((PyObject *)mp, &i, &key, &value)) {
        write_id(encoder, get_reference_id(encoder, value));
    }
}

static void write_list(binaryplist_encoder *encoder, PyObject *obj)
{
    Py_ssize_t i = 0;
    PyListObject *v = (PyListObject*) obj;

    write_int_header(encoder, 0xA, v->ob_size);
    for (i = 0; i < v->ob_size; i++) {
        write_id(encoder, get_reference_id(encoder, v->ob_item[i]));
    }
}

static void write_tuple(binaryplist_encoder *encoder, PyObject *obj)
{
    Py_ssize_t i = 0;
    PyTupleObject *v = (PyTupleObject*) obj;

    write_int_header(encoder, 0xA, v->ob_size);
    for (i = 0; i < v->ob_size; ++i) {
        write_id(encoder, get_reference_id(encoder, v->ob_item[i]));
    }
}

static int offset_size(long n) {
    if (n < 256) return 1;
    if (n < 65536) return 2;
    if (n < 4294967296ULL) return 4;
    return 8;
}

static int ref_id_size(int n) {
    if (n < 256) return 1;
    if (n < 65536) return 2;
    return 4;
}

static inline uint64_t double_to_raw(double x) {
    uint64_t bits;
    memcpy(&bits, &x, sizeof bits);
    return bits;
}

int encoder_write(binaryplist_encoder *encoder)
{
    int i, off_sz;
    PyListObject *objects = (PyListObject*) encoder->objects;
    PyObject *object, *tmp;
    Py_ssize_t len;
    long off_pos, lval, *offsets;
    uint8_t padding[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    const char *errors = NULL;
    char *buf;
    int status = BINARYPLIST_OK;

    encoder->ref_id_sz = ref_id_size(encoder->nobjects);
    /* write the magic header data */
    utstring_bincpy(encoder->output, BPLIST_MAGIC, BPLIST_MAGIC_SIZE);
    utstring_bincpy(encoder->output, BPLIST_VERSION, BPLIST_VERSION_SIZE);

    /* write the object list */
    offsets = malloc(sizeof(long) * objects->ob_size);
    for (i = 0; i < objects->ob_size; i++) {
        object = objects->ob_item[i];
        offsets[i] = utstring_len(encoder->output);
       if (binaryplist_data_type && PyObject_IsInstance(object, binaryplist_data_type) == 1) {
            PyString_AsStringAndSize(object, &buf, &len);
            write_int_header(encoder, 0x4, len);
            write_bytes(encoder, buf, len);
       } else if (binaryplist_uid_type && PyObject_IsInstance(object, binaryplist_uid_type) == 1) {
            lval = PyLong_AsLong(object);
            write_byte(encoder, 0x80 + offset_size(lval) - 1);
            write_multi_be(encoder, lval, offset_size(lval));
        } else if (object == Py_True) {
            write_byte(encoder, 0x09);
        } else if (object == Py_False) {
            write_byte(encoder, 0x08);
        } else if (object == Py_None) {
            if (encoder->convert_nulls == Py_True) {
                write_int_header(encoder, 0x5, 0);
            } else {
                write_byte(encoder, 0x00);
            }
        } else if (PyString_Check(object)) {
            PyString_AsStringAndSize(object, &buf, &len);
            write_int_header(encoder, 0x5, len);
            write_bytes(encoder, buf, len);
        } else if (PyUnicode_Check(object)) {
            /*
             * treat 0 len unicode as ascii. apple parser is weak like kitten.
             *
             */
            if (PyUnicode_GET_SIZE(object) == 0) {
                write_int_header(encoder, 0x5, 0);
            } else {
                if (encoder->as_ascii == Py_True && (tmp = PyUnicode_AsASCIIString(object))) {
                    PyString_AsStringAndSize(tmp, &buf, &len);
                    write_int_header(encoder, 0x5, len);
                    write_bytes(encoder, buf, len);
                } else {
                    PyErr_Clear();
                    tmp = PyUnicode_EncodeUTF16(PyUnicode_AS_UNICODE(object),
                                                PyUnicode_GET_SIZE(object),
                                                errors, 1 /* big endian */);
                    PyString_AsStringAndSize(tmp, &buf, &len);
                    /* nonsense, len here is characters not bytes */
                    write_int_header(encoder, 0x6, len/2);
                    write_bytes(encoder, buf, len);
                }
                Py_DECREF(tmp);
            }
        } else if (PyInt_Check(object) || PyLong_Check(object)) {
            write_int(encoder, PyLong_AsLong(object));
        } else if (PyDate_Check(object) || PyDateTime_Check(object)) {
            struct tm t = {
                PyDateTime_DATE_GET_SECOND(object),
                PyDateTime_DATE_GET_MINUTE(object),
                PyDateTime_DATE_GET_HOUR(object),
                PyDateTime_GET_DAY(object),
                PyDateTime_GET_MONTH(object)-1,
                PyDateTime_GET_YEAR(object)-1900,
                0,0,0
            };
            write_byte(encoder, 0x33);
            write_long(encoder, double_to_raw(mktime(&t) - APPLE_EPOCH_OFFSET));
        } else if (PyFloat_Check(object)) {
            write_byte(encoder, 0x23);
            write_long(encoder, double_to_raw(PyFloat_AS_DOUBLE(object)));
        } else if (PyList_Check(object)) {
            write_list(encoder, object);
        } else if (PyTuple_Check(object)) {
            write_tuple(encoder, object);
        } else if (PyDict_Check(object)) {
            write_dict(encoder, object);
        } else {
            status = BINARYPLIST_ERROR;
            break;
        }
        if (encoder->debug) {
            fprintf(stderr, "write_object(ref:%ld, len:%ld): ", get_reference_id(encoder, object),
                utstring_len(encoder->output) - offsets[i]);
            PyObject_Print(object, stderr, 0); 
            fprintf(stderr, "\n");
        }
    }

    /* write the offsets */
    off_pos = utstring_len(encoder->output);
    off_sz = offset_size(off_pos);
    for (i = 0; i < objects->ob_size; i++) {
        write_multi_be(encoder, offsets[i], off_sz);
    }
    if (encoder->debug) {
        fprintf(stderr, "ref_id_sz: %d off_sz: %d offset table: %ld length: %d\n", 
            encoder->ref_id_sz, off_sz,
            off_pos, off_sz*encoder->nobjects);
    }
    free(offsets);

    /* trailer */
    write_bytes(encoder, padding, 6);
    /* size of individual offset */
    write_byte(encoder, off_sz);
    /* size of individual ref */
    write_byte(encoder, encoder->ref_id_sz);
    /* number of objects */
    write_long(encoder, objects->ob_size);
    /* reference id of root object*/
    write_long(encoder, get_reference_id(encoder, encoder->root));
    /* byte position of offsets table */
    write_long(encoder, off_pos);

    return status;
}


/*
 * Routines for setting up encoder state.
 *
 */

static int encode_dict(binaryplist_encoder *encoder, PyObject *dict)
{
    int status = BINARYPLIST_OK;
    Py_ssize_t i = 0;
    PyObject *key, *value;
    PyDictObject *mp = (PyDictObject*) dict;

    i = Py_ReprEnter((PyObject *)mp);
    if (i != 0) {
        if (i > 0) {
            PyErr_SetString(PLIST_Error, 
                "a dict with references to itself is not encodable");
        }
        return BINARYPLIST_ERROR;
    }

    i = 0;
    while (PyDict_Next((PyObject *)mp, &i, &key, &value)) {
        if (encoder_encode_object(encoder, key) != BINARYPLIST_OK
            || encoder_encode_object(encoder, value) != BINARYPLIST_OK) {
            status = BINARYPLIST_ERROR;
            break;
        }
    }
    Py_ReprLeave((PyObject *)mp);
    return status;
}

static int encode_list(binaryplist_encoder *encoder, PyObject *list)
{
    int i;
    int status = BINARYPLIST_OK;
    PyListObject *v = (PyListObject*) list;

    i = Py_ReprEnter((PyObject*)v);
    if (i != 0) {
        if (i > 0) {
            PyErr_SetString(PLIST_Error, 
                "a dict with references to itself is not encodable");
        }
        return BINARYPLIST_ERROR;
    }

    for (i = 0; i < v->ob_size; ++i) {
        if (encoder_encode_object(encoder, v->ob_item[i]) != BINARYPLIST_OK) {
            status = BINARYPLIST_ERROR;
            break;
        }
    }
    Py_ReprLeave((PyObject *)v);
    return status;
}

static int encode_tuple(binaryplist_encoder *encoder, PyObject *tuple)
{
    int i;
    PyTupleObject *v = (PyTupleObject*) tuple;

    for (i = 0; i < v->ob_size; ++i) {
        if (encoder_encode_object(encoder, v->ob_item[i]) != BINARYPLIST_OK) {
            return BINARYPLIST_ERROR;
        }
    }
    return BINARYPLIST_OK;
}

static PyObject *get_unique(binaryplist_encoder *encoder, PyObject *object)
{
    PyObject *tmp = NULL;

    if (encoder->dounique && UNIQABLE(object)) {
        if (object == Py_None) {
            (encoder->uNone) ? (tmp = encoder->uNone) : (tmp = NULL);
        } else if (object == Py_True) {
            (encoder->uTrue) ? (tmp = encoder->uTrue) : (tmp = NULL);
        } else if (object == Py_False) {
            (encoder->uFalse) ? (tmp = encoder->uFalse) : (tmp = NULL);
        } else {
            tmp = PyDict_GetItem(encoder->uniques, object);
        }
    }
    return tmp;
}

static void set_ref(binaryplist_encoder *encoder, void *object, long id)
{
    PyObject *ol, *oid;

    ol = PyLong_FromVoidPtr(object);
    oid = PyLong_FromLong(id);
    PyDict_SetItem(encoder->ref_table, ol, oid);
    Py_DECREF(ol);
    Py_DECREF(oid);
}

int encoder_encode_object(binaryplist_encoder *encoder, PyObject *object)
{
    PyObject *ou, *tmp = NULL;
    int ret;

    if (!object) {
        PyErr_SetString(PLIST_Error, "object contains an unsupported type");
        return BINARYPLIST_ERROR;
    }

    /* Add supported data types below */
    if (!PyDict_Check(object) && !PyList_Check(object) && !PyTuple_Check(object)
        && object != Py_True && object != Py_False && object != Py_None
        && !PyString_Check(object) && !PyUnicode_Check(object)
        && !PyFloat_Check(object) && !PyInt_Check(object)
        && !PyDateTime_Check(object) && !PyDate_Check(object)
        && !PyLong_Check(object) ){

        if (encoder->object_hook != Py_None) {
            /*
             * total hack. add the crap object to the table to reference what will
             * hopefully be the right object returned by the hook.
             *
             */
            tmp = PyObject_CallFunctionObjArgs(encoder->object_hook, object, NULL);
            if (!tmp) {
                return BINARYPLIST_ERROR;
            }
            ou = get_unique(encoder, tmp);
            set_ref(encoder, (void *)object, (ou ? get_reference_id(encoder, ou) : encoder->nobjects));
            ret = encoder_encode_object(encoder, tmp);
            Py_DECREF(tmp);
            return ret;
        } else {
            PyErr_SetString(PLIST_Error, "object contains an unsupported type");
            return BINARYPLIST_ERROR;
        }
    }

    if (!encoder->root) {
        encoder->root = object;
    }
        

    if (encoder->dounique && UNIQABLE(object)) {
        if (object == Py_None) {
            (encoder->uNone) ? (tmp = encoder->uNone) : (encoder->uNone = object);
        } else if (object == Py_True) {
            (encoder->uTrue) ? (tmp = encoder->uTrue) : (encoder->uTrue = object);
        } else if (object == Py_False) {
            (encoder->uFalse) ? (tmp = encoder->uFalse) : (encoder->uFalse = object);
        } else {
            tmp = PyDict_GetItem(encoder->uniques, object);
            if (!tmp) {
                PyDict_SetItem(encoder->uniques, object, object);
            }
        }
        if (tmp) {
            /*
             * add object to the reference table pointing to its first
             * occurrence. do NOT append to object list since we already
             * exist.
             *
             */
            set_ref(encoder, (void *)object, get_reference_id(encoder, tmp));
            if (encoder->debug) {
                fprintf(stderr, "encode_object(UNIQ ref:%ld): ", get_reference_id(encoder, tmp));
                PyObject_Print(object, stderr, 0); 
                fprintf(stderr, "\n");
            }
            return BINARYPLIST_OK;
        }
    }

    if (++encoder->depth >= encoder->max_recursion) {
        PyErr_SetString(PLIST_Error, "object depth exceeded max_recursion");
        return BINARYPLIST_ERROR;
    }
    if (encoder->debug) {
        fprintf(stderr, "encode_object(ref:%d depth:%d): ", encoder->nobjects, encoder->depth);
        PyObject_Print(object, stderr, 0); 
        fprintf(stderr, "\n");
    }
    set_ref(encoder, (void *)object, encoder->nobjects++);
    PyList_Append(encoder->objects, object);

    if (PyDict_Check(object)) {
        return encode_dict(encoder, object);
    } else if (PyList_Check(object)) {
        return encode_list(encoder, object);
    } else if (PyTuple_Check(object)) {
        return encode_tuple(encoder, object);
    }

    encoder->depth--;
    return BINARYPLIST_OK;
}

void encoder_init()
{
    PyObject *tmp, *class, *module_name, *module, *module_dict;

    /*
     * Compiler voodoo ( static ), must be called from within this file.
     */
    PyDateTime_IMPORT;
    if (!binaryplist_uid_type || !binaryplist_data_type) {
        module_name = PyString_FromString("binaryplist");
        module = PyImport_Import(module_name);
        module_dict = PyModule_GetDict(module);

        class = PyDict_GetItemString(module_dict, "Uid");
        if (PyCallable_Check(class)) {
            tmp = PyObject_CallObject(class, NULL);
            binaryplist_uid_type = PyObject_Type(tmp);
            Py_DECREF(tmp);
        }
        class = PyDict_GetItemString(module_dict, "Data");
        if (PyCallable_Check(class)) {
            tmp = PyObject_CallObject(class, NULL);
            binaryplist_data_type = PyObject_Type(tmp);
            Py_DECREF(tmp);
        }
        Py_XDECREF(module_name);
        Py_XDECREF(module);
    }
    if (!PLIST_Error) {
        PLIST_Error = PyErr_NewException("binaryplist.Error", NULL, NULL);
    }   
}
