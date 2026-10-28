// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
#include "coredump.h"

#include <esp_core_dump.h>
#include <esp_partition.h>
#include <stdio.h>

#include <mutex>

#include "../log/app_log.h"

namespace coredump {

namespace {

std::mutex sMutex;
bool sChecked = false;
bool sPresent = false;
size_t sAddr = 0;  // absolute Flash-Adresse des Images
size_t sSize = 0;

const esp_partition_t *partition() {
  static const esp_partition_t *p =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
  return p;
}

// Leere Partition (0xFF) oder kaputtes Image → kein Bericht. Ergebnis wird bis zum Löschen gemerkt.
void refresh() {
  if (sChecked) return;
  sChecked = true;
  sPresent = esp_core_dump_image_check() == ESP_OK && esp_core_dump_image_get(&sAddr, &sSize) == ESP_OK &&
             sSize > 0 && partition();
}

}  // namespace

void begin() {
  if (!partition()) {
    LOGW("crash", "Keine Coredump-Partition gefunden");
    return;
  }
  if (!present()) return;
  esp_core_dump_summary_t s;
  if (esp_core_dump_get_summary(&s) == ESP_OK) {
    LOGW("crash", "Absturzbericht vorhanden: Task \"%s\", PC 0x%08lx, %lu Bytes – Download im Reiter „Protokoll“",
         s.exc_task, static_cast<unsigned long>(s.exc_pc), static_cast<unsigned long>(sSize));
  } else {
    LOGW("crash", "Absturzbericht vorhanden (%lu Bytes), Zusammenfassung nicht lesbar", static_cast<unsigned long>(sSize));
  }
}

bool present() {
  std::lock_guard<std::mutex> lock(sMutex);
  refresh();
  return sPresent;
}

size_t size() {
  std::lock_guard<std::mutex> lock(sMutex);
  refresh();
  return sPresent ? sSize : 0;
}

void buildInfo(JsonObject out) {
  std::lock_guard<std::mutex> lock(sMutex);
  refresh();
  out["present"] = sPresent;
  if (!sPresent) return;
  out["size"] = sSize;
  esp_core_dump_summary_t s;
  if (esp_core_dump_get_summary(&s) != ESP_OK) return;
  out["task"] = s.exc_task;
  char reason[64];
  if (esp_core_dump_get_panic_reason(reason, sizeof(reason)) == ESP_OK) out["reason"] = reason;
  char hex[11];
  snprintf(hex, sizeof(hex), "0x%08lx", static_cast<unsigned long>(s.exc_pc));
  out["pc"] = hex;
  // Backtrace als Adressliste, passt direkt in addr2line bzw. den Exception-Decoder
  JsonArray bt = out["backtrace"].to<JsonArray>();
  for (uint32_t i = 0; i < s.exc_bt_info.depth && i < sizeof(s.exc_bt_info.bt) / sizeof(s.exc_bt_info.bt[0]); ++i) {
    snprintf(hex, sizeof(hex), "0x%08lx", static_cast<unsigned long>(s.exc_bt_info.bt[i]));
    bt.add(hex);
  }
  out["backtrace_corrupted"] = s.exc_bt_info.corrupted;
  // ELF-SHA-256 (gekürzt) der abgestürzten Firmware: zeigt, welches firmware.elf zum Auswerten passt
  out["elf_sha256"] = reinterpret_cast<const char *>(s.app_elf_sha256);
}

bool read(size_t offset, uint8_t *buf, size_t len) {
  const esp_partition_t *p = partition();
  size_t addr, total;
  {
    std::lock_guard<std::mutex> lock(sMutex);
    refresh();
    if (!sPresent || !p) return false;
    addr = sAddr;
    total = sSize;
  }
  if (offset + len > total) return false;
  return esp_partition_read(p, addr - p->address + offset, buf, len) == ESP_OK;
}

bool erase() {
  std::lock_guard<std::mutex> lock(sMutex);
  const esp_err_t err = esp_core_dump_image_erase();
  sChecked = false;
  if (err != ESP_OK) {
    LOGE("crash", "Absturzbericht löschen: %s", esp_err_to_name(err));
    return false;
  }
  LOGI("crash", "Absturzbericht gelöscht");
  return true;
}

}  // namespace coredump
