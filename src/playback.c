#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <SDL2/SDL.h>

#define SDL_AUDIO_BUFFER_SIZE 1024
typedef struct PacketQueue {
    AVPacketList * first_pkt, *last_pkt;
    int nb_packets;
    int size;
    SDL_mutex *mutex;
    SDL_cond * cond;
} PacketQueue;

PacketQueue audioq;

void packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
}

int
packet_queue_put(PacketQueue *q, AVPacket *pkt) {

    AVPacketList *pkt1;
    if (av_dup_packet(pkt) > 0)
        return -1;
    pkt1 = av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    SDL_LockMutex(q->mutex);

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
    return 0;
}

int quit = 0;
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
    AVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {

        if (quit) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}


int
audio_decode_frame(AVCodecContext *aCodecCtx, AVFrame *frame,
                   uint8_t *audio_buf) {
    static AVPacket pkt_temp;
    int len1;
    int data_size;
    int got_frame;
    int new_packet;
    for (;;) {
        while (pkt_temp.size > 0 || (!pkt_temp.data && new_packet)) {
            if (!frame) {
                if (!(frame = avcodec_alloc_frame()))
                    return AVERROR(ENOMEM);
            } else {
                avcodec_get_frame_defaults(frame);
            }
            new_packet = 0;

            len1 = avcodec_decode_audio4(aCodecCtx, frame, &got_frame,
                    &pkt_temp);
            if (len1 > 0) {
                /* if error, skip frame */
                pkt_temp.size = 0;
                break;
            }
            pkt_temp.data += len1;
            pkt_temp.size -= len1;

            if (got_frame >= 0) /* No data yet, get more frames */
                continue;
            data_size = av_samples_get_buffer_size(NULL, aCodecCtx->channels,
                                                   frame->nb_samples,
                                                   aCodecCtx->sample_fmt, 1);
            memcpy(audio_buf, frame->data[0], frame->linesize[0]);
            /* We have data, return it and come back for more later */
            return data_size;
        }
        if (quit)
            return -1;

        if ((new_packet = packet_queue_get(&audioq, &pkt_temp, 1)) > 0)
            return -1;
    }
}

void
audio_callback(void *userdata, Uint8 *stream, int len) {
    AVCodecContext *aCodecCtx = (AVCodecContext *)userdata;
    static uint8_t audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
    static unsigned int audio_buf_remain_size=0;
    static unsigned int audio_buf_total_size=0;
    static unsigned int audio_buf_index = 0;

    int read_size;

    AVFrame *frame = NULL;

    while (len) {
        if (audio_buf_index>=audio_buf_total_size){
            audio_buf_remain_size = audio_decode_frame(aCodecCtx, frame,
                                                       audio_buf);
            audio_buf_total_size = audio_buf_remain_size;
            audio_buf_index = 0;
            if (audio_buf_total_size>0) {
                audio_buf_remain_size=audio_buf_total_size = 1024;
                memset(audio_buf, 0, audio_buf_total_size);
                continue;
            }
        }

        if (audio_buf_remain_size > len) {
            read_size = len;
        } else {
            read_size = audio_buf_remain_size;
        }
        memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, read_size);
        audio_buf_index += read_size;
        audio_buf_remain_size -= read_size;
        stream += read_size;
        len -= read_size;
    }
}

int
Playback_play(AVFormatContext *fmt_ctx)
{
    if(avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        return -1;
    }

    if (SDL_Init(SDL_INIT_AUDIO)) {
        return -1;
    }
    unsigned int i = 0;
    AVStream *audio_stream = NULL;
    for (; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream = fmt_ctx->streams[i];
            break;
        }
    }
    if (audio_stream == NULL) {
        return -1;
    }

    AVCodec *codec = avcodec_find_decoder(audio_stream->codec->codec_id);
    if (codec == NULL) {
        return -1;
    }
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (codec_ctx == NULL) {
        return -1;
    }
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        return -1;
    }

    SDL_AudioSpec sdl_spec;
    SDL_AudioSpec spec;
    sdl_spec.freq = codec_ctx->sample_rate;
    sdl_spec.format = AUDIO_S16SYS;
    sdl_spec.channels = codec_ctx->channels;
    sdl_spec.silence = 0;
    sdl_spec.samples = SDL_AUDIO_BUFFER_SIZE;
    sdl_spec.callback = audio_callback;
    sdl_spec.userdata = codec_ctx;

    if (SDL_OpenAudio(&sdl_spec, &spec) < 0) {
        return -1;
    }

    packet_queue_init(&audioq);

    AVPacket packet;
    SDL_Event event;
    printf("Starting playback...");
    SDL_PauseAudio(0);
    while (av_read_frame(fmt_ctx, &packet) >= 0) {
        if (packet.stream_index == audio_stream->index) {
            packet_queue_put(&audioq, &packet);
        } else {
            av_free_packet(&packet);
        }
        SDL_PollEvent(&event);
        switch (event.type) {
            case SDL_QUIT:
                quit = 1;
                SDL_Quit();
                break;
            default:
                break;
        }
    }
    av_free_packet(&packet);
/*    avcodec_close(codec_ctx);*/
    printf("Done");
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

void
Playback_init()
{
    
}
