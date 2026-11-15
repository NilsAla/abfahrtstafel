// wannbus-Farben als Code (Quelle: docs/branding/BRANDING.md, Abschnitt 2). Board-unabhängig, ohne LVGL.
// Werte als 0xRRGGBB; die UI wandelt sie selbst um (z. B. lv_color_hex). Gegenstück für die Web-UI: web/tokens.css.
// Bei Änderungen beide Dateien und den Test in test/test_branding anpassen.
#pragma once

#include <stdint.h>

namespace abf {
namespace brand {

// Markenfarben (Logo, Akzente). Gelb nur im Logo, Türkis einzige Aktionsfarbe.
constexpr uint32_t kNavy = 0x1B1F3B;
constexpr uint32_t kYellow = 0xFFC72C;
constexpr uint32_t kCoral = 0xFF4F5E;
constexpr uint32_t kTeal = 0x1FC8B5;
constexpr uint32_t kCream = 0xFFF6E5;

struct ThemeTokens {
  uint32_t bg;          // Hintergrund Web-UI
  uint32_t bgDisplay;   // Hintergrund Display
  uint32_t bgRest;      // Betriebsruhe
  uint32_t card;
  uint32_t cardHot;     // Zeile 1 auf dem Display
  uint32_t border;
  uint32_t text;
  uint32_t muted;
  uint32_t tealText;    // Türkis für Text (Kontrast ≥ 4,5:1)
  uint32_t coralText;   // Koralle für Text (Kontrast ≥ 4,5:1)
  uint32_t tealBig;     // Minuten, große Zahlen
  uint32_t coralBig;
  uint32_t badgeBg;
  uint32_t badgeText;
};

constexpr ThemeTokens kNight = {
    0x14162E, 0x0E1024, 0x0A0B1A, 0x1B1F3B, 0x163A44, 0x2E3466, 0xFFF6E5,
    0xA9ABC9, 0x3DDCC9, 0xFF6B78, 0x1FC8B5, 0xFF4F5E, 0xFFF6E5, 0x1B1F3B,
};

constexpr ThemeTokens kDay = {
    0xEEF7FC, 0xEAF4FA, 0xE1ECF4, 0xFFFFFF, 0xDDF5F1, 0xCFE3EE, 0x1B1F3B,
    0x4A5075, 0x0A7D71, 0xC81F33, 0x0A8F80, 0xE02D3C, 0x1B1F3B, 0xFFF6E5,
};

}  // namespace brand
}  // namespace abf
