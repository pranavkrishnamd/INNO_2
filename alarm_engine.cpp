// ============================================
// alarm_engine.cpp — INNO TableBot Stage 2
//
// Called every second from main loop.
// When bot.alarm.hour/minute match current time:
//   - bot.alarm.isRinging = true
//   - bot.face.targetExpression = FACE_ALARM
//   - bot.display.showAlarm = true
// display_manager draws full-screen alarm while
// isRinging is true.
//
// Touch Sensor 1 (PIN_TOUCH) tap → alarm_dismiss()
// ============================================

#include "alarm_engine.h"

// Tracks the last minute we triggered, so the alarm
// doesn't re-trigger every second for the full minute.
static int lastTriggeredMinute = -1;

void alarm_tick(BotState &bot) {
  if (!bot.alarm.isSet) return;
  if (bot.alarm.isRinging) return; // already ringing, wait for dismiss

  if (bot.time.hour == bot.alarm.hour &&
      bot.time.minute == bot.alarm.minute &&
      bot.time.minute != lastTriggeredMinute) {

    bot.alarm.isRinging = true;
    bot.face.targetExpression = FACE_ALARM;
    bot.display.showAlarm = true;

    lastTriggeredMinute = bot.time.minute;
  }

  // Reset trigger guard once the minute has passed
  if (bot.time.minute != lastTriggeredMinute) {
    // allow re-trigger next time minute matches again
  }
}

void alarm_dismiss(BotState &bot) {
  if (!bot.alarm.isRinging) return;

  bot.alarm.isRinging = false;
  bot.display.showAlarm = false;

  // Restore face based on current mode — display_manager
  // will set the correct targetExpression on next render,
  // but set a safe default here in case it's checked first.
  if (bot.mode == MODE_ACTIVE) {
    bot.face.targetExpression = FACE_HAPPY;
  } else if (bot.mode == MODE_FOCUS) {
    bot.face.targetExpression = FACE_FOCUSED;
  }
}
