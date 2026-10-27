// Board-HAL: einzige Stelle mit board-spezifischem Code.
// Display-Init/Flush und Backlight implementiert M0.5; bis dahin melden die Funktionen „kein Display“.
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

// --- Display (M0.5) ---------------------------------------------------------
// Die UI zeichnet im Querformat (kLogicalWidth × kLogicalHeight, RGB565). Das HAL dreht beim
// Flush in die native Hochkant-Ausrichtung des Panels.
constexpr uint16_t kLogicalWidth = kNativeHeight;   // 820
constexpr uint16_t kLogicalHeight = kNativeWidth;   // 320

bool displayBegin();       // false = kein Display (z. B. DevKit ohne Panel) → UI bleibt aus
void displayFlush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint16_t *pixels);  // Querformat-Koordinaten
void setBacklight(uint8_t percent);  // 0 = aus

}  // namespace board
