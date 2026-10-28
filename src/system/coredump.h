// Absturzbericht (ESP-IDF-Coredump im ELF-Format in der Partition `coredump`) für die Web-UI.
// Auswerten am PC: esp-coredump info_corefile -c coredump.elf firmware.elf (ELF aus dem Release derselben Version).
#pragma once

#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

namespace coredump {

void begin();  // beim Start: vorhandenen Bericht ins Log schreiben

bool present();
void buildInfo(JsonObject out);  // Inhalt von /api/coredump/info

// Rohdaten lesen (für den Download); false bei Lesefehler
size_t size();
bool read(size_t offset, uint8_t *buf, size_t len);

bool erase();

}  // namespace coredump
