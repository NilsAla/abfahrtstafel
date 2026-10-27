// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
#include "display_task.h"

#include <Arduino.h>
#include <display_model.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <time.h>
#include <time_berlin.h>

#include "../board/board.h"
#include "../config/config_service.h"
#include "../log/app_log.h"
#include "../net/network.h"
#include "../ota/ota.h"
#include "../provider/departure_service.h"
#include "../ui/ui.h"

namespace display {

namespace {

constexpr uint32_t kUpdateMs = 500;           // Zustand abfragen; neu gezeichnet wird nur bei Änderungen
constexpr uint32_t kStackBytes = 16 * 1024;   // LVGL-Software-Rendering braucht etwas Stack

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

void taskMain(void *) {
  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  // Ein Vollbild-Puffer im PSRAM (820 × 320 × 2 = 525 KB); das HAL kopiert gedreht ins Panel
  const size_t bytes = static_cast<size_t>(board::kLogicalWidth) * board::kLogicalHeight * 2;
  void *buf = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) {
    LOGE("disp", "Kein PSRAM für den Bildpuffer (%u Bytes)", static_cast<unsigned>(bytes));
    sActive = false;
    vTaskDelete(nullptr);
    return;
  }
  lv_display_t *disp = lv_display_create(board::kLogicalWidth, board::kLogicalHeight);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, buf, nullptr, static_cast<uint32_t>(bytes), LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(disp, flush);
  LOGI("disp", "LVGL bereit, %ux%u", static_cast<unsigned>(board::kLogicalWidth),
       static_cast<unsigned>(board::kLogicalHeight));

  int lastBacklight = -1;
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
    LOGI("disp", "Kein Display erkannt, Anzeige bleibt aus");
    return;
  }
  sActive = true;
  const BaseType_t ok = xTaskCreatePinnedToCore(taskMain, "display", kStackBytes, nullptr, 1, nullptr, 1);
  if (ok != pdPASS) {
    sActive = false;
    LOGE("disp", "Task konnte nicht gestartet werden");
  }
}

bool active() { return sActive; }

}  // namespace display
