// Abfahrtstafel – Einstiegspunkt
// M0: bootet und loggt Basisinformationen. Funktionen folgen ab M1.

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

#ifndef FW_VERSION
#define FW_VERSION "0.0.0-dev"
#endif

static const char *resetReasonText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "Power-on";
    case ESP_RST_EXT:       return "Externer Reset";
    case ESP_RST_SW:        return "Software-Reset";
    case ESP_RST_PANIC:     return "Panic";
    case ESP_RST_INT_WDT:   return "Interrupt-Watchdog";
    case ESP_RST_TASK_WDT:  return "Task-Watchdog";
    case ESP_RST_WDT:       return "Watchdog";
    case ESP_RST_DEEPSLEEP: return "Deep-Sleep";
    case ESP_RST_BROWNOUT:  return "Brownout";
    case ESP_RST_SDIO:      return "SDIO";
    case ESP_RST_USB:       return "USB";
    case ESP_RST_JTAG:      return "JTAG";
    default:                return "Unbekannt";
  }
}

static void logBootInfo() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  Serial.printf("\n=== Abfahrtstafel %s ===\n", FW_VERSION);
  Serial.printf("Reset-Grund : %s\n", resetReasonText(esp_reset_reason()));
  Serial.printf("Partition   : %s @ 0x%06lx\n", running ? running->label : "?",
                running ? (unsigned long)running->address : 0UL);
  Serial.printf("Flash       : %lu MB\n", (unsigned long)(ESP.getFlashChipSize() >> 20));
  Serial.printf("Heap        : %lu frei / %lu gesamt\n", (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getHeapSize());
  Serial.printf("PSRAM       : %lu frei / %lu gesamt\n", (unsigned long)ESP.getFreePsram(),
                (unsigned long)ESP.getPsramSize());
}

void setup() {
  Serial.begin(115200);
  // USB-CDC braucht nach dem Boot einen Moment, bis der Host verbunden ist.
  const uint32_t start = millis();
  while (!Serial && millis() - start < 2000) {
    delay(10);
  }
  logBootInfo();
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 10000) {
    last = millis();
    Serial.printf("[%lus] Heap %lu, PSRAM %lu\n", (unsigned long)(millis() / 1000),
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram());
  }
  delay(50);
}
