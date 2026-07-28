#ifndef AUDIO_MODULE_H
#define AUDIO_MODULE_H

#include "bot_state.h"
#include <Audio.h>

void     Audio_init(BotState &bot);
void     Audio_startRecording(BotState &bot);
uint8_t* Audio_stopRecording(BotState &bot, size_t &length);
void     Audio_loop();
void     Audio_playURL(const char* url);
bool     Audio_isPlaying();
void     Audio_stop();
void     Audio_setVolume(int volumePercent);

#endif