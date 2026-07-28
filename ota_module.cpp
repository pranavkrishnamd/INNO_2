// ============================================
// ota_module.cpp — INNO TableBot Stage 2
// OTA firmware update via HTTPUpdate
// BLE sends URL → ESP32 downloads + flashes
// Last updated: June 2026
// ============================================

#include "ota_module.h"
#include "state_manager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include "pins.h"

// ── RGB LED helper (defined in main .ino) ────
extern void setRGB(uint8_t r, uint8_t g, uint8_t b);
extern void setWS2812(uint8_t r, uint8_t g, uint8_t b);

// ════════════════════════════════════════════
// OTA_init
// Call once in setup() after WiFi connects
// Nothing to configure — HTTPUpdate is
// stateless, just needs WiFi to be up
// ════════════════════════════════════════════
void OTA_init(BotState &bot) {
  Serial.println("OTA: Ready");
}

// ════════════════════════════════════════════
// OTA_onProgress
// Called repeatedly during flash write
// Prints % to Serial + pulses RGB blue
// ════════════════════════════════════════════
void OTA_onProgress(int current, int total) {
  int percent = (current * 100) / total;

  // Blue pulse on RGB LED during flash
  // Common anode — LOW = ON
  digitalWrite(PIN_RGB_B, LOW);
  digitalWrite(PIN_RGB_R, HIGH);
  digitalWrite(PIN_RGB_G, HIGH);

  Serial.print("OTA: Flashing ");
  Serial.print(percent);
  Serial.println("%");
}

// ════════════════════════════════════════════
// OTA_start
// Called from mode_manager when BLE sends:
// "OTA:https://github.com/.../firmware.bin"
//
// Flow:
//   1. Set STATE_UPDATING
//   2. Notify app via BLE
//   3. Download + flash via HTTPUpdate
//   4. On success → reboot (BLE drops here)
//   5. On fail    → State_onError + notify app
// ════════════════════════════════════════════
void OTA_start(BotState &bot, const char* url) {
  Serial.println("OTA: Starting update...");
  Serial.print("OTA: URL → ");
  Serial.println(url);

  // ── Guard: need WiFi ─────────────────────
  if (!bot.wifiConnected ||
      WiFi.status() != WL_CONNECTED) {
    Serial.println("OTA: No WiFi — aborted");
    if (bot.ble.isConnected) {
      BLE_sendMessage("OTA:FAIL:NO_WIFI");
    }
    return;
  }

  // ── Set state ────────────────────────────
  State_onUpdateStart(bot);

  // ── Notify app — BLE will drop after reboot
  if (bot.ble.isConnected) {
    BLE_sendMessage("OTA:STARTING");
  }

  // ── RGB = blue during OTA ─────────────────
  setRGB(0, 0, 255);
  setWS2812(0, 0, 50);

  // ── HTTPUpdate needs secure client ───────
  // GitHub releases use HTTPS
  WiFiClientSecure client;
  client.setInsecure();  // skip SSL cert verify
                         // fine for firmware URL
                         // from trusted GitHub release

  // ── Register progress callback ───────────
  httpUpdate.onProgress(OTA_onProgress);

  // ── Perform update ───────────────────────
  Serial.println("OTA: Connecting to server...");
  t_httpUpdate_return result =
    httpUpdate.update(client, url);

  // ── Handle result ────────────────────────
  switch (result) {

    case HTTP_UPDATE_OK:
      // Should not reach here —
      // httpUpdate.update() reboots on success
      Serial.println("OTA: Success — rebooting");
      setRGB(0, 255, 0);   // green flash before reboot
      delay(500);
      ESP.restart();
      break;

    case HTTP_UPDATE_FAILED:
      Serial.print("OTA: Failed — error: ");
      Serial.print(httpUpdate.getLastError());
      Serial.print(" — ");
      Serial.println(httpUpdate.getLastErrorString());

      setRGB(255, 0, 0);   // red = failed

      if (bot.ble.isConnected) {
        BLE_sendMessage("OTA:FAIL:DOWNLOAD_ERROR");
      }
      State_onError(bot);
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("OTA: Server says no update");
      if (bot.ble.isConnected) {
        BLE_sendMessage("OTA:FAIL:NO_UPDATE");
      }
      State_onUpdateDone(bot);  // back to ACTIVE
      break;
  }
}