// Board-HAL: einzige Stelle mit board-spezifischem Code (Waveshare ESP32-S3-LCD-3.16, Pins in docs/HARDWARE.md).
#pragma once

#include <stdint.h>

namespace board {

// Native Displaygeometrie (hochkant). Querformat entsteht per Software-Rotation.
constexpr uint16_t kNativeWidth = 320;
constexpr uint16_t kNativeHeight = 820;

// BOOT-Taster (Strapping-Pin, low-aktiv). GPIO0 ist zugleich Display-CS (3-Wire-SPI): Er wird nur während
// displayBegin() als Ausgang genutzt und danach wieder Eingang mit Pull-up (wie in der Waveshare-Firmware).
constexpr int kBootButtonPin = 0;

void initButtons();
bool bootButtonPressed();

// --- Display ----------------------------------------------------------------
// Die UI zeichnet im Querformat (kLogicalWidth × kLogicalHeight, RGB565). Das HAL dreht beim
// Flush in die native Hochkant-Ausrichtung des Panels.
constexpr uint16_t kLogicalWidth = kNativeHeight;   // 820
constexpr uint16_t kLogicalHeight = kNativeWidth;   // 320

bool displayBegin();       // false = kein Display (z. B. DevKit ohne Panel) → UI bleibt aus
const char *displayError();  // Klartext zum letzten Fehlschlag von displayBegin(), sonst ""
void displayFlush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint16_t *pixels);  // Querformat-Koordinaten
void setBacklight(uint8_t percent);  // 0 = aus
void displayResync();  // regelmäßig aufrufen: behebt seitlichen Bildversatz nach PSRAM-Engpässen

// Nur für env:bringup: Panel-Framebuffer in nativer Ausrichtung (kNativeWidth × kNativeHeight, RGB565).
// Schreibzugriffe sind sofort sichtbar (Bounce-Buffer liest über den Cache).
uint16_t *nativeFramebuffer();

}  // namespace board
