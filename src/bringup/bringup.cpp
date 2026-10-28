// env:bringup – Display-Testbild direkt in den Panel-Framebuffer, ohne LVGL und ohne restlichen Stack.
// Ablauf und Erwartung: docs/ROADMAP.md, „Offen am Board“ → Bring-up.
//
// Testbild (native Hochkant-Koordinaten, 320 × 820, y = 0 ist „oben“ des Panels):
//   - 1-px-Rahmen weiß an allen vier Kanten
//   - oben: weißer Pfeil, zeigt zu y = 0; gelbes Quadrat in der Ecke x = 0 (links)
//   - darunter vier Balken Rot, Grün, Blau, Weiß, jeweils von links dunkel nach rechts hell
#include <Arduino.h>

#include "../board/board.h"

namespace {

constexpr uint16_t kW = board::kNativeWidth;
constexpr uint16_t kH = board::kNativeHeight;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kYellow = 0xFFE0;

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void fillRect(uint16_t *fb, int x, int y, int w, int h, uint16_t c) {
  for (int yy = y; yy < y + h; ++yy) {
    for (int xx = x; xx < x + w; ++xx) fb[yy * kW + xx] = c;
  }
}

void drawTestImage(uint16_t *fb) {
  fillRect(fb, 0, 0, kW, kH, 0);

  // Pfeil nach oben: Spitze bei y = 12, Dreieck bis y = 92, Schaft bis y = 150
  const int cx = kW / 2;
  for (int y = 12; y < 92; ++y) {
    const int half = (y - 12);
    fillRect(fb, cx - half, y, 2 * half + 1, 1, kWhite);
  }
  fillRect(fb, cx - 20, 92, 41, 58, kWhite);

  // Marker links oben (Spiegelung erkennen)
  fillRect(fb, 8, 8, 40, 40, kYellow);

  // Vier Verläufe ab y = 164, je 163 px hoch, 2 px Abstand zum Rahmen
  const int top = 164;
  const int bandH = (kH - top - 4) / 4;
  for (int band = 0; band < 4; ++band) {
    for (int x = 2; x < kW - 2; ++x) {
      const uint8_t v = static_cast<uint8_t>((x - 2) * 255 / (kW - 5));
      uint16_t c = 0;
      switch (band) {
        case 0: c = rgb565(v, 0, 0); break;
        case 1: c = rgb565(0, v, 0); break;
        case 2: c = rgb565(0, 0, v); break;
        default: c = rgb565(v, v, v); break;
      }
      fillRect(fb, x, top + band * bandH, 1, bandH - 2, c);
    }
  }

  // 1-px-Rahmen zuletzt
  fillRect(fb, 0, 0, kW, 1, kWhite);
  fillRect(fb, 0, kH - 1, kW, 1, kWhite);
  fillRect(fb, 0, 0, 1, kH, kWhite);
  fillRect(fb, kW - 1, 0, 1, kH, kWhite);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t start = millis();
  while (!Serial && millis() - start < 3000) delay(10);
  Serial.printf("[bringup] Start, PSRAM frei %u, Heap frei %u\n", ESP.getFreePsram(), ESP.getFreeHeap());

  board::initButtons();
  const uint32_t t0 = millis();
  if (!board::displayBegin()) {
    Serial.printf("[bringup] displayBegin fehlgeschlagen: %s\n", board::displayError());
    return;
  }
  Serial.printf("[bringup] displayBegin ok in %lu ms, PSRAM frei %u, Heap frei %u\n",
                static_cast<unsigned long>(millis() - t0), ESP.getFreePsram(), ESP.getFreeHeap());

  drawTestImage(board::nativeFramebuffer());
  Serial.println("[bringup] Testbild gezeichnet");

  // Backlight-Rampe: hoch in 10-%-Schritten, dann runter bis 0, dann 100 %
  for (int p = 0; p <= 100; p += 10) {
    board::setBacklight(p);
    Serial.printf("[bringup] Backlight %d %%\n", p);
    delay(700);
  }
  for (int p = 90; p >= 0; p -= 10) {
    board::setBacklight(p);
    Serial.printf("[bringup] Backlight %d %%\n", p);
    delay(700);
  }
  board::setBacklight(100);
  Serial.println("[bringup] Backlight 100 %, Rampe fertig");
}

void loop() {
  // BOOT-Taster nach der Display-Init (GPIO0 = CS) muss weiter funktionieren
  static bool last = false;
  const bool pressed = board::bootButtonPressed();
  if (pressed != last) {
    Serial.printf("[bringup] BOOT %s\n", pressed ? "gedrückt" : "losgelassen");
    last = pressed;
  }
  static uint32_t beat = 0;
  if (millis() - beat > 10000) {
    beat = millis();
    Serial.printf("[bringup] läuft, %lu s\n", static_cast<unsigned long>(millis() / 1000));
  }
  delay(20);
}
