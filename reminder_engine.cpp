// ============================================
// reminder_engine.cpp — INNO TableBot Stage 2
//
// Called every second from main loop.
// Checks all 5 entries in bot.reminders[].
// When hour/minute match current time and the
// reminder is set and not already showing:
//   - reminders[i].isShowing = true
//   - bot.display.showReminder = true
// display_manager draws a popup overlay while showing.
//
// Touch Sensor 1 (PIN_TOUCH) tap → reminder_dismiss()
//   - marks the showing reminder isSet = false
//   - clears isShowing
// ============================================

#include "reminder_engine.h"

// Tracks last-triggered minute per reminder slot to
// avoid re-triggering every second within the same minute.
static int lastTriggeredMinute[5] = { -1, -1, -1, -1, -1 };

void reminder_tick(BotState &bot) {
  for (int i = 0; i < 5; i++) {
    ReminderData &r = bot.reminders[i];

    if (!r.isSet) continue;
    if (r.isShowing) continue; // already showing, wait for dismiss

    if (r.hour == bot.time.hour &&
        r.minute == bot.time.minute &&
        bot.time.minute != lastTriggeredMinute[i]) {

      r.isShowing = true;
      bot.display.showReminder = true;
      lastTriggeredMinute[i] = bot.time.minute;
    }
  }
}

void reminder_dismiss(BotState &bot) {
  for (int i = 0; i < 5; i++) {
    ReminderData &r = bot.reminders[i];

    if (r.isShowing) {
      r.isShowing = false;
      r.isSet = false; // dismissed reminders are cleared per spec
      break; // only one popup shown at a time
    }
  }

  // Check if any reminder is still showing — if not, hide popup flag
  bool anyShowing = false;
  for (int i = 0; i < 5; i++) {
    if (bot.reminders[i].isShowing) {
      anyShowing = true;
      break;
    }
  }
  bot.display.showReminder = anyShowing;
}
