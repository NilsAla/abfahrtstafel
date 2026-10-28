#include "heap_monitor.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "../log/app_log.h"
#include "../ota/ota.h"
#include "system.h"

namespace heapmon {

namespace {

constexpr uint32_t kCheckMs = 5 * 1000;
constexpr uint32_t kCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;

abf::HeapWatch sWatch;
uint32_t sLast = 0;
bool sRestartPending = false;  // fällig, wartet aber auf ein laufendes Update

}  // namespace

size_t largestBlock() { return heap_caps_get_largest_free_block(kCaps); }

abf::HeapLevel level() { return sWatch.level(); }

void loop() {
  const uint32_t now = millis();
  if (now - sLast < kCheckMs) return;
  sLast = now;
  const size_t freeBytes = heap_caps_get_free_size(kCaps);
  const size_t block = largestBlock();
  switch (sWatch.update(freeBytes, block, now)) {
    case abf::HeapWatch::Event::BecameLow:
      LOGW("sys", "Heap knapp: %lu frei, größter Block %lu", static_cast<unsigned long>(freeBytes),
           static_cast<unsigned long>(block));
      break;
    case abf::HeapWatch::Event::BecameCritical:
      LOGE("sys", "Heap kritisch: %lu frei, größter Block %lu – Neustart, wenn das 10 min anhält",
           static_cast<unsigned long>(freeBytes), static_cast<unsigned long>(block));
      break;
    case abf::HeapWatch::Event::Recovered:
      LOGI("sys", "Heap wieder ausreichend: %lu frei", static_cast<unsigned long>(freeBytes));
      break;
    case abf::HeapWatch::Event::RestartDue:
      sRestartPending = true;
      break;
    case abf::HeapWatch::Event::None:
      break;
  }
  if (sWatch.level() != abf::HeapLevel::Critical) sRestartPending = false;  // erholt: neue Phase zählt von vorn
  // Nicht mitten im Update neu starten: Download/Flash hat Vorrang, danach startet das Gerät ohnehin neu
  if (sRestartPending && !ota::busy()) {
    sRestartPending = false;
    LOGE("sys", "Heap seit 10 min kritisch, Neustart");
    sys::scheduleRestart(1000);
  }
}

}  // namespace heapmon
