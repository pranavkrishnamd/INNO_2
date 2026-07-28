// ============================================
// reminder_engine.h — INNO TableBot Stage 2
// ============================================

#ifndef REMINDER_ENGINE_H
#define REMINDER_ENGINE_H

#include "bot_state.h"

// Call once per second from main loop.
// Checks all 5 entries in bot.reminders against bot.time.
void reminder_tick(BotState &bot);

// Call when Touch Sensor 1 is tapped while a reminder popup is showing.
// Dismisses the currently showing reminder (isSet = false).
void reminder_dismiss(BotState &bot);

#endif
