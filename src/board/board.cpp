// Board-HAL für Waveshare ESP32-S3-LCD-3.16 (ST7701, RGB565 über esp_lcd-RGB-Panel).
// Pins, Timings und Init-Sequenz aus der Waveshare-Demo (LVGL-9.3-Variante), Abgleich mit Schaltplan V1 und
// Rev1.1 in docs/HARDWARE.md. Befunde am Board: docs/ROADMAP.md, „Offen am Board“.
#include "board.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_system.h>
#include <rom/ets_sys.h>

#include "st7701_init.h"

namespace board {

namespace {

// 3-Wire-SPI (nur für die Init-Sequenz, 9 Bit: D/C + 8 Datenbits, Mode 0)
constexpr int kLcdCs = 0;  // = BOOT, siehe board.h
constexpr int kLcdSck = 2;
constexpr int kLcdSda = 1;
constexpr int kLcdReset = 16;

// RGB-Schnittstelle
constexpr int kPclk = 41;
constexpr int kDe = 40;
constexpr int kVsync = 39;
constexpr int kHsync = 38;
// Datenleitungen D0..D15 = B0..B4, G0..G5, R0..R4 (RGB565, niederwertigstes Bit zuerst)
constexpr int kData[16] = {21, 5, 45, 48, 47,       // B0..B4
                           14, 13, 12, 11, 10, 9,   // G0..G5
                           17, 46, 3, 8, 18};       // R0..R4
constexpr uint32_t kPclkHz = 18 * 1000 * 1000;      // ≈ 52 Hz Bildwiederholrate

// Backlight: Boost-Wandler AP3032, BL_PWM speist über RC in den Feedback-Pin → invertiert:
// Duty 0 = volle Helligkeit, Duty 255 = aus (Waveshare: LCD_PWM_MODE_x = 0xFF - x).
constexpr int kBacklight = 6;
constexpr uint32_t kBacklightHz = 50 * 1000;
constexpr uint8_t kBacklightBits = 8;

// Querformat → Hochformat. Panel-Oberkante (y = 0) ist die Seite gegenüber USB-C, x = 0 links (am Board
// bestimmt, M0.5). Gehäuse: USB-C links → obere Kante des Querformats = linke Panelkante (x = 0) → false.
// true wäre USB-C rechts.
constexpr bool kRotateClockwise = false;

esp_lcd_panel_handle_t sPanel = nullptr;
uint16_t *sFb = nullptr;
const char *sError = "";
bool sBacklightReady = false;

void spiWrite9(bool isData, uint8_t value) {
  digitalWrite(kLcdCs, LOW);
  ets_delay_us(1);
  const uint16_t word = (isData ? 0x100 : 0) | value;
  for (int bit = 8; bit >= 0; --bit) {
    digitalWrite(kLcdSck, LOW);
    digitalWrite(kLcdSda, (word >> bit) & 1);
    ets_delay_us(1);
    digitalWrite(kLcdSck, HIGH);  // ST7701 übernimmt bei steigender Flanke
    ets_delay_us(1);
  }
  digitalWrite(kLcdSck, LOW);
  digitalWrite(kLcdSda, LOW);
  ets_delay_us(1);
  digitalWrite(kLcdCs, HIGH);
  ets_delay_us(1);
}

void sendInitSequence() {
  pinMode(kLcdCs, OUTPUT);
  digitalWrite(kLcdCs, HIGH);
  pinMode(kLcdSck, OUTPUT);
  digitalWrite(kLcdSck, LOW);
  pinMode(kLcdSda, OUTPUT);
  digitalWrite(kLcdSda, LOW);

  pinMode(kLcdReset, OUTPUT);
  digitalWrite(kLcdReset, LOW);
  delay(10);
  digitalWrite(kLcdReset, HIGH);
  delay(120);

  for (const st7701::InitCmd &c : st7701::kInit) {
    spiWrite9(false, c.cmd);
    for (uint8_t i = 0; i < c.len; ++i) spiWrite9(true, c.data[i]);
    if (c.delayMs) delay(c.delayMs);
  }

  // GPIO0 zurück an den BOOT-Taster. Ohne SCK-Flanken ignoriert das Panel einen CS-Pegelwechsel.
  pinMode(kLcdCs, INPUT_PULLUP);
}

// Vor esp_restart(): Panel-DMA stoppen. Läuft er weiter, hängt der Neustart und der RTC-Watchdog greift
// (am Board: Reset-Grund „Watchdog“ statt „Software“, auch nach OTA).
void shutdownPanel() {
  if (sBacklightReady) ledcWrite(kBacklight, (1u << kBacklightBits) - 1);
  if (sPanel) {
    esp_lcd_panel_del(sPanel);
    sPanel = nullptr;
    sFb = nullptr;
  }
}

bool fail(const char *msg) {
  sError = msg;
  return false;
}

}  // namespace

void initButtons() { pinMode(kBootButtonPin, INPUT_PULLUP); }

bool bootButtonPressed() { return digitalRead(kBootButtonPin) == LOW; }

bool displayBegin() {
  if (sPanel) return true;
  setBacklight(0);  // kein Bildmüll während der Init
  sendInitSequence();

  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = kPclkHz;
  cfg.timings.h_res = kNativeWidth;
  cfg.timings.v_res = kNativeHeight;
  cfg.timings.hsync_pulse_width = 6;
  cfg.timings.hsync_back_porch = 30;
  cfg.timings.hsync_front_porch = 30;
  cfg.timings.vsync_pulse_width = 40;
  cfg.timings.vsync_back_porch = 20;
  cfg.timings.vsync_front_porch = 20;
  cfg.timings.flags.pclk_active_neg = 0;  // Demo: Daten bei steigender PCLK-Flanke
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  cfg.num_fbs = 1;
  // Bounce-Buffer im internen RAM: Bild bleibt stabil, während Flash geschrieben wird (OTA) und der
  // PSRAM-Cache kurz aus ist. Demo: 10 Zeilen → mit WLAN leichtes seitliches Wackeln (WLAN-/LWIP-Puffer liegen im
  // PSRAM). 41 Zeilen (2 × 26 KB) puffern das ab, am Board ruhig. Die Zeilenzahl muss 820 teilen.
  cfg.bounce_buffer_size_px = 41 * kNativeWidth;
  cfg.dma_burst_size = 64;
  cfg.hsync_gpio_num = kHsync;
  cfg.vsync_gpio_num = kVsync;
  cfg.de_gpio_num = kDe;
  cfg.pclk_gpio_num = kPclk;
  cfg.disp_gpio_num = -1;
  for (int i = 0; i < 16; ++i) cfg.data_gpio_nums[i] = kData[i];
  cfg.flags.fb_in_psram = 1;

  if (esp_lcd_new_rgb_panel(&cfg, &sPanel) != ESP_OK) {
    sPanel = nullptr;
    return fail("RGB-Panel anlegen fehlgeschlagen (PSRAM/DMA)");
  }
  if (esp_lcd_panel_reset(sPanel) != ESP_OK || esp_lcd_panel_init(sPanel) != ESP_OK) {
    return fail("RGB-Panel starten fehlgeschlagen");
  }
  void *fb = nullptr;
  if (esp_lcd_rgb_panel_get_frame_buffer(sPanel, 1, &fb) != ESP_OK || !fb) {
    return fail("Framebuffer nicht verfügbar");
  }
  sFb = static_cast<uint16_t *>(fb);
  memset(sFb, 0, static_cast<size_t>(kNativeWidth) * kNativeHeight * 2);
  esp_register_shutdown_handler(shutdownPanel);
  sError = "";
  return true;
}

const char *displayError() { return sError; }

uint16_t *nativeFramebuffer() { return sFb; }

void displayFlush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint16_t *pixels) {
  if (!sFb) return;
  const int32_t w = x2 - x1 + 1;
  // Zeilenweise im Ziel schreiben (zusammenhängend im PSRAM), Quelle spaltenweise lesen.
  // Querformat (lx, ly) → nativ: im Uhrzeigersinn (nx, ny) = (319 - ly, lx), sonst (ly, 819 - lx).
  for (int32_t lx = x1; lx <= x2; ++lx) {
    const int32_t ny = kRotateClockwise ? lx : (kNativeHeight - 1 - lx);
    uint16_t *row = sFb + static_cast<size_t>(ny) * kNativeWidth;
    const uint16_t *src = pixels + (lx - x1);
    for (int32_t ly = y1; ly <= y2; ++ly, src += w) {
      const int32_t nx = kRotateClockwise ? (kNativeWidth - 1 - ly) : ly;
      row[nx] = *src;
    }
  }
}

void setBacklight(uint8_t percent) {
  if (percent > 100) percent = 100;
  if (!sBacklightReady) {
    sBacklightReady = ledcAttach(kBacklight, kBacklightHz, kBacklightBits);
    if (!sBacklightReady) return;
  }
  const uint32_t max = (1u << kBacklightBits) - 1;
  ledcWrite(kBacklight, max - (max * percent + 50) / 100);
}

}  // namespace board
