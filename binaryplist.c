#include "binaryplist.h"

#define BINARYPLIST_CMD_STR \
"class Uid(int):__module__='binaryplist'\n" \
"class Data(str):__module__='binaryplist'\n"

/*
 * Python command initialization and callbacks.
 *
 */

static PyObject* binaryplist_encode(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"obj", "unique", "debug", "convert_nulls",
                             "max_recursion", "object_hook", "as_ascii", NULL};
    PyObject *newobj = NULL;
    PyObject *oinput = NULL;
    PyObject *ounique = NULL;
    PyObject *odebug = NULL;
    PyObject *orecursion = NULL;
    binaryplist_encoder encoder;
    
    memset(&encoder, 0, sizeof(binaryplist_encoder));
    encoder.convert_nulls = Py_False;
    encoder.as_ascii = Py_False;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|OOOOOO", kwlist, &oinput, &ounique,
        &odebug, &(encoder.convert_nulls), &orecursion, &(encoder.object_hook),
        &(encoder.as_ascii))) {   
        return NULL;
    }
    if (encoder.object_hook && !PyCallable_Check(encoder.object_hook)) {
        PyErr_SetString(PLIST_Error, "object_hook is not callable");
        return NULL;
    }
    encoder.ref_table = PyDict_New();
    encoder.objects = PyList_New(0);
    utstring_new(encoder.output);
    if (!ounique || (ounique && PyObject_IsTrue(ounique))) {
        /* default to True */
        encoder.dounique = 1;
        encoder.uniques = PyDict_New();
    }
    if (odebug && PyObject_IsTrue(odebug)) {
        encoder.debug = 1;
    }
    if (orecursion && PyInt_Check(orecursion)) {
        encoder.max_recursion = PyInt_AsLong(orecursion);
    } else {
        encoder.max_recursion = 1024*16;
    }

    if (encoder_encode_object(&encoder, oinput) == BINARYPLIST_OK
        && encoder_write(&encoder) == BINARYPLIST_OK) {
        newobj = PyString_FromStringAndSize(utstring_body(encoder.output),
            utstring_len(encoder.output));
    }

    Py_DECREF(encoder.ref_table);
    Py_DECREF(encoder.objects);
    Py_XDECREF(encoder.uniques);
    utstring_free(encoder.output);

    return newobj;
}

static PyMethodDef binaryplist_methods[] =
{
    {"encode", (PyCFunction)binaryplist_encode, METH_VARARGS | METH_KEYWORDS,
     "Generate the binary plist representation of an object."},
    {NULL, NULL, 0, NULL}
};
 

PyDoc_STRVAR(module_doc, "OS X compatible binary plist encoder/decoder.");

PyMODINIT_FUNC
initlibbinaryplist(void)
{
    PyObject *module = Py_InitModule3("libbinaryplist", binaryplist_methods, module_doc);
    encoder_init();
}
