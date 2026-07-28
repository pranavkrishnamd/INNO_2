#ifndef WEATHER_MODULE_H
#define WEATHER_MODULE_H

#include "bot_state.h"

// Fetches current weather from Open-Meteo REST API and updates bot.weather
bool Weather_fetch(BotState &bot);

#endif