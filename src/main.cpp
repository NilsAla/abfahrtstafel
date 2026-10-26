// Abfahrtstafel – Einstiegspunkt
// M1: Logging, Konfiguration, WLAN-Einrichtung, Web-UI, Watchdog, Factory Reset.
// M2: Firmware-Update per Knopfdruck, Selbsttest und Rollback.

#include <Arduino.h>
#include <esp_ota_ops.h>

#include "board/board.h"
#include "config/config_service.h"
#include "log/app_log.h"
#include "net/network.h"
#include "ota/ota.h"
#include "system/system.h"
#include "version.h"

namespace {

constexpr uint32_t kWatchdogMs = 30 * 1000;
constexpr uint32_t kHeartbeatMs = 10 * 60 * 1000;

void logBootInfo() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  LOGI("boot", "=== Abfahrtstafel %s ===", FW_VERSION);
  LOGI("boot", "Reset-Grund: %s", sys::resetReasonText());
  LOGI("boot", "Partition: %s @ 0x%06lx", running ? running->label : "?",
       running ? static_cast<unsigned long>(running->address) : 0UL);
  LOGI("boot", "Flash %lu MB, Heap %lu/%lu, PSRAM %lu/%lu",
       static_cast<unsigned long>(ESP.getFlashChipSize() >> 20), static_cast<unsigned long>(ESP.getFreeHeap()),
       static_cast<unsigned long>(ESP.getHeapSize()), static_cast<unsigned long>(ESP.getFreePsram()),
       static_cast<unsigned long>(ESP.getPsramSize()));
}

void heartbeat() {
  static uint32_t last = 0;
  if (millis() - last < kHeartbeatMs) return;
  last = millis();
  LOGI("sys", "Heap %lu (min %lu), PSRAM %lu", static_cast<unsigned long>(ESP.getFreeHeap()),
       static_cast<unsigned long>(ESP.getMinFreeHeap()), static_cast<unsigned long>(ESP.getFreePsram()));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  // USB-CDC braucht nach dem Boot einen Moment, bis der Host verbunden ist.
  const uint32_t start = millis();
  while (!Serial && millis() - start < 2000) {
    delay(10);
  }

  applog::begin();
  logBootInfo();
  sys::watchdogBegin(kWatchdogMs);
  board::initButtons();
  config::begin();
  net::begin(config::get().hostname);
  ota::begin();
}

void loop() {
  sys::watchdogFeed();
  sys::loop();
  net::loop();
  ota::loop();
  heartbeat();
  delay(10);
}
