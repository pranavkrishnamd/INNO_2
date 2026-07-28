// ============================================
// alarm_engine.h — INNO TableBot Stage 2
// ============================================

#ifndef ALARM_ENGINE_H
#define ALARM_ENGINE_H

#include "bot_state.h"

// Call once per second from main loop.
// Checks bot.alarm against bot.time and sets isRinging.
void alarm_tick(BotState &bot);

// Call when Touch Sensor 1 is tapped while alarm is ringing.
// Dismisses the alarm and restores normal display.
void alarm_dismiss(BotState &bot);

#endif
