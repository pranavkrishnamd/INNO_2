// ============================================
// ota_module.h — INNO TableBot Stage 2
// OTA firmware update via HTTPUpdate
// Triggered by BLE command from Niranjan's app
// Last updated: June 2026
// ============================================

#ifndef OTA_MODULE_H
#define OTA_MODULE_H

#include "bot_state.h"

// ── Init ─────────────────────────────────────
// Call once in setup() after WiFi connects
void OTA_init(BotState &bot);

// ── Trigger ──────────────────────────────────
// Called from mode_manager when BLE sends
// "OTA:https://github.com/.../firmware.bin"
void OTA_start(BotState &bot, const char* url);

// ── Progress callback (internal) ─────────────
// Prints progress to Serial during flash
void OTA_onProgress(int current, int total);

#endif