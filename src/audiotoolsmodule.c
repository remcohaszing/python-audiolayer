#include <Python.h>
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include <structmember.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>

typedef struct {
    PyObject_HEAD
    PyObject *filepath;
    AVFormatContext *fmt_ctx;
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

/*define PyDoc_STRVAR(song_init__doc__,*/
/*"Song(path) -> New Song object read from path.\n\*/
/*\n\*/
/*Initializes a new Song object containing metadata from the file at the given \*/
/*path. libav is used to read data from and write data to the file.")*/
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
    PyObject *ascii_path = PyUnicode_AsASCIIString(key);
    char *str = PyBytes_AsString(ascii_path);

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
    PyObject *ascii_key = PyUnicode_AsASCIIString(key);
    char *char_key = PyBytes_AsString(ascii_key);
    char *char_value = NULL;
    if (value != NULL) {
        PyObject *str_value = PyObject_Str(value);
        PyObject *ascii_value = PyUnicode_AsASCIIString(str_value);
        char_value = PyBytes_AsString(ascii_value);
    }
    return av_dict_set(&(self->fmt_ctx->metadata),
                       char_key, char_value, AV_DICT_IGNORE_SUFFIX);
}

static PyObject *
Song_print(Song *self)
{
    AVDictionaryEntry *tag = NULL;
    while ((tag = av_dict_get(self->fmt_ctx->metadata,
                              "", tag, AV_DICT_IGNORE_SUFFIX))) {
        char *key = tag->key;
        int i = 0;
        for (; key[i]; i++) {
            key[i] = tolower(key[i]);
        }
        printf("%s -> %s\n", key, tag->value);
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
Song_play(Song *self)
{
    if(avformat_find_stream_info(self->fmt_ctx, NULL) < 0) {
        PyErr_SetString(PyExc_IOError, "Unable to find stream info");
        return NULL;
    }
    int i = 0;
    AVStream *audio_stream = NULL;
    for (; i < self->fmt_ctx->nb_streams; i++) {
        if (self->fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream = self->fmt_ctx->streams[i];
            break;
        }
    }
    if (audio_stream == NULL) {
        PyErr_SetString(PyExc_IOError, "No audio stream found");
        return NULL;
    }
    AVCodec *codec = avcodec_find_decoder(audio_stream->codec->codec_id);
    if (codec == NULL) {
        PyErr_SetString(PyExc_IOError, "No audio decoder found");
        return NULL;
    }
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (codec_ctx == NULL) {
        PyErr_SetString(PyExc_IOError, "No context found for codec");
        return NULL;
    }
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        PyErr_SetString(PyExc_IOError, "Unable to open codec context");
        return NULL;
    }
    AVPacket packet;
    if (av_read_frame(self->fmt_ctx, &packet) < 1) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    AVFrame *frame = avcodec_alloc_frame();
    int has_next = 1;
    i = 0;
    while (has_next != 0) {
        int processed = avcodec_decode_audio4(codec_ctx, frame,
                                              &has_next, &packet);
        if (processed < 0) {
            PyErr_SetString(PyExc_IOError,
                "Something went wrong processing the stream");
            return NULL;
        }
        printf(" has_next: %d\n", has_next);
        printf("processed: %d\n", processed);
        if (i++ > 1000) {
            break;
        }
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMappingMethods Song_as_mapping = {
    0,
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
    {"play", (PyCFunction)Song_play, METH_NOARGS,
     "Start or continue playing this song."},
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
    0,                         /* tp_as_sequence */
    &Song_as_mapping,          /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_BASETYPE |
        Py_TPFLAGS_HAVE_GC,    /* tp_flags */
    "Song objects",            /* tp_doc */
    (traverseproc)Song_traverse, /* tp_traverse */
    (inquiry)Song_clear,       /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
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

    if (PyType_Ready(&SongType) < 0)
        return NULL;

    module = PyModule_Create(&audiotoolsmodule);
    if (module == NULL)
        return NULL;

    Py_INCREF(&SongType);
    PyModule_AddObject(module, "Song", (PyObject *)&SongType);
    return module;
}
