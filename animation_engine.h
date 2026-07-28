// ============================================
// animation_engine.h — INNO TableBot Stage 2
// Face animations for GC9A01 240x240 color TFT
// ============================================

#ifndef ANIMATION_ENGINE_H
#define ANIMATION_ENGINE_H

#include "bot_state.h"
#include <Arduino_GFX_Library.h>

// Shared display object, defined in display_manager.cpp
extern Arduino_GFX *gfx;

// Call once in setup(), after Display_init()
void Animation_init();

// Call every ~100ms from main loop — advances frame, blink timers etc.
void Anim_tick(FaceState &face);

// Draws the current face based on face.currentExpression
// cx, cy = center of face on screen
// eyeColor = color used for eyes/pupils (varies per mode)
// mouthColor = color used for mouth (only drawn if showMouth)
void drawFace(FaceState &face, int cx, int cy, uint16_t eyeColor, uint16_t mouthColor);

#endif
