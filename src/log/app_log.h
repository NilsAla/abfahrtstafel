// Logging: jede Zeile geht auf Serial und in den Ringpuffer für /api/log.
// Threadsicher (Loop, async_tcp, eigene Tasks), aber nicht aus ISRs aufrufen.
#pragma once

#include <stdint.h>

#include <string>

namespace applog {

void begin();  // Ringpuffer anlegen (PSRAM bevorzugt). Davor geloggte Zeilen gehen nur auf Serial.

void write(char level, const char *tag, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

struct Chunk {
  std::string text;
  uint32_t last;
  bool gap;
};
Chunk readSince(uint32_t since);

}  // namespace applog

#define LOGE(tag, ...) applog::write('E', tag, __VA_ARGS__)
#define LOGW(tag, ...) applog::write('W', tag, __VA_ARGS__)
#define LOGI(tag, ...) applog::write('I', tag, __VA_ARGS__)
#define LOGD(tag, ...) applog::write('D', tag, __VA_ARGS__)
