// ============================================
// ai_module.h — INNO TableBot Stage 2
// Gemini 1.5 Flash (audio input) + Google TTS
// API key delivered via BLE from Niranjan's app
// Last updated: June 2026
// ============================================

#ifndef AI_MODULE_H
#define AI_MODULE_H
// Maximum recording duration (milliseconds)
#define MAX_RECORD_MS 10000      // 10 seconds

#include "bot_state.h"

// ── Init ─────────────────────────────────────
// Call once in setup() after WiFi + Audio init
void AI_init(BotState &bot);

// ── Main pipeline ────────────────────────────
// Call from loop() when bot.mode == MODE_AI
// Manages full LISTENING→THINKING→SPEAKING flow
void AI_run(BotState &bot);

// ── Send WAV to Gemini ───────────────────────
// Returns true on success
// Fills bot.ai.responseText with Gemini reply
bool AI_sendAudio(BotState &bot,
                  uint8_t* wavData,
                  size_t   wavLength);

// ── Build Google TTS URL ─────────────────────
// Free tier — no API key needed
// 200 char limit per request
void AI_buildTTSUrl(const char* text, char* urlOut, size_t urlSize);

#endif
