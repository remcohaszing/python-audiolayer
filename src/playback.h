#ifndef AUDIOTOOLS_PLAYBACK_H
#define AUDIOTOOLS_PLAYBACK_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>

int Playback_play(AVFormatContext *fmt_ctx, AVCodecContext *codec_ctx, AVStream *stream);

int Playback_pause(int i);

int Playback_stop(int i);

int Playback_init(void);

void Playback_free(void);

#endif
