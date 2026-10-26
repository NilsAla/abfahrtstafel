// Board-HAL: einzige Stelle mit board-spezifischem Code.
// Interface wird in M0.5 ausgearbeitet (Display-Init/Flush, Backlight, Taster).
#pragma once

#include <stdint.h>

namespace board {

// Native Displaygeometrie (hochkant). Querformat entsteht per Software-Rotation.
constexpr uint16_t kNativeWidth = 320;
constexpr uint16_t kNativeHeight = 820;

}  // namespace board
