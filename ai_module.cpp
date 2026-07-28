// ============================================
// ai_module.cpp — INNO TableBot Stage 2
// Gemini 1.5 Flash audio → text
// Google Translate TTS text → speech
// ============================================

#include "ai_module.h"
#include "audio_module.h"
#include "state_manager.h"
#include "ble_manager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>

// ── Gemini API endpoint ───────────────────────
#define GEMINI_MODEL     "gemini-1.5-flash"
#define GEMINI_HOST      "generativelanguage.googleapis.com"
#define GEMINI_URL       "https://generativelanguage.googleapis.com/v1beta/models/" \
                         GEMINI_MODEL ":generateContent"

// ── Google TTS (free, no key) ─────────────────
#define TTS_BASE_URL     "https://translate.google.com/translate_tts" \
                         "?ie=UTF-8&tl=en&client=tw-ob&q="

#define HTTP_TIMEOUT_MS  10000   // 10 seconds
#define MAX_RECORD_MS    8000    // 8 seconds max recording

// ════════════════════════════════════════════
// AI_init
// ════════════════════════════════════════════
void AI_init(BotState &bot) {
  bot.ai.isActive          = false;
  bot.ai.isRecording       = false;
  bot.ai.isWaitingResponse = false;
  bot.ai.hasResponse       = false;
  bot.ai.userText[0]       = '\0';
  bot.ai.responseText[0]   = '\0';
  Serial.println("AI: Module ready");
}

// ════════════════════════════════════════════
// AI_buildTTSUrl
// URL-encodes response text for Google TTS
// ════════════════════════════════════════════
void AI_buildTTSUrl(const char* text, char* urlOut, size_t urlSize) {
  char encoded[201];
  int  ei = 0;

  for (int i = 0; text[i] != '\0' && ei < 200; i++) {
    char c = text[i];
    if (c == ' ') {
      encoded[ei++] = '+';
    } else if ((c >= 'A' && c <= 'Z') ||
               (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') ||
               c == '-' || c == '_' || c == '.' || c == '~') {
      encoded[ei++] = c;
    } else {
      if (ei + 3 < 200) {
        snprintf(encoded + ei, 4, "%%%02X", (unsigned char)c);
        ei += 3;
      }
    }
  }
  encoded[ei] = '\0';
  snprintf(urlOut, urlSize, "%s%s", TTS_BASE_URL, encoded);
}

// ════════════════════════════════════════════
// AI_sendAudio
// POST WAV audio to Gemini 1.5 Flash
// ════════════════════════════════════════════
bool AI_sendAudio(BotState &bot, uint8_t* wavData, size_t wavLength) {
  if (!bot.wifiConnected || WiFi.status() != WL_CONNECTED) {
    Serial.println("AI: No WiFi");
    if (bot.ble.isConnected) BLE_sendMessage("STATE:ERROR");
    State_onError(bot);
    return false;
  }

  if (strlen(bot.ai.apiKey) == 0) {
    Serial.println("AI: No API key");
    if (bot.ble.isConnected) BLE_sendMessage("STATE:NO_APIKEY");
    return false;
  }

  // Base64 encode PCM/WAV buffer
  size_t b64Len = ((wavLength + 2) / 3) * 4 + 1;
  char* b64Data = (char*) ps_malloc(b64Len);
  if (!b64Data) {
    Serial.println("AI: Base64 PSRAM alloc failed");
    State_onError(bot);
    return false;
  }

  String b64Str = base64::encode(wavData, wavLength);
  strncpy(b64Data, b64Str.c_str(), b64Len - 1);
  b64Data[b64Len - 1] = '\0';

  // Build JSON request body
  DynamicJsonDocument doc(512);
  JsonArray contents    = doc.createNestedArray("contents");
  JsonObject part0      = contents.createNestedObject();
  JsonArray  parts      = part0.createNestedArray("parts");

  JsonObject audioPart  = parts.createNestedObject();
  JsonObject inlineData = audioPart.createNestedObject("inline_data");
  inlineData["mime_type"] = "audio/wav";
  inlineData["data"]      = b64Data;

  JsonObject textPart   = parts.createNestedObject();
  textPart["text"] = "You are INNO, a friendly AI assistant on a desk companion robot. "
                     "Listen to the audio and respond helpfully and concisely in 1-2 sentences.";

  String requestBody;
  serializeJson(doc, requestBody);
  free(b64Data);

  // HTTP POST to Gemini API
  char url[256];
  snprintf(url, sizeof(url), "%s?key=%s", GEMINI_URL, bot.ai.apiKey);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(HTTP_TIMEOUT_MS);

  Serial.println("AI: Sending audio to Gemini...");
  int httpCode = http.POST(requestBody);

  if (httpCode <= 0) {
    Serial.printf("AI: Connection failed — %s\n", http.errorToString(httpCode).c_str());
    http.end();
    if (bot.ble.isConnected) BLE_sendMessage("STATE:ERROR");
    State_onError(bot);
    return false;
  }

  if (httpCode == 429) {
    Serial.println("AI: Rate limit exceeded");
    http.end();
    if (bot.ble.isConnected) BLE_sendMessage("STATE:LIMIT_EXCEEDED");
    State_onError(bot);
    return false;
  }

  if (httpCode != 200) {
    Serial.printf("AI: HTTP error %d\n", httpCode);
    http.end();
    if (bot.ble.isConnected) BLE_sendMessage("STATE:ERROR");
    State_onError(bot);
    return false;
  }

  String response = http.getString();
  http.end();

  DynamicJsonDocument respDoc(4096);
  DeserializationError err = deserializeJson(respDoc, response);
  if (err) {
    Serial.println("AI: JSON parse error");
    State_onError(bot);
    return false;
  }

  const char* text = respDoc["candidates"][0]["content"]["parts"][0]["text"];
  if (!text) {
    Serial.println("AI: No text in response");
    State_onError(bot);
    return false;
  }

  strncpy(bot.ai.responseText, text, sizeof(bot.ai.responseText) - 1);
  bot.ai.responseText[sizeof(bot.ai.responseText) - 1] = '\0';
  bot.ai.hasResponse = true;

  Serial.print("AI: Response → ");
  Serial.println(bot.ai.responseText);
  return true;
}

// ════════════════════════════════════════════
// AI_run
// State machine manager called in loop()
// ════════════════════════════════════════════
void AI_run(BotState &bot) {
  // SPEAKING state — wait for TTS playback to finish
  if (bot.state == STATE_SPEAKING) {
    Audio_loop();
    if (!Audio_isPlaying()) {
      State_onAIDone(bot);
    }
    return;
  }

  // THINKING state — stop mic recording, send to Gemini, trigger TTS
  if (bot.state == STATE_THINKING) {
    if (!bot.ai.isWaitingResponse) return;
    bot.ai.isWaitingResponse = false;

    size_t   wavLen = 0;
    uint8_t* wav    = Audio_stopRecording(bot, wavLen);

    if (!wav || wavLen == 0) {
      Serial.println("AI: Empty recording");
      State_onError(bot);
      return;
    }

    bool ok = AI_sendAudio(bot, wav, wavLen);
    if (!ok) return;

    // Convert response text to speech URL & start playback
    char ttsUrl[512];
    AI_buildTTSUrl(bot.ai.responseText, ttsUrl, sizeof(ttsUrl));
    State_onSpeaking(bot);
    Audio_playURL(ttsUrl);
    return;
  }

  // LISTENING state — stream mic data into PSRAM buffer
  if (bot.state == STATE_LISTENING) {
    Audio_loop();

    unsigned long elapsed = millis() - bot.ai.recordStart;
    if (elapsed >= MAX_RECORD_MS) {
      Serial.println("AI: Max duration reached");
      bot.ai.isWaitingResponse = true;
      State_onThinking(bot);
    }
    return;
  }
}