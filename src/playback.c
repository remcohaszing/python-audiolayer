#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_mutex.h>

#define SDL_AUDIO_BUFFER_SIZE 1024

typedef struct {
    AVPacketList *first_packet, *last_packet;
    int nb_packets;
    int size;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;

int
audio_decode_frame(AVCodecContext *codec_context, uint8_t *audio_buf,
                   int buf_size)
{
    return -1;
}

void
audio_callback(void *userdata, Uint8 *stream, int length)
{
    AVCodecContext *codex_context = (AVCodecContext *)userdata;
    int len1;
    int audio_size;

    static uint8_t audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;

    while (length > 0) {
        if (audio_buf_index >= audio_buf_index) {
            audio_size = audio_decode_frame(codex_context, audio_buf,
                                            sizeof(audio_buf));
            if (audio_size < 0) {
                audio_buf_size = 1024;
                memset(audio_buf, 0, audio_buf_size);
            } else {
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;
        }
    }
    len1 = audio_buf_size - audio_buf_index;
    if (len1 > length) {
        len1 = length;
    }
    memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
    length -= len1;
    stream += len1;
    audio_buf_index += len1;
}

void
packet_queue_init(PacketQueue *queue)
{
    memset(queue, 0, sizeof(PacketQueue));
    queue->mutex = SDL_CreateMutex();
    queue->cond = SDL_CreateCond();
}

int
packet_queue_put(PacketQueue *queue, AVPacket *packet)
{
    AVPacketList *packet_list;
    if (av_dup_packet(packet) < 0) {
        return -1;
    }
    packet_list = av_malloc(sizeof(AVPacketList));
    if (!packet_list) {
        return -1;
    }
    packet_list->pkt = *packet;
    packet_list->next = NULL;

    SDL_LockMutex(queue->mutex);
    if (queue->last_packet) {
        queue->last_packet->next = packet_list;
    } else {
        queue->first_packet = packet_list;
    }
    queue->last_packet = packet_list;
    queue->nb_packets++;
    queue->size += packet_list->pkt.size;

    SDL_CondSignal(queue->cond);
    SDL_UnlockMutex(queue->mutex);
    return 0;
}

int quit = 0;

static int
packet_queue_get(PacketQueue *queue, AVPacket *packet, int block)
{
    AVPacketList *packet_list;
    int ret;

    SDL_LockMutex(queue->mutex);
    for (;;) {
        if (quit) {
            ret = -1;
            break;
        }
        packet_list = queue->first_packet;
        if (packet_list) {
            if (!queue->first_packet) {
                queue->last_packet = NULL;
            }
            queue->nb_packets--;
            queue->size -= packet_list->pkt.size;
            *packet = packet_list->pkt;
            av_free(packet_list);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(queue->cond, queue->mutex);
        }
    }
    SDL_UnlockMutex(queue->mutex);
    return ret;
}

int
Playback_play(AVCodecContext *codec_context, AVFormatContext *format_context,
              AVStream *audio_stream)
{

    SDL_AudioSpec sdl_spec;
    SDL_AudioSpec spec;
    sdl_spec.freq = codec_context->sample_rate;
    sdl_spec.format = AUDIO_S16SYS;
    sdl_spec.channels = codec_context->channels;
    sdl_spec.silence = 0;
    sdl_spec.samples = SDL_AUDIO_BUFFER_SIZE;
    sdl_spec.callback = audio_callback;
    sdl_spec.userdata = codec_context;

    if (SDL_OpenAudio(&sdl_spec, &spec) < 0) {
        return -1;
    }

    PacketQueue audio_queue;
    packet_queue_init(&audio_queue);

    AVFrame* frame = avcodec_alloc_frame();
    AVPacket packet;
    printf("Starting playback...");
    SDL_PauseAudio(0);
    int got_frame = 0;
    int len1;
    while (av_read_frame(format_context, &packet) >= 0) {
        if (packet.stream_index == audio_stream->index) {
            len1 = avcodec_decode_audio4(codec_context, frame, &got_frame, &packet);

            if (got_frame) {
                printf("len1: %d\n", len1);
            }
        }
        av_free_packet(&packet);
    }
    return 0;
}

int Playback_pause(int i)
{
    return -1;
}

int Playback_stop(int i)
{
    return -1;
}
