#include "binaryplist.h"

/*
 * Python command initialization and callbacks.
 *
 */

static PyObject* binaryplist_encode(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"obj", "unique", "debug", "object_hook",  NULL};
    PyObject *newobj;
    PyObject *oinput = NULL;
    PyObject *ounique = NULL;
    PyObject *odebug = NULL;
    PyObject *ohook = NULL;
    binaryplist_encoder encoder;
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|OOO", kwlist, &oinput, &ounique,
        &odebug, &ohook)) {   
        return NULL;
    }   

    memset(&encoder, 0, sizeof(binaryplist_encoder));
    encoder.ref_table = PyDict_New();
    encoder.uniques = PyDict_New();
    encoder.objects = PyList_New(0);
    utstring_new(encoder.output);
    if (!ounique || (ounique && PyObject_IsTrue(ounique))) {
        /* default to True */
        encoder.dounique = 1;
    }
    if (odebug && PyObject_IsTrue(odebug)) {
        encoder.debug = 1;
    }
    encoder.object_hook = ohook;
    encoder_encode_object(&encoder, oinput);
    encoder_write(&encoder);
    newobj = PyString_FromStringAndSize(utstring_body(encoder.output),
        utstring_len(encoder.output));
    Py_DECREF(encoder.ref_table);
    Py_DECREF(encoder.uniques);
    Py_DECREF(encoder.objects);
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
initbinaryplist(void)
{
    PyObject *m;

    m = Py_InitModule3("binaryplist", binaryplist_methods, module_doc);
    encoder_init();
    if (PLIST_Error == NULL) {
        PLIST_Error = PyErr_NewException("binaryplist.Error", NULL, NULL);
        if (PLIST_Error) {
            PyModule_AddObject(m, "Error", PLIST_Error);
        }
    }
}
