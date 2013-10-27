#include <Python.h>
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include <structmember.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>

#include "playback.h"

typedef struct {
    PyObject_HEAD
    PyObject *filepath;
    AVFormatContext *fmt_ctx;
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
    avformat_free_context(self->fmt_ctx);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
Song_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Song *self;

    self = (Song *)type->tp_alloc(type, 0);
    return (PyObject *)self;
}

/*PyDoc_STRVAR(song_init__doc__,*/
/*"Song(path) -> New Song object read from path.\n\*/
/*\n\*/
/*Initializes a new Song object containing metadata from the file at the given \*/
/*path. libav is used to read data from and write data to the file.");*/
PyDoc_STRVAR(Song_play__doc__, "Start or continue playing this song.");

static int
Song_init(Song *self, PyObject *args, PyObject *kwds)
{
    PyObject *obj;
    int ret;
    PyObject *tmp;

    if (!PyArg_ParseTuple(args, "U", &obj))
        return -1;

    PyObject *ascii_path = PyUnicode_AsASCIIString(obj);
    char *str = PyBytes_AsString(ascii_path);

    if (obj) {
        tmp = self->filepath;
        Py_INCREF(obj);
        self->filepath = obj;
        Py_XDECREF(tmp);
    }
    if((ret = avformat_open_input(&self->fmt_ctx, str, NULL, NULL)) < 0) {
        PyErr_SetString(PyExc_FileNotFoundError, "File not found");
        return -1;
    }

    return ret;
}

static PyObject *
Song_getfilepath(Song *self, void *closure)
{
    Py_INCREF(self->filepath);
    return self->filepath;
}

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
        Py_XDECREF(str_value);
    }
    return av_dict_set(&(self->fmt_ctx->metadata),
                       char_key, char_value, AV_DICT_IGNORE_SUFFIX);
    return 0;
}

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
    if (Playback_play(self->fmt_ctx) < 0) {
        PyErr_SetString(PyExc_IOError, "An error occurred in sdl");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
Song_save(Song *self)
{
/*    char *out_name = "out.flac";*/
/*    AVOutputFormat *ofmt = av_guess_format(self->fmt_ctx->iformat->name, out_name, NULL);*/
/*    if (!ofmt) {*/
/*        PyErr_SetString(PyExc_IOError, "Unable to detect output format.");*/
/*        return NULL;*/
/*    }*/
/*    AVFormatContext * output_format_context = avformat_alloc_context();*/
/*    if (!output_format_context) {*/
/*        PyErr_SetString(PyExc_IOError, "Unable to allocate output context");*/
/*        return NULL;*/
/*    }*/
/*    output_format_context->oformat = ofmt;*/
/*    if (!(ofmt->flags & AVFMT_NOFILE)) {*/
/*        if (avio_open(&self->fmt_ctx->pb, out_name, AVIO_FLAG_WRITE) < 0) {*/
/*            PyErr_SetString(PyExc_IOError, "Error 1");*/
/*            return NULL;*/
/*        }*/
/*    }*/
/*    unsigned int i = 0;*/
/*    for (; i < self->fmt_ctx->nb_streams; i++) {*/
/*        AVStream *in_stream = self->fmt_ctx->streams[i];*/
/*        AVStream *out_stream = avformat_new_stream(output_format_context, NULL);*/
/*        if (!out_stream) {*/
/*            PyErr_SetString(PyExc_IOError, "Error 2");*/
/*            return NULL;*/
/*        }*/
/*    }*/



/*    if(avformat_find_stream_info(self->fmt_ctx, NULL) < 0) {*/
/*        PyErr_SetString(PyExc_IOError, "Unable to find stream info");*/
/*        return NULL;*/
/*    }*/
/*    int i = 0;*/
/*    AVStream *audio_stream = NULL;*/
/*    for (; i < self->fmt_ctx->nb_streams; i++) {*/
/*        if (self->fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {*/
/*            audio_stream = self->fmt_ctx->streams[i];*/
/*            break;*/
/*        }*/
/*    }*/
    printf("Writing metadata\n");
/*    if (avformat_write_header(self->fmt_ctx, NULL) < 0) {*/
/*        PyErr_SetString(PyExc_IOError, "Could not write header.");*/
/*        return NULL;*/
/*    }*/



/*    printf("Done writing metadata\n");*/
/*    printf("Writing frames\n");*/
/*    AVPacket packet;*/
/*    while (av_read_frame(self->fmt_ctx, &packet) >= 0) {*/
/*        if (av_write_frame(self->fmt_ctx, &packet) < 0) {*/
/*            PyErr_SetString(PyExc_IOError, "Error writing stream.");*/
/*            return NULL;*/
/*        }*/
/*        printf("Writing\n");*/
/*    }*/
/*    printf("Done writing frames\n");*/
    Py_RETURN_NONE;
}

PyObject *
Song_iter(PyObject *self)
{
    Py_INCREF(self);
    return self;
}

PyObject *
Song_iternext(PyObject *self_as_obj)
{
    Song *self = (Song *)self_as_obj;
    self->current_tag = av_dict_get(self->fmt_ctx->metadata, "",
                                         self->current_tag,
                                         AV_DICT_IGNORE_SUFFIX);
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

int
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

/* Use the same hack as Pythons builtin dict */
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
    {"filepath", (getter)Song_getfilepath, NULL,
     "The path of the file.", NULL},
    {NULL}
};

static PyMethodDef Song_methods[] = {
    {"print", (PyCFunction)Song_print, METH_NOARGS, "Print all metadata."},
    {"save", (PyCFunction)Song_save, METH_NOARGS, ""},
    {"play", (PyCFunction)Song_play, METH_NOARGS, Song_play__doc__},
    {NULL}  /* Sentinel */
};

static PyTypeObject SongType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "audiotools.Song",         /* tp_name */
    sizeof(Song),              /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)Song_dealloc,  /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    &Song_as_sequence,         /* tp_as_sequence */
    &Song_as_mapping,          /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    (reprfunc)Song_str,        /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE |
        Py_TPFLAGS_HAVE_GC,    /* tp_flags */
    "Song objects",            /* tp_doc */
    (traverseproc)Song_traverse, /* tp_traverse */
    (inquiry)Song_clear,       /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    Song_iter,                 /* tp_iter */
    Song_iternext,             /* tp_iternext */
    Song_methods,              /* tp_methods */
    0,                         /* tp_members */
    Song_getseters,            /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Song_init,       /* tp_init */
    0,                         /* tp_alloc */
    Song_new,                  /* tp_new */
};

static PyModuleDef audiotoolsmodule = {
    PyModuleDef_HEAD_INIT,
    "audiotools",
    "This module serves as a wrapper around the libav C library.",
    -1,
    NULL, NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_audiotools(void)
{
    PyObject* module;
    av_register_all();

    if (PyType_Ready(&SongType) < 0) {
        return NULL;
    }

    module = PyModule_Create(&audiotoolsmodule);
    if (module == NULL) {
        return NULL;
    }

    Py_INCREF(&SongType);
    PyModule_AddObject(module, "Song", (PyObject *)&SongType);
    return module;
}
