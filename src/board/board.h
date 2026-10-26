// Board-HAL: einzige Stelle mit board-spezifischem Code.
// Display-Init/Flush und Backlight folgen in M0.5.
#pragma once

#include <stdint.h>

namespace board {

// Native Displaygeometrie (hochkant). Querformat entsteht per Software-Rotation.
constexpr uint16_t kNativeWidth = 320;
constexpr uint16_t kNativeHeight = 820;

// BOOT-Taster (Strapping-Pin, low-aktiv). Konflikt mit Display-CS klärt M0.5.
constexpr int kBootButtonPin = 0;

void initButtons();
bool bootButtonPressed();

}  // namespace board
