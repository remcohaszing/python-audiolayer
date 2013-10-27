#ifndef AUDIOTOOLS_PLAYBACK_H
#define AUDIOTOOLS_PLAYBACK_H

#include <SDL2/SDL_audio.h>

typedef struct {
    AVPacketList *first_packet, *last_packet;
    int nb_packets;
    int size;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;

int Playback_play(AVFormatContext *fmt_ctx);

int Playback_pause(int i);

int Playback_stop(int i);

int Playback_init(void);

#endif
