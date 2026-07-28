// ============================================
// display_manager.h — INNO TableBot Stage 2
// GC9A01 Round TFT 240x240 (GOLDENMORNING_240x240)
// ============================================

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "bot_state.h"

// Init display hardware (SPI, GC9A01) and show splash screen
void Display_init();

// Main render function — call every loop, reads bot.display flags
void Display_render(BotState &bot);

#endif
