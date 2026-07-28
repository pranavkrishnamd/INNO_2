// ============================================
// game_engine.cpp — INNO TableBot Stage 2
// Full-screen games for GC9A01 240x240 round TFT
//
// Ported from elxie_game_console.ino (OLED 128x64)
// to Arduino_GFX on 240x240 round display.
//
// Coordinates roughly scaled ~2x from the original
// 128x64 layouts and re-centered for the round screen
// (play area kept inside the visible circle).
//
// CONTROLS:
//   Menu:
//     PIN_TOUCH_GAME (Sensor 2) → scroll selection
//     PIN_TOUCH      (Sensor 1) → select / enter game
//   In-game:
//     PIN_TOUCH_GAME (Sensor 2) → flap (Flappy) / jump (Dino) / restart when dead
//     PIN_TOUCH      (Sensor 1) → hold ~1.2s → exit to menu
//   From menu:
//     PIN_TOUCH      (Sensor 1) → hold ~1.2s → exit to ACTIVE mode
//
// All state lives in bot.game (BotState) — no new globals
// besides game-internal static variables scoped to this file.
// ============================================

#include "game_engine.h"
#include "pins.h"
#include "mode_manager.h"
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx;

// ── COLORS ─────────────────────────────────────
#define GCOL_BG       0x0000   // black
#define GCOL_FG       0xFFFF   // white
#define GCOL_ACCENT   0xFC9F   // pink accent (matches face palette)
#define GCOL_GROUND   0x07FF   // cyan ground line
#define GCOL_BAD      0xF800   // red (game over)

// ── SCREEN GEOMETRY (240x240 round) ────────────
#define SCR_W   240
#define SCR_H   240
// Usable rectangular play area inscribed in the circle,
// leaving a margin so corners aren't clipped by the round bezel.
#define PLAY_X   20
#define PLAY_Y   30
#define PLAY_W   200
#define PLAY_H   180
#define GROUND_Y (PLAY_Y + PLAY_H)   // y = 210

// ── APP-LEVEL STATE ─────────────────────────────
enum GameAppState { GA_MENU, GA_PLAYING };
static GameAppState gaState = GA_MENU;

#define NUM_GAMES 2
static const char *gameNames[NUM_GAMES] = { "FLAPPY BIRD", "DINO RUN" };

// ── TOUCH HANDLING ───────────────────────────────
// Both touch sensors are TTP223, INPUT_PULLDOWN (per pins.h):
// idle = LOW, pressed = HIGH.

static bool touch1Pressed() {
  static bool last = LOW;
  bool cur = digitalRead(PIN_TOUCH);
  bool edge = (last == LOW && cur == HIGH);
  last = cur;
  return edge;
}

static bool touch2Pressed() {
  static bool last = LOW;
  bool cur = digitalRead(PIN_TOUCH_GAME);
  bool edge = (last == LOW && cur == HIGH);
  last = cur;
  return edge;
}

// Long-press detection for Sensor 1 (exit from game/menu)
#define EXIT_HOLD_MS 1200
static unsigned long touch1HoldStart = 0;

static bool touch1LongPress() {
  if (digitalRead(PIN_TOUCH) == HIGH) {
    if (touch1HoldStart == 0) touch1HoldStart = millis();
    if (millis() - touch1HoldStart > EXIT_HOLD_MS) {
      touch1HoldStart = 0;
      return true;
    }
  } else {
    touch1HoldStart = 0;
  }
  return false;
}

// Long-press detection for Sensor 2 (unused for exit now, kept for future use)
static unsigned long touch2HoldStart = 0;

static bool touch2LongPress() {
  if (digitalRead(PIN_TOUCH_GAME) == HIGH) {
    if (touch2HoldStart == 0) touch2HoldStart = millis();
    if (millis() - touch2HoldStart > EXIT_HOLD_MS) {
      touch2HoldStart = 0;
      return true;
    }
  } else {
    touch2HoldStart = 0;
  }
  return false;
}

// ── HIGH SCORES (per-game, persist for session) ──
static int flappyHighScore = 0;
static int dinoHighScore   = 0;

// ═══════════════════════════════════════════════════════════════════
//  ██  GAME SELECTION MENU  ██
// ═══════════════════════════════════════════════════════════════════
static void drawMenu(BotState &bot) {
  gfx->fillScreen(GCOL_BG);

  gfx->setTextColor(GCOL_ACCENT);
  gfx->setTextSize(2);
  gfx->setCursor(50, 30);
  gfx->print("SELECT GAME");

  for (int i = 0; i < NUM_GAMES; i++) {
    int y = 90 + i * 40;
    bool sel = (i == bot.game.selectedGame);

    if (sel) {
      gfx->fillRoundRect(30, y - 6, 180, 32, 10, GCOL_ACCENT);
      gfx->setTextColor(GCOL_BG);
    } else {
      gfx->drawRoundRect(30, y - 6, 180, 32, 10, GCOL_FG);
      gfx->setTextColor(GCOL_FG);
    }

    gfx->setTextSize(2);
    gfx->setCursor(48, y);
    gfx->print(gameNames[i]);
  }

  gfx->setTextColor(GCOL_FG);
  gfx->setTextSize(1);
  gfx->setCursor(40, 200);
  gfx->print("S2 scroll  S1 select");
  gfx->setCursor(52, 215);
  gfx->print("Hold S1 to exit");
}

static void menuNavigate(BotState &bot) {
  if (touch2Pressed()) {
    bot.game.selectedGame = (bot.game.selectedGame + 1) % NUM_GAMES;
  }
}

// ═══════════════════════════════════════════════════════════════════
//  ██  FLAPPY BIRD  ██
//  Original 128x64 logic scaled ~1.875x horizontal, ~2.8x vertical
//  into the 200x180 play area.
// ═══════════════════════════════════════════════════════════════════
namespace Flappy {
  float birdY, birdVY;
  int   pipeX, gapY, score;
  bool  dead;
  unsigned long lastUpdate;

  const int PIPE_W   = 18;
  const int GAP_H    = 60;     // gap height (scaled from 18 -> ~60)
  const int BIRD_X   = 36;     // bird x position within play area
  const int BIRD_R   = 8;

  void reset() {
    birdY = PLAY_H / 2;
    birdVY = 0;
    pipeX = PLAY_W;
    gapY = random(20, PLAY_H - GAP_H - 20);
    score = 0;
    dead = false;
    lastUpdate = millis();
  }

  void update() {
    if (dead) return;
    if (millis() - lastUpdate < 50) return;
    lastUpdate = millis();

    birdVY += 0.55f;
    birdY += birdVY;
    pipeX -= 3;

    if (pipeX < -PIPE_W) {
      pipeX = PLAY_W;
      gapY = random(20, PLAY_H - GAP_H - 20);
      score++;
    }

    if (birdY < 0 || birdY > PLAY_H - BIRD_R) dead = true;

    if (pipeX < BIRD_X + BIRD_R && pipeX + PIPE_W > BIRD_X - BIRD_R) {
      if (birdY - BIRD_R < gapY || birdY + BIRD_R > gapY + GAP_H) dead = true;
    }

    if (dead && score > flappyHighScore) flappyHighScore = score;
  }

  void draw(BotState &bot) {
    gfx->fillScreen(GCOL_BG);

    // Play area border
    gfx->drawRoundRect(PLAY_X, PLAY_Y, PLAY_W, PLAY_H, 8, 0x4208);

    // Pipes
    int px = PLAY_X + pipeX;
    gfx->fillRect(px, PLAY_Y, PIPE_W, gapY, GCOL_FG);
    gfx->fillRect(px, PLAY_Y + gapY + GAP_H, PIPE_W, PLAY_H - gapY - GAP_H, GCOL_FG);

    // Bird
    int bx = PLAY_X + BIRD_X;
    int by = PLAY_Y + (int)birdY;
    gfx->fillCircle(bx, by, BIRD_R, GCOL_ACCENT);
    gfx->fillCircle(bx + 4, by - 2, 2, GCOL_BG); // eye

    // Score
    gfx->setTextColor(GCOL_FG);
    gfx->setTextSize(2);
    gfx->setCursor(PLAY_X + PLAY_W - 40, 8);
    gfx->print(score);

    if (dead) {
      gfx->fillRoundRect(40, 90, 160, 70, 10, GCOL_BG);
      gfx->drawRoundRect(40, 90, 160, 70, 10, GCOL_BAD);

      gfx->setTextColor(GCOL_BAD);
      gfx->setTextSize(2);
      gfx->setCursor(58, 100);
      gfx->print("GAME OVER");

      gfx->setTextColor(GCOL_FG);
      gfx->setTextSize(1);
      gfx->setCursor(60, 125);
      gfx->print("Score: ");
      gfx->print(score);

      gfx->setCursor(60, 140);
      gfx->print("Best: ");
      gfx->print(flappyHighScore);

      gfx->setCursor(48, 155);
      gfx->print("Tap S2 to restart");
    }

    bot.game.score = score;
    bot.game.highScore = flappyHighScore;
    bot.game.isDead = dead;
  }

  void run(BotState &bot) {
    update();

    if (touch2Pressed()) {
      if (dead) {
        reset();
      } else {
        birdVY = -4.5f;
      }
    }

    draw(bot);
  }
}

// ═══════════════════════════════════════════════════════════════════
//  ██  DINO RUN  ██
//  Original 128x64 logic scaled ~1.875x horizontal, ~2.8x vertical
//  into the 200x180 play area. Ground sits at GROUND_Y.
// ═══════════════════════════════════════════════════════════════════
namespace Dino {
  float dinoY, velY;
  bool  onGround, dead;
  int   obstX, obstH, score, gameSpeed;
  unsigned long lastUpdate;

  const int DINO_X = 30;     // dino x within play area
  const int DINO_W = 22;
  const int DINO_H = 30;
  const float GROUND_LEVEL = PLAY_H - DINO_H; // dino "feet" y when on ground

  void reset() {
    dinoY = GROUND_LEVEL;
    velY = 0;
    onGround = true;
    dead = false;
    obstX = PLAY_W;
    obstH = 24;
    score = 0;
    gameSpeed = 4;
    lastUpdate = millis();
  }

  void update() {
    if (dead) return;
    if (millis() - lastUpdate < 40) return;
    lastUpdate = millis();

    if (!onGround) {
      velY += 1.0f;
      dinoY += velY;
    }
    if (dinoY >= GROUND_LEVEL) {
      dinoY = GROUND_LEVEL;
      velY = 0;
      onGround = true;
    }

    obstX -= gameSpeed;
    if (obstX < -14) {
      obstX = PLAY_W + random(20, 100);
      obstH = random(18, 36);
      score++;
      if (score % 5 == 0 && gameSpeed < 10) gameSpeed++;
    }

    // Collision: obstacle horizontally overlaps dino, and dino top is below obstacle top
    if (obstX < DINO_X + DINO_W && obstX + 14 > DINO_X) {
      float dinoTop = dinoY;
      float dinoBottom = dinoY + DINO_H;
      float obstTop = PLAY_H - obstH;
      if (dinoBottom > obstTop) dead = true;
    }

    if (dead && score > dinoHighScore) dinoHighScore = score;
  }

  void draw(BotState &bot) {
    gfx->fillScreen(GCOL_BG);

    gfx->drawRoundRect(PLAY_X, PLAY_Y, PLAY_W, PLAY_H, 8, 0x4208);

    // Ground line
    gfx->drawLine(PLAY_X, GROUND_Y, PLAY_X + PLAY_W, GROUND_Y, GCOL_GROUND);

    // Dino
    int dx = PLAY_X + DINO_X;
    int dy = PLAY_Y + (int)dinoY;
    gfx->fillRoundRect(dx, dy, DINO_W, DINO_H, 4, GCOL_ACCENT);
    gfx->fillCircle(dx + DINO_W - 4, dy + 6, 2, GCOL_BG); // eye
    // legs
    gfx->fillRect(dx + 2, dy + DINO_H, 6, 6, GCOL_ACCENT);
    gfx->fillRect(dx + DINO_W - 8, dy + DINO_H, 6, 6, GCOL_ACCENT);

    // Obstacle (cactus)
    int ox = PLAY_X + obstX;
    gfx->fillRect(ox, PLAY_Y + PLAY_H - obstH, 14, obstH, GCOL_FG);

    // Score
    gfx->setTextColor(GCOL_FG);
    gfx->setTextSize(2);
    gfx->setCursor(PLAY_X + PLAY_W - 60, 8);
    gfx->print("SC:");
    gfx->print(score);

    if (dead) {
      gfx->fillRoundRect(40, 90, 160, 70, 10, GCOL_BG);
      gfx->drawRoundRect(40, 90, 160, 70, 10, GCOL_BAD);

      gfx->setTextColor(GCOL_BAD);
      gfx->setTextSize(2);
      gfx->setCursor(58, 100);
      gfx->print("GAME OVER");

      gfx->setTextColor(GCOL_FG);
      gfx->setTextSize(1);
      gfx->setCursor(60, 125);
      gfx->print("Score: ");
      gfx->print(score);

      gfx->setCursor(60, 140);
      gfx->print("Best: ");
      gfx->print(dinoHighScore);

      gfx->setCursor(48, 155);
      gfx->print("Tap S2 to restart");
    }

    bot.game.score = score;
    bot.game.highScore = dinoHighScore;
    bot.game.isDead = dead;
  }

  void run(BotState &bot) {
    update();

    if (touch2Pressed()) {
      if (dead) {
        reset();
      } else if (onGround) {
        velY = -8.5f;
        onGround = false;
      }
    }

    draw(bot);
  }
}

// ═══════════════════════════════════════════════════════════════════
//  ██  GAME ROUTER  ██
// ═══════════════════════════════════════════════════════════════════
static void launchGame(int g) {
  switch (g) {
    case 0: Flappy::reset(); break;
    case 1: Dino::reset();   break;
  }
}

static void runActiveGame(BotState &bot) {
  switch (bot.game.selectedGame) {
    case 0: Flappy::run(bot); break;
    case 1: Dino::run(bot);   break;
  }
}

// ═══════════════════════════════════════════════════════════════════
//  ██  PUBLIC API  ██
// ═══════════════════════════════════════════════════════════════════
void Game_init() {
  pinMode(PIN_TOUCH, INPUT_PULLDOWN);
  pinMode(PIN_TOUCH_GAME, INPUT_PULLDOWN);

  gaState = GA_MENU;
  flappyHighScore = 0;
  dinoHighScore = 0;
}

void Game_tick(BotState &bot) {
  bot.display.showGame = true;

  switch (gaState) {
    case GA_MENU: {
      menuNavigate(bot);
      drawMenu(bot);

      if (touch1Pressed()) {
        launchGame(bot.game.selectedGame);
        bot.game.isRunning = true;
        bot.game.isDead = false;
        gaState = GA_PLAYING;
      }

      // Hold Sensor 1 from menu → exit game mode entirely
      if (touch1LongPress()) {
        Mode_exitGame(bot);
        gaState = GA_MENU; // reset for next time game mode is entered
      }
      break;
    }

    case GA_PLAYING: {
      runActiveGame(bot);

      // Hold Sensor 1 during play → back to game selection menu
      if (touch1LongPress()) {
        bot.game.isRunning = false;
        gaState = GA_MENU;
      }
      break;
    }
  }
}
