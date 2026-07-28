// ============================================
// animation_engine.cpp — INNO TableBot Stage 2
// Face animations for GC9A01 240x240 color TFT
//
// All coordinates designed around a 240x240 round
// display, face centered around (cx, cy) passed in
// from display_manager.
//
// MOUTH RULE:
// face.showMouth must be true ONLY when
// face.currentExpression == FACE_SPEAKING
// (driven by display_manager based on STATE_SPEAKING)
// ============================================

#include "animation_engine.h"

// ── EYE GEOMETRY ──────────────────────────────
#define EYE_W        34   // eye width
#define EYE_H        40   // eye height (open)
#define EYE_H_HALF   18   // eye height (half closed / focus)
#define EYE_GAP      26   // gap between eye centers from cx
#define PUPIL_R      8

#define BLINK_INTERVAL_MS   3500
#define BLINK_DURATION_MS   120
#define MORPH_INTERVAL_MS   100

// Background color used to "erase" previous frame.
// Must match the background drawn by display_manager
// for each mode. We redraw full screen each tick in
// display_manager, so here we only clear the face area.
#define FACE_CLEAR_BG  0x0000

// ── INTERNAL STATE ─────────────────────────────
static int pupilOffsetX = 0;     // -1, 0, 1 → left/center/right
static int mouthFrame = 0;       // 0..3 mouth animation frame

// ══════════════════════════════════════════════
void Animation_init() {
  pupilOffsetX = 0;
  mouthFrame = 0;
}

// ── BLINK / MORPH TIMER UPDATE ────────────────
void Anim_tick(FaceState &face) {
  unsigned long now = millis();

  // Expression morph (instant swap — simple state machine,
  // smooth morphing can be added later if needed)
  if (face.currentExpression != face.targetExpression) {
    if (now - face.lastMorphMs >= MORPH_INTERVAL_MS) {
      face.currentExpression = face.targetExpression;
      face.lastMorphMs = now;
    }
  }

  // Blinking — only in ACTIVE / HAPPY expression
  if (face.currentExpression == FACE_HAPPY) {
    if (!face.isBlinking && now - face.lastBlinkMs >= BLINK_INTERVAL_MS) {
      face.isBlinking = true;
      face.lastBlinkMs = now;
    } else if (face.isBlinking && now - face.lastBlinkMs >= BLINK_DURATION_MS) {
      face.isBlinking = false;
      face.lastBlinkMs = now;
    }
  } else {
    face.isBlinking = false;
  }

  // Pupil drift — only in ACTIVE / HAPPY expression
  // Subtle left/right look every ~4 seconds
  static unsigned long lastLookMs = 0;
  if (face.currentExpression == FACE_HAPPY) {
    if (now - lastLookMs >= 4000) {
      pupilOffsetX = (pupilOffsetX == 0) ? ((now / 4000) % 2 == 0 ? -1 : 1) : 0;
      lastLookMs = now;
    }
  } else {
    pupilOffsetX = 0;
  }

  // Mouth animation frame — only when SPEAKING
  face.showMouth = (face.currentExpression == FACE_SPEAKING);
  if (face.showMouth) {
    if (now % 200 < 100) {
      mouthFrame = (mouthFrame + 1) % 4;
    }
  } else {
    mouthFrame = 0;
  }

  face.frame++;
}

// ── EYE DRAW HELPERS ───────────────────────────
static void drawEyeOpen(int x, int y, uint16_t color, int pupilShiftX) {
  // Rounded rect eye
  gfx->fillRoundRect(x - EYE_W / 2, y - EYE_H / 2, EYE_W, EYE_H, 10, color);
  // Pupil (dark circle)
  gfx->fillCircle(x + pupilShiftX, y, PUPIL_R, FACE_CLEAR_BG);
}

static void drawEyeClosed(int x, int y, uint16_t color) {
  // Simple horizontal line for closed eye
  gfx->fillRoundRect(x - EYE_W / 2, y - 3, EYE_W, 6, 3, color);
}

static void drawEyeHalf(int x, int y, uint16_t color, int pupilShiftX) {
  gfx->fillRoundRect(x - EYE_W / 2, y - EYE_H_HALF / 2, EYE_W, EYE_H_HALF, 8, color);
  gfx->fillCircle(x + pupilShiftX, y, PUPIL_R - 2, FACE_CLEAR_BG);
}

static void drawEyeWide(int x, int y, uint16_t color) {
  // Larger, more circular "alert" eye
  gfx->fillRoundRect(x - EYE_W / 2 - 4, y - EYE_H / 2 - 6, EYE_W + 8, EYE_H + 12, 14, color);
  gfx->fillCircle(x, y, PUPIL_R + 2, FACE_CLEAR_BG);
}

static void drawEyeLookUp(int x, int y, uint16_t color) {
  gfx->fillRoundRect(x - EYE_W / 2, y - EYE_H / 2, EYE_W, EYE_H, 10, color);
  // Pupil shifted up
  gfx->fillCircle(x, y - 10, PUPIL_R, FACE_CLEAR_BG);
}

// ── MOUTH DRAW HELPERS ─────────────────────────
static void drawMouth(int cx, int cy, uint16_t color, int frame) {
  // 4-frame open/close mouth animation
  int w = 50;
  int h;
  switch (frame) {
    case 0: h = 6;  break; // closed
    case 1: h = 16; break; // opening
    case 2: h = 28; break; // open
    case 3: h = 16; break; // closing
    default: h = 6; break;
  }
  gfx->fillRoundRect(cx - w / 2, cy - h / 2, w, h, h / 3, color);
}

// ── EXPRESSION DRAW FUNCTIONS ──────────────────
static void happyFace(FaceState &face, int cx, int cy, uint16_t eyeColor) {
  int leftX  = cx - EYE_GAP;
  int rightX = cx + EYE_GAP;

  if (face.isBlinking) {
    drawEyeClosed(leftX, cy, eyeColor);
    drawEyeClosed(rightX, cy, eyeColor);
  } else {
    drawEyeOpen(leftX, cy, eyeColor, pupilOffsetX * 4);
    drawEyeOpen(rightX, cy, eyeColor, pupilOffsetX * 4);
  }
}

static void focusedFace(FaceState &face, int cx, int cy, uint16_t eyeColor) {
  int leftX  = cx - EYE_GAP;
  int rightX = cx + EYE_GAP;

  drawEyeHalf(leftX, cy, eyeColor, 0);
  drawEyeHalf(rightX, cy, eyeColor, 0);
}

static void listeningFace(FaceState &face, int cx, int cy, uint16_t eyeColor) {
  int leftX  = cx - EYE_GAP;
  int rightX = cx + EYE_GAP;

  drawEyeWide(leftX, cy, eyeColor);
  drawEyeWide(rightX, cy, eyeColor);
}

static void thinkingFace(FaceState &face, int cx, int cy, uint16_t eyeColor) {
  int leftX  = cx - EYE_GAP;
  int rightX = cx + EYE_GAP;

  drawEyeLookUp(leftX, cy, eyeColor);
  drawEyeLookUp(rightX, cy, eyeColor);
}

static void speakingFace(FaceState &face, int cx, int cy, uint16_t eyeColor, uint16_t mouthColor) {
  int leftX  = cx - EYE_GAP;
  int rightX = cx + EYE_GAP;

  drawEyeOpen(leftX, cy, eyeColor, 0);
  drawEyeOpen(rightX, cy, eyeColor, 0);

  if (face.showMouth) {
    drawMouth(cx, cy + 45, mouthColor, mouthFrame);
  }
}

static void alarmFace(FaceState &face, int cx, int cy, uint16_t eyeColor) {
  int leftX  = cx - EYE_GAP;
  int rightX = cx + EYE_GAP;

  drawEyeWide(leftX, cy, eyeColor);
  drawEyeWide(rightX, cy, eyeColor);
}

static void sadFace(FaceState &face, int cx, int cy, uint16_t eyeColor) {
  int leftX  = cx - EYE_GAP;
  int rightX = cx + EYE_GAP;

  // Drooping eyes — rounded rect rotated visually via shape (simple version)
  gfx->fillRoundRect(leftX - EYE_W / 2, cy - 6, EYE_W, 12, 6, eyeColor);
  gfx->fillRoundRect(rightX - EYE_W / 2, cy - 6, EYE_W, 12, 6, eyeColor);

  // Small downward "tear drop" accents for sad look
  gfx->fillCircle(leftX, cy + 16, 4, eyeColor);
  gfx->fillCircle(rightX, cy + 16, 4, eyeColor);
}

// ── MAIN DRAW FUNCTION ─────────────────────────
void drawFace(FaceState &face, int cx, int cy, uint16_t eyeColor, uint16_t mouthColor) {
  switch (face.currentExpression) {
    case FACE_HAPPY:
      happyFace(face, cx, cy, eyeColor);
      break;
    case FACE_FOCUSED:
      focusedFace(face, cx, cy, eyeColor);
      break;
    case FACE_LISTENING:
      listeningFace(face, cx, cy, eyeColor);
      break;
    case FACE_THINKING:
      thinkingFace(face, cx, cy, eyeColor);
      break;
    case FACE_SPEAKING:
      speakingFace(face, cx, cy, eyeColor, mouthColor);
      break;
    case FACE_ALARM:
      alarmFace(face, cx, cy, eyeColor);
      break;
    case FACE_SAD:
      sadFace(face, cx, cy, eyeColor);
      break;
  }
}
