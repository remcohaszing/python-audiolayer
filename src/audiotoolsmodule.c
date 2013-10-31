#include <Python.h>
#include <stdio.h>
#include <strings.h>
#include <structmember.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "playback.h"

/**
 * Exception definitions.
 */
static PyObject *NoMediaException;

/**
 * Definitions for the Song class.
 */
typedef struct {
    PyObject_HEAD
    PyObject *filepath;
    AVFormatContext *fmt_ctx;
    AVStream *audio_stream;
    AVCodecContext *codec_ctx;
    AVDictionaryEntry *current_tag; /* Used for iteration: for tag in song */
} Song;

/* Required for cyclic garbage collection */
static int
Song_traverse(Song *self, visitproc visit, void *arg)
{
    Py_VISIT(self->filepath);
    return 0;
}

static int
Song_clear(Song *self)
{
    Py_CLEAR(self->filepath);
    return 0;
}

static void
Song_dealloc(Song* self)
{
    Song_clear(self);
    if (self->codec_ctx != NULL) {
        avcodec_close(self->codec_ctx);
    }
    if (self->fmt_ctx != NULL) {
        avformat_free_context(self->fmt_ctx);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
Song_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Song *self;

    self = (Song *)type->tp_alloc(type, 0);
    return (PyObject *)self;
}

/* Song objecty docstring */
PyDoc_STRVAR(Song_doc, "This class represents an audio file.\n\
\n\
This class loads an audio file and reads its metadata and stream info. The \
metadata can be read from a Song object using subscript. Example using the \
test file provided in the test directory:\n\
\n\
>>> song = Song('test.flac')\n\
>>> song['artist']\n\
Machinae Supremacy\n\
>>>");
/* Method docstrings */
PyDoc_STRVAR(Song_play__doc__, "Start or continue playing this song.");
/* Property docstrings */
PyDoc_STRVAR(Song_filepath__doc__, "The path of the file.");
PyDoc_STRVAR(Song_duration__doc__, "The duration of the file in seconds.");
PyDoc_STRVAR(Song_samplerate__doc__, "The sample rate of the file.");
PyDoc_STRVAR(Song_channels__doc__, "The number of audio channels of the file");

static int
Song_init(Song *self, PyObject *args, PyObject *kwds)
{
    PyObject *obj;
    PyObject *tmp;

    if (!PyArg_ParseTuple(args, "U", &obj)) {
        return -1;
    }

    char *str = PyUnicode_AsUTF8(obj);

    if (obj) {
        tmp = self->filepath;
        Py_INCREF(obj);
        self->filepath = obj;
        Py_XDECREF(tmp);
    }
    int result = avformat_open_input(&self->fmt_ctx, str, NULL, NULL);
    switch (result) {
        case -1:
            PyErr_SetFromErrnoWithFilename(PyExc_IsADirectoryError, str);
            return -1;
        case -2:
            PyErr_SetFromErrnoWithFilename(PyExc_FileNotFoundError, str);
            return -1;
        case -1094995529:
            PyErr_SetFromErrnoWithFilename(NoMediaException, str);
            return -1;
    }
    if (result < 0) {
        PyErr_SetString(PyExc_RuntimeError,
                        "An unknown exception has occurred.");
        return -1;
    }
    /* This is 20 times the default, is this ok? */
    self->fmt_ctx->max_analyze_duration = 100000000;

    /* This is required for formats with no header info. */
    if (avformat_find_stream_info(self->fmt_ctx, NULL) < 0) {
        PyErr_SetString(PyExc_IOError, "Cannot find stream info.");
        return -1;
    }
    unsigned int i = 0;
    for (; i < self->fmt_ctx->nb_streams; i++) {
        if (self->fmt_ctx->streams[i]->codec->codec_type ==
                AVMEDIA_TYPE_AUDIO) {
            self->audio_stream = self->fmt_ctx->streams[i];
            break;
        }
    }
    if (self->audio_stream == NULL) {
        PyErr_SetString(PyExc_IOError, "Cannot find audio stream.");
        return -1;
    }
    self->codec_ctx = self->audio_stream->codec;
    return 0;
}

/**
 * Property getters.
 */
static PyObject *
Song_getfilepath(Song *self, void *closure)
{
    Py_INCREF(self->filepath);
    return self->filepath;
}

static PyObject *
Song_getduration(Song *self, void *closure)
{
    /*
     * The duration is stored in nanoseconds.
     * Return as a python float in seconds for usability.
     */
    return PyFloat_FromDouble((double)self->fmt_ctx->duration / AV_TIME_BASE);
}

static PyObject *
Song_getsamplerate(Song *self, void *closure)
{
    return PyLong_FromLong(self->codec_ctx->sample_rate);
}

static PyObject *
Song_getchannels(Song *self, void *closure)
{
    return PyLong_FromLong(self->codec_ctx->channels);
}

/**
 * Subscript functions.
 */
static PyObject *
Song_getitem(Song *self, PyObject *key)
{
    char *str = PyUnicode_AsUTF8(key);
    AVDictionaryEntry *tag = NULL;
    while ((tag = av_dict_get(self->fmt_ctx->metadata,
                              "", tag, AV_DICT_IGNORE_SUFFIX))) {
        if (strcasecmp(str, tag->key) == 0) {
            return PyUnicode_FromString(tag->value);
        }
    }
    PyErr_SetString(PyExc_KeyError, "Metadata not found");
    return NULL;
}

static int
Song_setitem(Song *self, PyObject *key, PyObject *value)
{
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(PyExc_TypeError, "Key must be a string");
        return -1;
    }
    char *char_key = PyUnicode_AsUTF8(key);
    char *char_value = NULL;
    if (value != NULL) {
        PyObject *str_value = PyObject_Str(value);
        char_value = PyUnicode_AsUTF8(str_value);
    }
    return av_dict_set(&(self->fmt_ctx->metadata),
                       char_key, char_value, AV_DICT_IGNORE_SUFFIX);
    return 0;
}

/**
 * Methods.
 */
static PyObject *
Song_print(Song *self)
{
    AVDictionaryEntry *tag = NULL;
    while ((tag = av_dict_get(self->fmt_ctx->metadata,
                              "", tag, AV_DICT_IGNORE_SUFFIX))) {
        char *key = tag->key;
        unsigned int i = 0;
        for (; key[i]; i++) {
            key[i] = tolower(key[i]);
        }
        printf("%s -> %s\n", key, tag->value);
    }
    Py_RETURN_NONE;
}

static PyObject *
Song_play(Song *self)
{
    if (Playback_play(self->fmt_ctx, self->codec_ctx, self->audio_stream) < 0) {
        PyErr_SetString(PyExc_IOError, "An error occurred");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
Song_save(Song *self)
{
    Py_RETURN_NONE;
}

/**
 * Iteration and sequence functions.
 */
static PyObject *
Song_iter(PyObject *self)
{
    Py_INCREF(self);
    return self;
}

static PyObject *
Song_iternext(PyObject *self_as_obj)
{
    Song *self = (Song *)self_as_obj;
    self->current_tag = av_dict_get(self->fmt_ctx->metadata, "",
                                    self->current_tag, AV_DICT_IGNORE_SUFFIX);
    if (self->current_tag == NULL) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }
    char *lower_tag = self->current_tag->key;
    unsigned int i = 0;
    for (; lower_tag[i]; i++) {
        lower_tag[i] = tolower(lower_tag[i]);
    }
    return PyUnicode_FromString(lower_tag);
}

static Py_ssize_t
Song_len(Song *self)
{
    unsigned int len = 0;
    AVDictionaryEntry *tag = NULL;
    while ((tag = av_dict_get(self->fmt_ctx->metadata,
                              "", tag, AV_DICT_IGNORE_SUFFIX))) {
        len++;
    }
    return (Py_ssize_t)len;
}

static int
Song_contains(PyObject *self_as_obj, PyObject *key)
{
    Song *self = (Song *)self_as_obj;

    char *str = PyUnicode_AsUTF8(key);
    AVDictionaryEntry *tag = NULL;
    while ((tag = av_dict_get(self->fmt_ctx->metadata,
                              "", tag, AV_DICT_IGNORE_SUFFIX))) {
        if (strcasecmp(str, tag->key) == 0) {
            return 1;
        }
    }
    return 0;
}

static PyObject *
Song_str(Song *self)
{
    char *ret = "audiotools.Song(";
    AVDictionaryEntry *tag = NULL;
    AVDictionaryEntry *next = av_dict_get(self->fmt_ctx->metadata, "", NULL,
                                          AV_DICT_IGNORE_SUFFIX);
    while ((next = av_dict_get(self->fmt_ctx->metadata,
                              "", tag, AV_DICT_IGNORE_SUFFIX))) {
        char *prefix;
        if (tag == NULL) {
            prefix = "";
        } else {
            prefix = ", ";
        }
        char *key = next->key;
        char *value_start = "='";
        char *value_end = "'";
        unsigned int i = 0;
        for (; key[i]; i++) {
            key[i] = tolower(key[i]);
        }
        char *tmp = malloc(strlen(ret) + strlen(key) + strlen(next->value) +
                           strlen(prefix) + strlen(value_start) +
                           strlen(value_end) + 1);
        strcpy(tmp, ret);
        strcat(tmp, prefix);
        strcat(tmp, key);
        strcat(tmp, value_start);
        strcat(tmp, next->value);
        strcat(tmp, value_end);
        ret = tmp;
        tag = next;
    }
    char *tmp = malloc(strlen(ret) + 2);
    strcpy(tmp, ret);
    strcat(tmp, ")");
    return PyUnicode_FromString(tmp);
}

/**
 * Song mappings.
 */
/* Use the same hack as Pythons builtin dict to support: tag in song */
static PySequenceMethods Song_as_sequence = {
    0,                          /* sq_length */
    0,                          /* sq_concat */
    0,                          /* sq_repeat */
    0,                          /* sq_item */
    0,                          /* sq_slice */
    0,                          /* sq_ass_item */
    0,                          /* sq_ass_slice */
    Song_contains,              /* sq_contains */
    0,                          /* sq_inplace_concat */
    0,                          /* sq_inplace_repeat */
};

static PyMappingMethods Song_as_mapping = {
    (lenfunc)Song_len,
    (binaryfunc)Song_getitem,
    (objobjargproc)Song_setitem
};

static PyGetSetDef Song_getseters[] = {
    {"filepath", (getter)Song_getfilepath, NULL, Song_filepath__doc__, NULL},
    {"duration", (getter)Song_getduration, NULL, Song_duration__doc__, NULL},
    {"sample_rate", (getter)Song_getsamplerate, NULL,
     Song_samplerate__doc__, NULL},
    {"channels", (getter)Song_getchannels, NULL, Song_channels__doc__, NULL},
    {NULL}
};

static PyMethodDef Song_methods[] = {
    {"print", (PyCFunction)Song_print, METH_NOARGS, "Print all metadata."},
    {"save", (PyCFunction)Song_save, METH_NOARGS, ""},
    {"play", (PyCFunction)Song_play, METH_NOARGS, Song_play__doc__},
    {NULL}
};

static PyTypeObject SongType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "audiotools.Song",           /* tp_name */
    sizeof(Song),                /* tp_basicsize */
    0,                           /* tp_itemsize */
    (destructor)Song_dealloc,    /* tp_dealloc */
    0,                           /* tp_print */
    0,                           /* tp_getattr */
    0,                           /* tp_setattr */
    0,                           /* tp_reserved */
    0,                           /* tp_repr */
    0,                           /* tp_as_number */
    &Song_as_sequence,           /* tp_as_sequence */
    &Song_as_mapping,            /* tp_as_mapping */
    0,                           /* tp_hash  */
    0,                           /* tp_call */
    (reprfunc)Song_str,          /* tp_str */
    0,                           /* tp_getattro */
    0,                           /* tp_setattro */
    0,                           /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE |
        Py_TPFLAGS_HAVE_GC,      /* tp_flags */
    Song_doc,                    /* tp_doc */
    (traverseproc)Song_traverse, /* tp_traverse */
    (inquiry)Song_clear,         /* tp_clear */
    0,                           /* tp_richcompare */
    0,                           /* tp_weaklistoffset */
    Song_iter,                   /* tp_iter */
    Song_iternext,               /* tp_iternext */
    Song_methods,                /* tp_methods */
    0,                           /* tp_members */
    Song_getseters,              /* tp_getset */
    0,                           /* tp_base */
    0,                           /* tp_dict */
    0,                           /* tp_descr_get */
    0,                           /* tp_descr_set */
    0,                           /* tp_dictoffset */
    (initproc)Song_init,         /* tp_init */
    0,                           /* tp_alloc */
    Song_new,                    /* tp_new */
};

/**
 * Definitions for the audiotools module.
 */
PyDoc_STRVAR(audiotools__doc__, "This module contains the Song object.");

static void
audiotools_free(void *unused)
{
    Playback_free();
}

static PyModuleDef audiotoolsmodule = {
    PyModuleDef_HEAD_INIT,      /* m_base */
    "audiotools",               /* m_name */
    audiotools__doc__,          /* m_doc */
    -1,                         /* m_size */
    NULL,                       /* m_methods */
    NULL,                       /* m_reload */
    NULL,                       /* m_traverse */
    NULL,                       /* m_clear */
    (freefunc)audiotools_free   /* m_free */
};

PyMODINIT_FUNC
PyInit_audiotools(void)
{
    PyObject* module;
    av_register_all();
    Playback_init();

    if (PyType_Ready(&SongType) < 0) {
        return NULL;
    }

    module = PyModule_Create(&audiotoolsmodule);
    if (module == NULL) {
        return NULL;
    }

    NoMediaException = PyErr_NewException("audiotools.NoMediaException",
                                          NULL, NULL);
    Py_INCREF(NoMediaException);
    PyModule_AddObject(module, "NoMediaException", NoMediaException);
    Py_INCREF(&SongType);
    PyModule_AddObject(module, "Song", (PyObject *)&SongType);
    return module;
}
