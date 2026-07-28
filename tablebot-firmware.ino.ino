// ============================================
// tablebot-firmware.ino — INNO TableBot
// Board: Waveshare ESP32-S3-Tiny
// Stage 2 — Final June 2026
// ============================================

#include <Wire.h>
#include <Arduino.h>
#include "pins.h"
#include "bot_state.h"
#include "state_manager.h"
#include "ble_manager.h"
#include "ota_module.h"
#include "mode_manager.h"
#include "audio_module.h"
#include "ai_module.h"
#include "display_manager.h"
#include "animation_engine.h"
#include "alarm_engine.h"
#include "reminder_engine.h"
#include "game_engine.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <Adafruit_NeoPixel.h>
#include "time.h"
#include "weather_module.h"
#include <nvs_flash.h>

// ── Global instances ──────────────────────────
BotState bot;
Adafruit_NeoPixel ws2812(WS2812_COUNT,
                          PIN_WS2812,
                          NEO_GRB + NEO_KHZ800);

// ── NTP ───────────────────────────────────────
#define NTP_SERVER   "pool.ntp.org"
#define GMT_OFFSET   19800
#define DAYLIGHT     0

// ── Task intervals ────────────────────────────
#define INTERVAL_BLE          20
#define INTERVAL_INPUT        50
#define INTERVAL_ANIM         100
#define INTERVAL_GAME         20
#define INTERVAL_CLOCK        1000
#define INTERVAL_IDLE_CHECK   5000
#define INTERVAL_STATUS       5000
#define INTERVAL_WEATHER      600000
#define INTERVAL_NTP          3600000
#define INTERVAL_BATTERY      30000

// ── Timing variables ─────────────────────────
unsigned long lastBLECheck     = 0;
unsigned long lastInputCheck   = 0;
unsigned long lastAnimFrame    = 0;
unsigned long lastGameFrame    = 0;
unsigned long lastClockUpdate  = 0;
unsigned long lastIdleCheck    = 0;
unsigned long lastStatusSend   = 0;
unsigned long lastWeatherFetch = 0;
unsigned long lastNTPSync      = 0;
unsigned long lastBatteryCheck = 0;

bool sensor1WasHigh = false;
bool sensor2WasHigh = false;

void checkBattery();
void setRGB(uint8_t r, uint8_t g, uint8_t b);
void setWS2812(uint8_t r, uint8_t g, uint8_t b);
const char* getDayName(int d);

// ── Touch 1 mode cycle ───────────────────────
// ACTIVE → AI → GAME → ACTIVE
// FOCUS is app-only, not in touch cycle
static void cycleModeTouch1(BotState &bot) {
  switch (bot.mode) {
    case MODE_ACTIVE:
      Mode_setAI(bot);
      break;
    case MODE_AI:
      // Exit AI cleanly before entering game
      if (bot.state == STATE_LISTENING ||
          bot.state == STATE_THINKING  ||
          bot.state == STATE_SPEAKING) {
        Audio_stop();
        State_onAIDone(bot);
      }
      Mode_setGame(bot);
      break;
    case MODE_GAME:
      Mode_exitGame(bot);   // returns to ACTIVE
      break;
    case MODE_FOCUS:
      // Focus is app-only — touch 1 does nothing here
      break;
  }
}// ════════════════════════════════════════════
// SETUP - FINAL POLISHED SEQUENCE
// ════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("==============================");
  Serial.println("INNO TableBot booting...");

  // ── 0. NVS INITIALIZATION (Required to save WiFi passwords) ──
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  // ── 1. HARDWARE INIT ────────────────────────
  pinMode(PIN_TOUCH,      INPUT_PULLDOWN);
  pinMode(PIN_TOUCH_GAME, INPUT_PULLDOWN);
  pinMode(PIN_CHARGE_STATUS, INPUT_PULLUP);
  pinMode(PIN_CHARGE_FULL,   INPUT_PULLUP);
  pinMode(PIN_RGB_R, OUTPUT);
  pinMode(PIN_RGB_G, OUTPUT);
  pinMode(PIN_RGB_B, OUTPUT);
  setRGB(0, 0, 0);

  ws2812.begin();
  ws2812.clear();
  ws2812.show();

  Display_init();

  // ── 2. WIFI HOTSPOT FIRST ───────────────────
  // We do this BEFORE Bluetooth so the antenna can broadcast the AP cleanly
  Serial.println("WiFi: Starting INNO-Setup Hotspot...");
  WiFi.mode(WIFI_STA);
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180); // 3 minutes to enter password
  
  // This broadcasts the INNO-Setup network
  wifiManager.autoConnect("INNO-Setup");

  if (WiFi.status() == WL_CONNECTED) {
    bot.wifiConnected = true;
    Serial.print("WiFi: Connected -> ");
    Serial.println(WiFi.localIP());
  } else {
    bot.wifiConnected = false;
    Serial.println("WiFi: Portal timeout — continuing offline");
  }

  // ── 3. START BLUETOOTH SECOND ────────────────
  // Now that WiFi is connected, it is 100% safe to start BLE
  BLE_init(bot);
  Serial.println("BLE IS LIVE: You can connect the app NOW.");

  // ── 4. REMAINING MODULES ────────────────────
  OTA_init(bot);
  //Audio_init(bot);
  //AI_init(bot);

  Animation_init();
  Game_init();
  State_init(bot);
  Mode_init(bot);

  bot.firstBootDone = true;
  Serial.println("INNO: Boot complete");
  Serial.println("==============================");
}

// ════════════════════════════════════════════
// LOOP
// ════════════════════════════════════════════
void loop() {
  unsigned long now = millis();

  // ── Always: Audio loop ───────────────────
//  Audio_loop();

  // ── Always: AI pipeline ──────────────────
  if (bot.mode == MODE_AI) {
    AI_run(bot);
  }

  // ── 20ms: BLE ────────────────────────────
  if (now - lastBLECheck >= INTERVAL_BLE) {
    lastBLECheck = now;
    BLE_poll(bot);

    if (bot.ble.isConnected) {
      setWS2812(0, 0, 50);
    } else {
      setWS2812(0, 0, 0);
    }

    if (bot.ble.newCommandReceived) {
      Mode_apply(bot);
    }
  }

  // ── 50ms: Input ──────────────────────────
  if (now - lastInputCheck >= INTERVAL_INPUT) {
    lastInputCheck = now;

    // ── Touch 1 (LEFT) ───────────────────
    bool s1High = (digitalRead(PIN_TOUCH) == HIGH);
    if (s1High && !sensor1WasHigh) {
      if (now - bot.lastTouchMs > 300) {
        bot.lastTouchMs = now;

        // Priority:
        // 1. Alarm dismiss
        // 2. Reminder dismiss
        // 3. AI mid-pipeline interaction
        // 4. Mode cycle ACTIVE→AI→GAME→ACTIVE

        if (bot.alarm.isRinging) {
          alarm_dismiss(bot);
        } else if (bot.display.showReminder) {
          reminder_dismiss(bot);
        } else if (bot.mode == MODE_AI &&
                   (bot.state == STATE_LISTENING ||
                    bot.state == STATE_THINKING  ||
                    bot.state == STATE_SPEAKING)) {
          State_onTouch(bot);
        } else if (bot.mode == MODE_GAME) {
          // Game handles touch 1 internally via game_engine
          // do nothing here
        } else {
          cycleModeTouch1(bot);
        }
      }
    }
    sensor1WasHigh = s1High;

    // ── Touch 2 (RIGHT) ──────────────────
    // Handled entirely by game_engine when in game
    // No function outside game mode
  }

  // ── 100ms: Animation ─────────────────────
  if (now - lastAnimFrame >= INTERVAL_ANIM) {
    lastAnimFrame = now;
    if (bot.mode != MODE_GAME) {
      Anim_tick(bot.face);
      Display_render(bot);
    }
  }

  // ── 20ms: Game ───────────────────────────
  if (now - lastGameFrame >= INTERVAL_GAME) {
    lastGameFrame = now;
    if (bot.mode == MODE_GAME) {
      Game_tick(bot);
    }
  }

  // ── 1s: Clock + Alarm + Reminder ─────────
  if (now - lastClockUpdate >= INTERVAL_CLOCK) {
    lastClockUpdate = now;
    if (bot.mode != MODE_GAME) {
      bot.time.second++;
      if (bot.time.second >= 60) {
        bot.time.second = 0;
        bot.time.minute++;
        if (bot.time.minute >= 60) {
          bot.time.minute = 0;
          bot.time.hour++;
          if (bot.time.hour >= 24)
            bot.time.hour = 0;
        }
      }
    }
    alarm_tick(bot);
    reminder_tick(bot);
  }

  // ── 5s: Idle check ───────────────────────
  if (now - lastIdleCheck >= INTERVAL_IDLE_CHECK) {
    lastIdleCheck = now;
    State_checkIdle(bot);
  }

  // ── 5s: BLE status ───────────────────────
  if (now - lastStatusSend >= INTERVAL_STATUS) {
    lastStatusSend = now;
    if (bot.ble.isConnected) {
      BLE_sendStatus(bot);
    }
  }

  // ── 30s: Battery ─────────────────────────
  if (now - lastBatteryCheck >= INTERVAL_BATTERY) {
    lastBatteryCheck = now;
    checkBattery();
  }

  // ── 10min: Weather ───────────────────────
  if (now - lastWeatherFetch >= INTERVAL_WEATHER) {
   lastWeatherFetch = now;
    if (bot.wifiConnected &&
        bot.mode != MODE_GAME &&
        bot.mode != MODE_AI) {
     State_onUpdateStart(bot);
      bool ok = Weather_fetch(bot);
      if (ok) State_onUpdateDone(bot);
      else    State_onError(bot);
    }
  }

  // ── 1hr: NTP resync ──────────────────────
  if (now - lastNTPSync >= INTERVAL_NTP) {
    lastNTPSync = now;
    configTime(GMT_OFFSET, DAYLIGHT, NTP_SERVER);
    Serial.println("NTP: Re-synced");
  }
}

// ════════════════════════════════════════════
// HELPERS
// ════════════════════════════════════════════

// RGB LED — common anode (LOW = ON)
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  digitalWrite(PIN_RGB_R, r > 0 ? LOW : HIGH);
  digitalWrite(PIN_RGB_G, g > 0 ? LOW : HIGH);
  digitalWrite(PIN_RGB_B, b > 0 ? LOW : HIGH);
}

// WS2812 onboard LED
void setWS2812(uint8_t r, uint8_t g, uint8_t b) {
  ws2812.setPixelColor(0, ws2812.Color(r, g, b));
  ws2812.show();
}

void checkBattery() {
  int raw     = analogRead(PIN_BATTERY_ADC);
  int percent = constrain(
                  map(raw, 0, 4095, 0, 100),
                  0, 100);

  bot.battery.percentage = percent;
  bot.battery.isCharging =
    (digitalRead(PIN_CHARGE_STATUS) == LOW);
  bot.battery.isLow = (percent < 20);

  if (bot.battery.isCharging) {
    setRGB(255, 0, 0);
  } else if (percent > 60) {
    setRGB(0, 255, 0);
  } else if (percent > 20) {
    setRGB(255, 165, 0);
  } else {
    setRGB(255, 0, 0);
  }

  bot.display.showBatteryIcon = bot.battery.isLow;

  if (percent < 10 && bot.ble.isConnected) {
    BLE_sendMessage("BATTERY:CRITICAL");
  }

  Serial.print("Battery: ");
  Serial.print(percent);
  Serial.print("%  Charging: ");
  Serial.println(bot.battery.isCharging ? "YES" : "NO");
}

const char* getDayName(int d) {
  const char* days[] = {
    "Sunday","Monday","Tuesday","Wednesday",
    "Thursday","Friday","Saturday"
  };
  if (d >= 0 && d <= 6) return days[d];
  return "Unknown";
}
