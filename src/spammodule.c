#include <Python.h>
#include <stdlib.h>
#include <bytesobject.h>

struct module_state {
    PyObject *error;
};

#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))

/**
 * This function says hello
 */
static PyObject *
say_hello(PyObject *self, PyObject *args)
{
    PyObject *name;
    PyObject *result;

    if (!PyArg_ParseTuple(args, "U:say_hello", &name)) {
        return NULL;
    }

    result = PyUnicode_FromFormat("Hello, %S!", name);
    return result;
}


static PyMethodDef spam_methods[] = {
    /* python_name,  method, *args, **kwargs? */
    {"say_hello", (PyCFunction) say_hello, METH_VARARGS, NULL},
    /* Why? */
    {NULL, NULL}
};


/* wtf is this? */
static int spam_traverse(PyObject *m, visitproc visit, void *arg)
{
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}


static int spam_clear(PyObject *m)
{
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}


/* Module definition */
static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "spam",                        /* module name */
    NULL,                          /* module docstring */
    sizeof(struct module_state),
    spam_methods,
    NULL,
    spam_traverse,
    spam_clear,
    NULL
};


PyObject *
PyInit_spam(void)
{
    PyObject *module = PyModule_Create(&moduledef);
    if (module == NULL) {
        return NULL;
    }

    struct module_state *st = PyModule_GetState(module);
    st->error = PyErr_NewException("spam.Error", NULL, NULL);
    if (st->error == NULL) {
        Py_DECREF(module);
        return NULL;
    }
    return module;
}
