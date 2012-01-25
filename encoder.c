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
    return PyLong_AsLong(PyDict_GetItem(encoder->ref_table, PyLong_FromVoidPtr((void *)obj)));
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
    long off_pos, *offsets;
    uint8_t padding[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    Py_ssize_t len;
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
        if (object == Py_True) {
            write_byte(encoder, BPLIST_TRUE);
        } else if (object == Py_False) {
            write_byte(encoder, BPLIST_FALSE);
        } else if (object == Py_None) {
            write_byte(encoder, 0x00);
        } else if (PyString_Check(object)) {
            PyString_AsStringAndSize(object, &buf, &len);
            write_int_header(encoder, 0x5, len);
            write_bytes(encoder, buf, len);
        } else if (PyUnicode_Check(object)) {
            tmp = PyUnicode_EncodeUTF16(PyUnicode_AS_UNICODE(object),
                                        PyUnicode_GET_SIZE(object),
                                        errors, 1 /* big endian */);
            PyString_AsStringAndSize(tmp, &buf, &len);
            /* nonsense, len here is characters not bytes */
            write_int_header(encoder, 0x6, len/2);
            write_bytes(encoder, buf, len);
            Py_DECREF(tmp);
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
            PyErr_SetString(PLIST_Error, "object is not encodable");
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
        //write_bytes(encoder, &offsets[i], off_sz);
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

int encoder_encode_object(binaryplist_encoder *encoder, PyObject *object)
{
    if (encoder->debug) {
        fprintf(stderr, "encode_object(ref:%d): ", encoder->nobjects);
        PyObject_Print(object, stderr, 0); 
        fprintf(stderr, "\n");
    }

    PyDict_SetItem(encoder->ref_table, PyLong_FromVoidPtr((void *)object), PyLong_FromLong(encoder->nobjects++));
    PyList_Append(encoder->objects, object);

    if (PyDict_Check(object)) {
        return encode_dict(encoder, object);
    } else if (PyList_Check(object)) {
        return encode_list(encoder, object);
    } else if (PyTuple_Check(object)) {
        return encode_tuple(encoder, object);

    /* Add supported data types that are not iterable below */
    } else if (object != Py_True && object != Py_False && object != Py_None
               && !PyString_Check(object) && !PyUnicode_Check(object)
               && !PyFloat_Check(object) && !PyInt_Check(object)
               && !PyDateTime_Check(object) && !PyDate_Check(object) && !PyTime_Check(object)
               && !PyLong_Check(object) ){
        PyErr_SetString(PLIST_Error, "object contains an unsupported type");
        return BINARYPLIST_ERROR;
    }
    return BINARYPLIST_OK;
}

void encoder_init()
{
    /*
     * Compiler voodoo, must be called from within this file.
     */
    PyDateTime_IMPORT;
}
