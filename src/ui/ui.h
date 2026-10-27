// Display-Oberfläche (LVGL 9.3), board-unabhängig.
//
// Bekommt eine fertige Bildbeschreibung (abf::View aus lib/dispmodel) und zeichnet sie im
// Querformat 820 × 320 auf den aktiven LVGL-Bildschirm. Welche Hardware die Pixel ausgibt,
// entscheidet der Aufrufer (Firmware: Board-HAL, Simulator: Speicherpuffer → PNG).
// Nicht threadsicher: nur aus dem Thread aufrufen, der auch lv_timer_handler() ausführt.
#pragma once

#include <display_model.h>

namespace ui {

constexpr int32_t kWidth = 820;
constexpr int32_t kHeight = 320;

// Zeichnet neu, wenn sich die Beschreibung geändert hat. true = neu aufgebaut.
bool show(const abf::View &view);

// Erzwingt den Neuaufbau beim nächsten show() (z. B. nach Display-Neuinitialisierung)
void invalidate();

}  // namespace ui
