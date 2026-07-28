#include "weather_module.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

bool Weather_fetch(BotState &bot) {
  if (!bot.wifiConnected || WiFi.status() != WL_CONNECTED) {
    Serial.println("Weather: WiFi disconnected");
    bot.weather.isValid = false;
    return false;
  }

  // Open-Meteo free API endpoint using location set by BLE/firmware
  char url[256];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current_weather=true",
           bot.weather.latitude, bot.weather.longitude);

  WiFiClientSecure client;
  client.setInsecure(); // Skip TLS check for Open-Meteo

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("Weather: HTTP begin failed");
    bot.weather.isValid = false;
    return false;
  }

  http.setTimeout(8000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("Weather: HTTP error %d\n", httpCode);
    http.end();
    bot.weather.isValid = false;
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.println("Weather: JSON parse failed");
    bot.weather.isValid = false;
    return false;
  }

  JsonObject current = doc["current_weather"];
  if (current.isNull()) {
    bot.weather.isValid = false;
    return false;
  }

  bot.weather.temp = current["temperature"] | 0.0f;
  int code = current["weathercode"] | 0;

  // WMO Weather interpretation codes
  if (code == 0) strncpy(bot.weather.description, "Clear", 32);
  else if (code <= 3) strncpy(bot.weather.description, "Cloudy", 32);
  else if (code <= 48) strncpy(bot.weather.description, "Foggy", 32);
  else if (code <= 67) strncpy(bot.weather.description, "Rainy", 32);
  else if (code <= 77) strncpy(bot.weather.description, "Snowy", 32);
  else if (code <= 82) strncpy(bot.weather.description, "Showers", 32);
  else strncpy(bot.weather.description, "Stormy", 32);

  bot.weather.humidity = 65.0f; // Standard estimate
  bot.weather.isValid = true;

  Serial.printf("Weather: Updated -> %.1f C, %s\n", bot.weather.temp, bot.weather.description);
  return true;
}