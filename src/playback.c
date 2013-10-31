#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <portaudio.h>

int cyclic_buffer[1024 * 1024 * 4];

static int pa_stream_callback(const void *input, void *output,
                              unsigned long frameCount,
                              const PaStreamCallbackTimeInfo* timeInfo,
                              PaStreamCallbackFlags statusFlags,
                              void *user_data)
{
/*    AVCodecContext *codec_ctx = (AVCodecContext *)user_data;*/
    float *out = (float *)output;
    unsigned int i = 0;
    for (; i < frameCount; i++) {
        /* Do something */
        *out++ = 2.0;
    }
    return 0;
}

int
Playback_play(AVFormatContext *fmt_ctx, AVCodecContext *codec_ctx, AVStream *stream)
{
    AVCodec *codec = avcodec_find_decoder(stream->codec->codec_id);
    if (codec == NULL) {
        return -1;
    }
    if (avcodec_find_decoder(codec_ctx->codec_id) < 0) {
        return -1;
    }
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        return -1;
    }

    PaStream *pa_stream;
    PaError err;
    err = Pa_OpenDefaultStream(
            &pa_stream,
            0,
            codec_ctx->channels,
            paFloat32,
            codec_ctx->sample_rate,
            paFramesPerBufferUnspecified,
            NULL,
            NULL);
/*            pa_stream_callback,*/
/*            &codec_ctx);*/
    if (err != paNoError) {
        printf("error1:  %d, %s\n", err, Pa_GetErrorText(err));
        return -1;
    }
/*    memset(cyclic_buffer, sizeof(cyclic_buffer),*/
/*           codec_ctx->sample_fmt == AV_SAMPLE_FMT_U8 ? 0x80 : 0x00);*/
    err = Pa_StartStream(pa_stream);
    if (err != paNoError) {
        printf("error3:  %d, %s\n", err, Pa_GetErrorText(err));
        return -1;
    }
    AVPacket packet;
    while (av_read_frame(fmt_ctx, &packet) >= 0) {
        if (packet.stream_index != stream->index) {
            continue;
        }
        AVFrame frame;
        int got_frame;
        int ret = avcodec_decode_audio4(codec_ctx, &frame,
                                        &got_frame, &packet);
        if (ret < 0) {
            continue;
        }
        if (got_frame) {
            err = Pa_WriteStream(pa_stream, packet.data, paFramesPerBufferUnspecified);
            if (err != paNoError) {
                printf("error1:  %d, %s\n", err, Pa_GetErrorText(err));
                return -1;
            }
        }
        av_free_packet(&packet);
    }
    avcodec_close(codec_ctx);
    printf("Done\n");
    return 0;
}

int
Playback_pause(int i)
{
    return -1;
}

int
Playback_stop(int i)
{
    return -1;
}

int
Playback_init(void)
{
    return Pa_Initialize();
}

void Playback_free(int i)
{
    Pa_Terminate();
}
