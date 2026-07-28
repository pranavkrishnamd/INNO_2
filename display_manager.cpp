// ============================================
// display_manager.cpp — INNO TableBot Stage 2
// GC9A01 Round TFT 240x240 (GOLDENMORNING_240x240)
// SPI: SCL=PIN_TFT_SCL, SDA=PIN_TFT_SDA, RES=PIN_TFT_RES, DC=PIN_TFT_DC
// BLK hardwired to 3.3V — no code needed
// ============================================

#include "display_manager.h"
#include "pins.h"
#include "animation_engine.h"
#include <Arduino_GFX_Library.h>

// ── SHARED DISPLAY OBJECT ────────────────────
// Other files (animation_engine, alarm_engine, etc.)
// must `extern` this object to draw on screen.
Arduino_DataBus *bus = new Arduino_ESP32SPI(
  PIN_TFT_DC,    /* DC */
  -1,            /* CS  - not used, tie CS to GND on module if present */
  PIN_TFT_SCL,   /* SCK */
  PIN_TFT_SDA,   /* MOSI */
  -1             /* MISO - not used */
);

Arduino_GFX *gfx = new Arduino_GC9A01(
  bus,
  PIN_TFT_RES,   /* RST */
  0,             /* rotation */
  true           /* IPS */
);

// ── CUTE COLOR PALETTE ───────────────────────
#define COL_BG          0x0000   // black background
#define COL_FACE_BG     0x0000   // round face bg (black)
#define COL_EYE_HAPPY   0x07FF   // cyan eyes
#define COL_EYE_FOCUS   0x841F   // soft purple
#define COL_EYE_LISTEN  0x07FF   // bright cyan
#define COL_EYE_THINK   0xFFE0   // soft yellow
#define COL_EYE_SAD     0x4A69   // muted blue-grey
#define COL_EYE_ALARM   0xF800   // red
#define COL_MOUTH       0xFD20   // peach/pink mouth
#define COL_TEXT        0xFFFF   // white text
#define COL_ACCENT      0xFC9F   // pink accent
#define COL_WEATHER     0x07E0   // green

// ── INIT ──────────────────────────────────────
void Display_init() {
  gfx->begin();
  gfx->fillScreen(COL_BG);

  // Splash screen
  gfx->setTextColor(COL_ACCENT);
  gfx->setTextSize(2);
  gfx->setCursor(60, 100);
  gfx->print("INNO");
  gfx->setTextSize(1);
  gfx->setCursor(70, 130);
  gfx->print("TableBot v2");

  Animation_init();
}

// ── HELPERS ───────────────────────────────────
static void drawClock(BotState &bot, int x, int y, bool big) {
  char buf[9];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", bot.time.hour, bot.time.minute, bot.time.second);

  gfx->setTextColor(COL_TEXT);
  gfx->setTextSize(big ? 4 : 2);

  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(x - w / 2, y - h / 2);
  gfx->print(buf);
}

static void drawWeather(BotState &bot, int x, int y) {
  if (!bot.weather.isValid) return;

  gfx->setTextColor(COL_WEATHER);
  gfx->setTextSize(1);

  char buf[24];
  snprintf(buf, sizeof(buf), "%.1fC  %.0f%%", bot.weather.temp, bot.weather.humidity);

  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(x - w / 2, y);
  gfx->print(buf);

  gfx->setCursor(x - w / 2, y + 14);
  gfx->print(bot.weather.description);
}

static void drawBatteryIcon(BotState &bot) {
  if (!bot.display.showBatteryIcon) return;

  int x = 210, y = 8, w = 22, h = 10;
  uint16_t col = COL_TEXT;
  if (bot.battery.isLow) col = COL_EYE_ALARM;

  gfx->drawRect(x, y, w, h, col);
  gfx->fillRect(x + w, y + 2, 2, h - 4, col); // nub

  int fillW = (w - 2) * bot.battery.percentage / 100;
  gfx->fillRect(x + 1, y + 1, fillW, h - 2, col);

  if (bot.battery.isCharging) {
    gfx->setTextColor(COL_ACCENT);
    gfx->setTextSize(1);
    gfx->setCursor(x - 12, y);
    gfx->print("z"); // simple charging bolt placeholder
  }
}

static void drawBLEIcon(BotState &bot) {
  if (!bot.display.showBLEIcon) return;
  uint16_t col = bot.ble.isConnected ? 0x001F : 0x4208; // blue if connected
  gfx->fillCircle(15, 15, 5, col);
}

// ── MODE DRAW FUNCTIONS ───────────────────────
static void drawActive(BotState &bot) {
  gfx->fillScreen(COL_BG);

  // Face centered upper-middle
  bot.face.targetExpression = FACE_HAPPY;
  drawFace(bot.face, 120, 100, COL_EYE_HAPPY, COL_MOUTH);

  if (bot.display.showClock) drawClock(bot, 120, 175, false);
  if (bot.display.showWeather) drawWeather(bot, 120, 200);

  drawBatteryIcon(bot);
  drawBLEIcon(bot);
}

static void drawFocus(BotState &bot) {
  gfx->fillScreen(COL_BG);

  bot.face.targetExpression = FACE_FOCUSED;
  drawFace(bot.face, 120, 70, COL_EYE_FOCUS, COL_MOUTH);

  drawClock(bot, 120, 150, true);

  drawBatteryIcon(bot);
  drawBLEIcon(bot);
}

static void drawAI(BotState &bot) {
  gfx->fillScreen(COL_BG);

  uint16_t eyeColor = COL_EYE_LISTEN;

  switch (bot.state) {
    case STATE_LISTENING:
      bot.face.targetExpression = FACE_LISTENING;
      eyeColor = COL_EYE_LISTEN;
      break;
    case STATE_THINKING:
      bot.face.targetExpression = FACE_THINKING;
      eyeColor = COL_EYE_THINK;
      break;
    case STATE_SPEAKING:
      bot.face.targetExpression = FACE_SPEAKING;
      eyeColor = COL_EYE_HAPPY;
      break;
    default:
      bot.face.targetExpression = FACE_HAPPY;
      break;
  }

  drawFace(bot.face, 120, 100, eyeColor, COL_MOUTH);

  if (bot.display.showAIResponse && bot.ai.hasResponse) {
    gfx->setTextColor(COL_TEXT);
    gfx->setTextSize(1);
    gfx->setCursor(10, 190);
    // Simple word-wrap-free single line preview
    gfx->print(bot.ai.responseText);
  }

  drawBLEIcon(bot);
}

static void drawAlarm(BotState &bot) {
  gfx->fillScreen(COL_EYE_ALARM);

  bot.face.targetExpression = FACE_ALARM;
  drawFace(bot.face, 120, 90, COL_BG, COL_BG);

  gfx->setTextColor(COL_TEXT);
  gfx->setTextSize(2);
  gfx->setCursor(60, 160);
  gfx->print("ALARM!");

  gfx->setTextSize(1);
  gfx->setCursor(50, 185);
  gfx->print(bot.alarm.label);

  gfx->setCursor(70, 200);
  gfx->print("Tap to dismiss");
}

static void drawReminder(BotState &bot, ReminderData &rem) {
  // Popup box overlay — does not clear whole screen
  int x = 20, y = 70, w = 200, h = 100;
  gfx->fillRoundRect(x, y, w, h, 12, COL_ACCENT);
  gfx->drawRoundRect(x, y, w, h, 12, COL_TEXT);

  gfx->setTextColor(COL_BG);
  gfx->setTextSize(1);
  gfx->setCursor(x + 10, y + 10);
  gfx->print("Reminder:");

  gfx->setCursor(x + 10, y + 30);
  gfx->print(rem.text);

  gfx->setCursor(x + 10, y + h - 20);
  gfx->print("Tap to dismiss");
}

static void drawError(BotState &bot) {
  gfx->fillScreen(COL_BG);

  bot.face.targetExpression = FACE_SAD;
  drawFace(bot.face, 120, 100, COL_EYE_SAD, COL_MOUTH);

  gfx->setTextColor(COL_EYE_ALARM);
  gfx->setTextSize(1);
  gfx->setCursor(60, 180);
  gfx->print("Something went wrong");
}

// ── MAIN RENDER ────────────────────────────────
void Display_render(BotState &bot) {
  // Game mode handled entirely by game_engine — skip drawing here
  if (bot.display.showGame) return;

  if (bot.display.showAlarm && bot.alarm.isRinging) {
    drawAlarm(bot);
    return;
  }

  if (bot.state == STATE_ERROR) {
    drawError(bot);
    return;
  }

  switch (bot.mode) {
    case MODE_ACTIVE:
      drawActive(bot);
      break;
    case MODE_FOCUS:
      drawFocus(bot);
      break;
    case MODE_AI:
      drawAI(bot);
      break;
    case MODE_GAME:
      // handled by game_engine
      break;
  }

  // Reminder popup drawn on top of whatever mode is active
  if (bot.display.showReminder) {
    for (int i = 0; i < 5; i++) {
      if (bot.reminders[i].isShowing) {
        drawReminder(bot, bot.reminders[i]);
        break; // only show one at a time
      }
    }
  }
}
