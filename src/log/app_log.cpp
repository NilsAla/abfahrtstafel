#include "app_log.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <log_ring.h>
#include <stdarg.h>
#include <time.h>
#include <time_berlin.h>

#include <mutex>

namespace applog {

namespace {

constexpr size_t kRingPsram = 32 * 1024;
constexpr size_t kRingInternal = 4 * 1024;

std::mutex sMutex;
abf::LogRing *sRing = nullptr;

}  // namespace

void begin() {
  size_t cap = kRingPsram;
  char *buf = static_cast<char *>(heap_caps_malloc(cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!buf) {
    cap = kRingInternal;
    buf = static_cast<char *>(malloc(cap));
  }
  if (!buf) return;
  std::lock_guard<std::mutex> lock(sMutex);
  sRing = new abf::LogRing(buf, cap);
}

void write(char level, const char *tag, const char *fmt, ...) {
  char line[256];
  const uint32_t ms = millis();
  int n = snprintf(line, sizeof(line), "%5lu.%03lu ", static_cast<unsigned long>(ms / 1000),
                   static_cast<unsigned long>(ms % 1000));

  const time_t now = time(nullptr);
  if (abf::isTimePlausible(now)) {
    struct tm tm;
    localtime_r(&now, &tm);
    n += strftime(line + n, sizeof(line) - n, "%H:%M:%S ", &tm);
  } else {
    n += snprintf(line + n, sizeof(line) - n, "--:--:-- ");
  }
  n += snprintf(line + n, sizeof(line) - n, "%c %s: ", level, tag);

  va_list args;
  va_start(args, fmt);
  const int m = vsnprintf(line + n, sizeof(line) - n, fmt, args);
  va_end(args);
  size_t len = m < 0 ? n : n + m;
  if (len > sizeof(line) - 1) len = sizeof(line) - 1;

  std::lock_guard<std::mutex> lock(sMutex);
  Serial.write(reinterpret_cast<const uint8_t *>(line), len);
  Serial.write('\n');
  if (sRing) sRing->append(line, len);
}

Chunk readSince(uint32_t since) {
  std::lock_guard<std::mutex> lock(sMutex);
  if (!sRing) return {std::string(), 0, false};
  abf::LogRing::ReadResult r = sRing->readSince(since);
  return {std::move(r.text), r.last, r.gap};
}

}  // namespace applog
