// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
#include "display_task.h"

#include <Arduino.h>
#include <boot_anim.h>
#include <display_model.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <time.h>
#include <time_berlin.h>

#include <algorithm>

#include "../board/board.h"
#include "../config/config_service.h"
#include "../log/app_log.h"
#include "../net/network.h"
#include "../ota/ota.h"
#include "../provider/departure_service.h"
#include "../ui/boot_scene.h"
#include "../ui/ui.h"

namespace display {

namespace {

constexpr uint32_t kUpdateMs = 500;           // Zustand abfragen; neu gezeichnet wird nur bei Änderungen
constexpr uint32_t kStackBytes = 16 * 1024;   // LVGL-Software-Rendering braucht etwas Stack
constexpr uint32_t kStripeLines = 20;         // Höhe des LVGL-Renderstreifens (Querformat-Zeilen)

bool sActive = false;

void flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px) {
  board::displayFlush(area->x1, area->y1, area->x2, area->y2, reinterpret_cast<const uint16_t *>(px));
  lv_display_flush_ready(disp);
}

abf::ViewInputs collect() {
  abf::ViewInputs in;
  in.cfg = config::get();
  in.wifi = net::state();
  in.apName = net::apName();
  in.hostname = net::hostname();
  in.ip = net::ip();
  in.setupRestart = net::setupRestart();
  in.now = time(nullptr);
  in.timeValid = abf::isTimePlausible(in.now);
  const ota::DisplayInfo up = ota::displayInfo();
  in.update = up.phase;
  in.updatePercent = up.percent;
  in.updateAvailable = up.available;
  departures::displayStops(in.stops, abf::rowsNeeded(in.cfg));
  in.uptimeS = millis() / 1000;
  return in;
}

// Bootanimation (BRANDING.md, Abschnitt 7): läuft nur hier im Display-Task, WLAN und Web-UI starten parallel.
// Bricht ab, sobald Abfahrten bereitstehen (frühestens nach der Einfahrt), spätestens nach 5,5 s.
void playBootAnimation(int &lastBacklight) {
  const abf::View first = abf::buildView(collect());
  const esp_reset_reason_t reason = esp_reset_reason();
  const bool cold = reason == ESP_RST_POWERON || reason == ESP_RST_EXT || reason == ESP_RST_USB;
  if (!abf::boot::shouldPlay(config::get().bootAnimation, cold, first.screen == abf::Screen::Setup,
                             first.backlight)) {
    return;
  }
  board::setBacklight(first.backlight);
  lastBacklight = first.backlight;
  ui::bootBegin(first.theme);  // ohne gültige Uhrzeit ist das Nacht

  const uint32_t t0 = millis();
  uint32_t frames = 0, slowest = 0, nextCheck = 0;
  bool ready = false;
  for (;;) {
    const uint32_t start = millis();
    const float t = (start - t0) / 1000.0f;
    if (start >= nextCheck) {  // Abfahrten schon da? Nicht in jedem Bild nachsehen
      ready = abf::buildView(collect()).screen == abf::Screen::Board;
      nextCheck = start + 250;
    }
    if (abf::boot::finished(t, ready)) break;
    if (t <= abf::boot::kOnceCut + 0.1f) {  // danach steht das Bild, nur noch warten
      ui::bootFrame(abf::boot::frameAt(t));
      lv_refr_now(nullptr);
      ++frames;
      slowest = std::max(slowest, millis() - start);
    }
    const uint32_t spent = millis() - start;
    vTaskDelay(pdMS_TO_TICKS(spent < 33 ? 33 - spent : 1));  // ~30 Bilder/s, anderen Tasks Luft lassen
  }
  ui::bootEnd();
  const uint32_t total = millis() - t0;
  LOGI("disp", "Bootanimation: %u Bilder bis %.1f s, langsamstes %u ms, Ende nach %u ms%s",
       static_cast<unsigned>(frames), static_cast<double>(abf::boot::kOnceCut), static_cast<unsigned>(slowest),
       static_cast<unsigned>(total), ready ? " (Abfahrten bereit)" : "");
}

void taskMain(void *) {
  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  // Streifen-Puffer im internen RAM (820 × 20 × 2 = 32 KB): LVGL rendert und das HAL dreht ohne PSRAM-Lesezugriffe.
  // Am Board: Vollbild-Puffer im PSRAM brauchte 250 ms pro gedrehtem Flush und ließ das Bild seitlich wackeln, weil der
  // RGB-DMA in der Zeit nicht schnell genug aus dem PSRAM nachgeladen wurde.
  const size_t bytes = static_cast<size_t>(board::kLogicalWidth) * kStripeLines * 2;
  void *buf = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!buf) {
    LOGE("disp", "Kein RAM für den Bildpuffer (%u Bytes)", static_cast<unsigned>(bytes));
    sActive = false;
    vTaskDelete(nullptr);
    return;
  }
  lv_display_t *disp = lv_display_create(board::kLogicalWidth, board::kLogicalHeight);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, buf, nullptr, static_cast<uint32_t>(bytes), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flush);
  LOGI("disp", "LVGL bereit, %ux%u", static_cast<unsigned>(board::kLogicalWidth),
       static_cast<unsigned>(board::kLogicalHeight));

  int lastBacklight = -1;
  playBootAnimation(lastBacklight);
  abf::Screen lastScreen = abf::Screen::Off;
  for (;;) {
    const abf::View view = abf::buildView(collect());
    if (ui::show(view) && view.screen != lastScreen) {
      LOGI("disp", "Bildschirm %d", static_cast<int>(view.screen));
      lastScreen = view.screen;
    }
    lv_timer_handler();  // zeichnet, falls nötig
    if (view.backlight != lastBacklight) {
      board::setBacklight(view.backlight);
      lastBacklight = view.backlight;
    }
    vTaskDelay(pdMS_TO_TICKS(kUpdateMs));
  }
}

}  // namespace

void begin() {
  if (!board::displayBegin()) {
    LOGI("disp", "Kein Display erkannt, Anzeige bleibt aus (%s)", board::displayError());
    return;
  }
  sActive = true;
#if defined(CONFIG_LCD_RGB_ISR_IRAM_SAFE) && defined(CONFIG_SPIRAM_XIP_FROM_PSRAM)
  LOGI("disp", "RGB-ISR IRAM-sicher, XIP aus PSRAM: Bild bleibt bei Flash-Zugriffen synchron");
#else
  LOGW("disp", "RGB-ISR nicht IRAM-sicher: Bildversatz nach Flash-Zugriffen möglich");
#endif
  const BaseType_t ok = xTaskCreatePinnedToCore(taskMain, "display", kStackBytes, nullptr, 1, nullptr, 1);
  if (ok != pdPASS) {
    sActive = false;
    LOGE("disp", "Task konnte nicht gestartet werden");
  }
}

bool active() { return sActive; }

}  // namespace display
