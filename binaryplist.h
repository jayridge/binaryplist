#ifndef _BINARYPLIST_H
#define _BINARYPLIST_H

#include <Python.h>
#include <datetime.h>
#include "utstring.h"

#define BINARYPLIST_OK          0
#define BINARYPLIST_ERROR       1
#define APPLE_EPOCH_OFFSET      978307200

#define BPLIST_MAGIC            ((uint8_t*)"bplist")
#define BPLIST_MAGIC_SIZE       6

#define BPLIST_VERSION          ((uint8_t*)"00")
#define BPLIST_VERSION_SIZE     3

#define UNIQABLE(o) ( !PyCallable_Check(o) && ( o == Py_True || o == Py_False || o == Py_None \
    || PyString_Check(o) || PyUnicode_Check(o) || PyFloat_Check(o) || PyInt_Check(o) \
    || PyDateTime_Check(o) || PyDate_Check(o) || PyLong_Check(o) ))

enum
{
    BPLIST_NULL = 0x00,
    BPLIST_FALSE = 0x08,
    BPLIST_TRUE = 0x09,
    BPLIST_FILL = 0x0F,                 /* will be used for length grabbing */
    BPLIST_UINT = 0x1,
    BPLIST_REAL = 0x2,
    BPLIST_DATE = 0x3,
    BPLIST_DATA = 0x4,
    BPLIST_STRING = 0x5,
    BPLIST_UNICODE = 0x6,
    BPLIST_UID = 0x7,
    BPLIST_ARRAY = 0xA,
    BPLIST_SET = 0xC,
    BPLIST_DICT = 0xD,
    BPLIST_MASK = 0xF
};

typedef struct binaryplist_encoder {
    int nobjects;
    int dounique;
    int ref_id_sz;
    int max_recursion;
    int depth;
    int debug;
    /* Hack to treat uni as ascii where possible */
    PyObject *as_ascii;
    /* Hack to treat None as empty string */
    PyObject *convert_nulls;
    /* This should always be 0th object */
    PyObject *root;
    /* PyStrings are immutable, so we use a utstring instead */
    UT_string *output;
    /* Dict of obj pointer to reference id */
    PyObject *ref_table;
    /* PyList of flattened objects */
    PyObject *objects;
    /* 
     * PyDict of uniq objects.
     * Followed by a few special case types.
     */
    PyObject *uniques;
    PyObject *uNone;
    PyObject *uTrue;
    PyObject *uFalse;
    /* Callback function when type is unknown or unspported */
    PyObject *object_hook;
} binaryplist_encoder;

static PyObject *PLIST_Error = NULL;
static PyObject *binaryplist_uid_type = NULL;
static PyObject *binaryplist_data_type = NULL;

/* encode.c */
int encoder_encode_object(binaryplist_encoder *encoder, PyObject *object);
int encoder_write(binaryplist_encoder *encoder);
void encoder_init(void);

#endif

