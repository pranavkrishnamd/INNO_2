// ============================================
// game_engine.h — INNO TableBot Stage 2
// Full-screen games for GC9A01 240x240 round TFT
//
// Games included (ported from elxie_game_console.ino logic):
//   0 = Flappy Bird
//   1 = Dino Run
//
// Controls (per pins.h):
//   PIN_TOUCH      (Sensor 1) — flap / jump / select in menu
//   PIN_TOUCH_GAME (Sensor 2) — scroll menu / restart / exit to ACTIVE
// ============================================

#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include "bot_state.h"

// Call once in setup(), after display_init()
void Game_init();

// Call every loop while bot.mode == MODE_GAME.
// Handles menu navigation, game logic, and drawing.
// Reads touch sensors directly (PIN_TOUCH / PIN_TOUCH_GAME).
// Calls Mode_exitGame(bot) on exit.
void Game_tick(BotState &bot);

#endif
