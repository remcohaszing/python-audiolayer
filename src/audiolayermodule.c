#include <fcntl.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libgen.h>
#include <portaudio.h>
#include <Python.h>
#include <stdio.h>
#include <strings.h>
#include <structmember.h>
#include <time.h>

/**
 * Exception definitions.
 */
static PyObject *NoMediaException;

/**
 * Definitions for the Song class.
 */
typedef struct {
    /* Python */
    PyObject_HEAD
    PyObject *filepath;
    PyObject *duration;
    PyObject *sample_rate;
    PyObject *channels;
    /* av */
    AVFormatContext *fmt_ctx;
    AVStream *audio_stream;
    AVCodecContext *codec_ctx;
    AVDictionaryEntry *current_tag; /* Used for iteration: for tag in song */
    /* portaudio */
    PaStream *pa_stream;
} Song;

/* Required for cyclic garbage collection */
static int
Song_traverse(Song *self, visitproc visit, void *arg)
{
    Py_VISIT(self->filepath);
    Py_VISIT(self->duration);
    Py_VISIT(self->sample_rate);
    Py_VISIT(self->channels);
    return 0;
}

static int
Song_clear(Song *self)
{
    Py_CLEAR(self->filepath);
    Py_CLEAR(self->duration);
    Py_CLEAR(self->sample_rate);
    Py_CLEAR(self->channels);
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
PyDoc_STRVAR(Song_save__doc__, "Save the song with its metadata.\n\
\n\
This saves the song to a file with the newly set metadata.\n\
\n\
:key filename: The path to save the new file to.");
PyDoc_STRVAR(Song_print__doc__, "Prints all metadata of this song.\n\
\n\
The metadata will be printed in the form `key -> value\\n`. This is a \
shorthand for:\n\
\n\
>>> for tag in song:\n\
...     print('{} -> {}'.format(tag, song[tag]))\n\
...");
/* Property docstrings */
PyDoc_STRVAR(Song_filepath__doc__, "The path of the file.");
PyDoc_STRVAR(Song_duration__doc__, "The duration of the file in seconds.");
PyDoc_STRVAR(Song_samplerate__doc__, "The sample rate of the file.");
PyDoc_STRVAR(Song_channels__doc__, "The number of audio channels of the file.");

static int
Song_init(Song *self, PyObject *args, PyObject *kwds)
{
    /* Song has already been initialized */
    if (self->filepath != NULL) {
        PyErr_SetString(PyExc_UserWarning,
                        "This song object has already been initialized.");
        return -1;
    }
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
    self->duration = PyFloat_FromDouble((double)self->fmt_ctx->duration /
                                         AV_TIME_BASE);
    Py_INCREF(self->duration);
    self->sample_rate = PyLong_FromLong(self->codec_ctx->sample_rate);
    Py_INCREF(self->sample_rate);
    self->channels = PyLong_FromLong(self->codec_ctx->channels);
    Py_INCREF(self->channels);
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
    Py_INCREF(self->duration);
    return self->duration;
}

static PyObject *
Song_getsamplerate(Song *self, void *closure)
{
    Py_INCREF(self->sample_rate);
    return self->sample_rate;
}

static PyObject *
Song_getchannels(Song *self, void *closure)
{
    Py_INCREF(self->channels);
    return self->channels;
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

#define PaPy_CHECK_ERROR(error) \
    if (error != paNoError) { \
        PyErr_SetString(PyExc_OSError, Pa_GetErrorText(error)); \
        return NULL; \
    }

static PyObject *
Song_play(Song *self)
{
    AVCodec *codec = avcodec_find_decoder(self->audio_stream->codec->codec_id);
    if (codec == NULL) {
        return NULL;
    }
    if (avcodec_find_decoder(self->codec_ctx->codec_id) < 0) {
        return NULL;
    }
    if (avcodec_open2(self->codec_ctx, codec, NULL) < 0) {
        return NULL;
    }

    PaSampleFormat sample_fmt;
    switch (self->codec_ctx->sample_fmt) {
        case AV_SAMPLE_FMT_U8:
            sample_fmt = paUInt8;
            break;
        case AV_SAMPLE_FMT_S16:
            sample_fmt = paInt16;
            break;
        case AV_SAMPLE_FMT_S32:
            sample_fmt = paInt32;
            break;
        case AV_SAMPLE_FMT_FLT:
            sample_fmt = paFloat32;
            break;
        default:
            PyErr_SetString(PyExc_OSError,
                            "Unable to parse audio sample format.");
            return NULL;
    }
    PaError err = Pa_OpenDefaultStream(&self->pa_stream,
                                       0,
                                       self->codec_ctx->channels,
                                       sample_fmt,
                                       self->codec_ctx->sample_rate,
                                       paFramesPerBufferUnspecified,
                                       NULL,
                                       NULL);
    PaPy_CHECK_ERROR(err)
    err = Pa_StartStream(self->pa_stream);
    PaPy_CHECK_ERROR(err)
    AVPacket packet;
    while (av_read_frame(self->fmt_ctx, &packet) >= 0) {
        if (packet.stream_index != self->audio_stream->index) {
            continue;
        }
        AVFrame frame;
        int got_frame;
        int ret = avcodec_decode_audio4(self->codec_ctx, &frame,
                                        &got_frame, &packet);
        if (ret < 0) {
            continue;
        }
        if (ret != packet.size) {
            continue;
        }
        if (got_frame) {
            err = Pa_WriteStream(self->pa_stream, *frame.data,
                                 frame.nb_samples);
            PaPy_CHECK_ERROR(err)
        }
        av_free_packet(&packet);
    }
    av_seek_frame(self->fmt_ctx, self->audio_stream->index, 0, 0);
    Py_RETURN_NONE;
}

static PyObject *
Song_save(Song *self, PyObject *args, PyObject *kwargs)
{
    char *filename;
    PyObject *py_filename = NULL;

    static char *kwds[] = {"filename", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|U", kwds, &py_filename)) {
        return NULL;
    }

    /* Create a random tmp filename to store the unfinished file. */
    static const char choice[] = "0123456789"
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "abcdefghijklmnopqrstuvwxyz";
    char *tmpfile = malloc(21);
    unsigned int i = 1;
    tmpfile[0] = '.';
    for (; i < 20; i++) {
        tmpfile[i] = choice[rand() % (sizeof(choice) - 1)];
    }
    tmpfile[i] = 0;

    if (py_filename) {
        filename = PyUnicode_AsUTF8(py_filename);
    } else {
        filename = self->fmt_ctx->filename;
    }
    struct stat s;
    char *dir = dirname(filename);
    if(stat(dir, &s)) {
        PyErr_SetFromErrnoWithFilename(PyExc_IOError, dir);
        return NULL;
    }

    AVOutputFormat *o_fmt = av_guess_format(self->fmt_ctx->iformat->name,
                                            filename, NULL);
    if (!o_fmt) {
        PyErr_SetString(PyExc_IOError, "Unable to detect output format.");
        return NULL;
    }
    AVFormatContext *o_fmt_ctx = avformat_alloc_context();
    o_fmt_ctx->oformat = o_fmt;
    if (!o_fmt_ctx) {
        PyErr_SetString(PyExc_IOError,
                        "Unable to allocate output format context.");
        return NULL;
    }
    if (!(o_fmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&(o_fmt_ctx->pb), tmpfile, AVIO_FLAG_WRITE) < 0) {
            PyErr_SetString(PyExc_IOError,
                            "Unable to open temporary output file.");
            return NULL;
        }
    }
    AVStream *o_stream = avformat_new_stream(o_fmt_ctx, NULL);
    if (!o_stream) {
        PyErr_SetString(PyExc_IOError, "Unable to allocate output stream.");
        return NULL;
    }
    o_stream->id = self->audio_stream->id;
    o_stream->disposition = self->audio_stream->disposition;
    o_stream->codec->bits_per_raw_sample = self->audio_stream->codec->bits_per_raw_sample;
    o_stream->codec->chroma_sample_location = self->audio_stream->codec->chroma_sample_location;
    o_stream->codec->codec_id = self->audio_stream->codec->codec_id;
    o_stream->codec->codec_type = self->audio_stream->codec->codec_type;
    o_stream->codec->codec_tag = self->audio_stream->codec->codec_tag;
    o_stream->codec->bit_rate = self->audio_stream->codec->bit_rate;
    o_stream->codec->rc_max_rate = self->audio_stream->codec->rc_max_rate;
    o_stream->codec->rc_buffer_size = self->audio_stream->codec->rc_buffer_size;
    o_stream->codec->field_order = self->audio_stream->codec->field_order;
    uint64_t extra_size = (uint64_t)self->audio_stream->codec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE;
    if (extra_size > INT_MAX) {
        return NULL;
    }
    o_stream->codec->extradata = av_mallocz(extra_size);
    if (!o_stream->codec->extradata) {
        return NULL;
    }
    memcpy(o_stream->codec->extradata, self->audio_stream->codec->extradata, self->audio_stream->codec->extradata_size);
    o_stream->codec->extradata_size = self->audio_stream->codec->extradata_size;
    o_stream->codec->time_base = self->audio_stream->time_base;

    /* Audio specific */
    o_stream->codec->channel_layout = self->audio_stream->codec->channel_layout;
    o_stream->codec->sample_rate = self->audio_stream->codec->sample_rate;
    o_stream->codec->channels = self->audio_stream->codec->channels;
    o_stream->codec->frame_size = self->audio_stream->codec->frame_size;
    o_stream->codec->audio_service_type = self->audio_stream->codec->audio_service_type;
    o_stream->codec->block_align = self->audio_stream->codec->block_align;

    /* Metadata */
    AVDictionaryEntry *tag = NULL;
    while((tag = av_dict_get(self->fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        av_dict_set(&o_fmt_ctx->metadata, tag->key, tag->value, AV_DICT_IGNORE_SUFFIX);
    }

    if (avformat_write_header(o_fmt_ctx, NULL) < 0) {
        PyErr_SetString(PyExc_IOError, "Unable to write metadata.");
        return NULL;
    }

    AVPacket packet;
    while (av_read_frame(self->fmt_ctx, &packet) >= 0) {
        if (packet.stream_index != self->audio_stream->index) {
            continue;
        }
        av_write_frame(o_fmt_ctx, &packet);
        av_free_packet(&packet);
    }

    av_seek_frame(self->fmt_ctx, self->audio_stream->index, 0, 0);

    if (av_write_trailer(o_fmt_ctx) < 0) {
        PyErr_SetString(PyExc_IOError, "Error writing trailer info.");
        return NULL;
    }

    avio_close(o_fmt_ctx->pb);
    avformat_free_context(o_fmt_ctx);
    rename(tmpfile, filename);

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
    char *ret = "audiolayer.Song(";
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
    {"print", (PyCFunction)Song_print, METH_NOARGS, Song_print__doc__},
    {"save", (PyCFunction)Song_save, METH_VARARGS | METH_KEYWORDS,
     Song_save__doc__},
    {"play", (PyCFunction)Song_play, METH_NOARGS, Song_play__doc__},
    {NULL}
};

static PyTypeObject SongType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "audiolayer.Song",           /* tp_name */
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
 * Definitions for the audiolayer module.
 */
PyDoc_STRVAR(audiolayer__doc__, "This module contains the Song object.");

static void
audiolayer_free(void *unused)
{
    Pa_Terminate();
}

static PyModuleDef audiolayermodule = {
    PyModuleDef_HEAD_INIT,      /* m_base */
    "audiolayer",               /* m_name */
    audiolayer__doc__,          /* m_doc */
    -1,                         /* m_size */
    NULL,                       /* m_methods */
    NULL,                       /* m_reload */
    NULL,                       /* m_traverse */
    NULL,                       /* m_clear */
    (freefunc)audiolayer_free   /* m_free */
};

PyMODINIT_FUNC
PyInit_audiolayer(void)
{
    srand(time(NULL));
    PyObject* module;
    av_register_all();

    /* Hack to disable annoying Portaudio logging when importing this module */
    fflush(stderr);
    int bak = dup(2);
    int new = open("/dev/null", O_WRONLY);
    dup2(new, 2);
    close(new);
    Pa_Initialize();
    fflush(stderr);
    dup2(bak, 2);
    close(bak);

    if (PyType_Ready(&SongType) < 0) {
        return NULL;
    }

    module = PyModule_Create(&audiolayermodule);
    if (module == NULL) {
        return NULL;
    }

    NoMediaException = PyErr_NewException("audiolayer.NoMediaException",
                                          NULL, NULL);
    Py_INCREF(NoMediaException);
    PyModule_AddObject(module, "NoMediaException", NoMediaException);
    Py_INCREF(&SongType);
    PyModule_AddObject(module, "Song", (PyObject *)&SongType);
    return module;
}
