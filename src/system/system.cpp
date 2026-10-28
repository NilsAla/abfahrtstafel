// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
#include "system.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <hold_button.h>
#include <nvs_flash.h>

#include <atomic>

#include "../board/board.h"
#include "../log/app_log.h"

namespace sys {

namespace {

constexpr uint32_t kFactoryResetHoldMs = 10 * 1000;

abf::HoldButton sBoot(kFactoryResetHoldMs);
std::atomic<bool> sRestartPending{false};
std::atomic<uint32_t> sRestartRequested{0};
std::atomic<uint32_t> sRestartDelay{0};
std::atomic<bool> sFactoryResetPending{false};

[[noreturn]] void factoryReset() {
  LOGW("reset", "Werksreset: lösche alle Einstellungen und WLAN-Zugangsdaten");
  WiFi.disconnect(true, true);
  delay(200);
  // Löscht die komplette NVS-Partition (Konfiguration, WLAN, PHY-Kalibrierung).
  // otadata liegt in einer eigenen Partition → die laufende Firmware bleibt aktiv.
  const esp_err_t err = nvs_flash_erase();
  if (err != ESP_OK) LOGE("reset", "nvs_flash_erase: %s", esp_err_to_name(err));
  delay(200);
  esp_restart();
}

}  // namespace

const char *resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "Stromversorgung eingeschaltet";
    case ESP_RST_EXT:       return "Externer Reset";
    case ESP_RST_SW:        return "Neustart durch Software";
    case ESP_RST_PANIC:     return "Absturz (Panic)";
    case ESP_RST_INT_WDT:   return "Interrupt-Watchdog";
    case ESP_RST_TASK_WDT:  return "Task-Watchdog";
    case ESP_RST_WDT:       return "Watchdog";
    case ESP_RST_DEEPSLEEP: return "Aufwachen aus Deep-Sleep";
    case ESP_RST_BROWNOUT:  return "Unterspannung (Netzteil/Kabel prüfen)";
    case ESP_RST_SDIO:      return "SDIO";
    case ESP_RST_USB:       return "USB";
    case ESP_RST_JTAG:      return "JTAG";
    default:                return "Unbekannt";
  }
}

void watchdogBegin(uint32_t timeoutMs) {
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = timeoutMs;
  cfg.idle_core_mask = 1 << 0;  // Idle-Task von Core 0 (WLAN/TCP) weiter überwachen
  cfg.trigger_panic = true;     // Panic → Neustart, Reset-Grund „Task-Watchdog“
  esp_err_t err = esp_task_wdt_reconfigure(&cfg);
  if (err == ESP_ERR_INVALID_STATE) err = esp_task_wdt_init(&cfg);
  if (err != ESP_OK) LOGE("wdt", "Konfiguration fehlgeschlagen: %s", esp_err_to_name(err));

  err = esp_task_wdt_add(nullptr);
  if (err != ESP_OK && err != ESP_ERR_INVALID_ARG) {  // INVALID_ARG = schon angemeldet
    LOGE("wdt", "Loop-Task nicht angemeldet: %s", esp_err_to_name(err));
    return;
  }
  LOGI("wdt", "Task-Watchdog aktiv (%lu s)", static_cast<unsigned long>(timeoutMs / 1000));
}

void watchdogFeed() { esp_task_wdt_reset(); }

void scheduleRestart(uint32_t delayMs) {
  sRestartDelay = delayMs;
  sRestartRequested = millis();
  sRestartPending = true;
}

void scheduleFactoryReset(uint32_t delayMs) {
  sFactoryResetPending = true;
  scheduleRestart(delayMs);
}

void loop() {
  const uint32_t now = millis();
  switch (sBoot.update(board::bootButtonPressed(), now)) {
    case abf::HoldButton::Event::Pressed:
      LOGI("reset", "BOOT gedrückt – 10 s halten setzt das Gerät zurück");
      break;
    case abf::HoldButton::Event::Released:
      LOGI("reset", "BOOT losgelassen");
      break;
    case abf::HoldButton::Event::HoldReached:
      factoryReset();
    case abf::HoldButton::Event::None:
      break;
  }

  if (sRestartPending && now - sRestartRequested >= sRestartDelay) {
    if (sFactoryResetPending) factoryReset();
    LOGI("sys", "Neustart");
    delay(100);
    esp_restart();
  }
}

}  // namespace sys
