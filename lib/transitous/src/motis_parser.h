// Parser für die MOTIS-2-API von Transitous (stoptimes v6, geocode v1).
// Reine Funktionen über einem JSON-Puffer; Filter-Dokumente halten den Speicherbedarf klein
// (20 Abfahrten ≈ 45 KB Antwort, davon wird nur ein Bruchteil gebraucht).
#pragma once

#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "transit_types.h"

namespace abf {

// "2026-10-26T11:21:00Z", optional mit Sekundenbruchteilen oder Offset ±HH:MM. false bei Formatfehlern.
bool parseIsoTime(const char *text, int64_t &epoch);

uint8_t modeFromMotis(const char *mode);  // "REGIONAL_RAIL" → kModeTrain …
int32_t parseHexColor(const char *text);  // "ea9d22" → 0xea9d22, sonst -1

struct StopTimesResult {
  std::vector<Departure> departures;
  std::string nextPageCursor;  // für „weitere laden“
};

// alloc: optional (Firmware: PSRAM). false bei kaputtem JSON oder fehlendem stopTimes-Array.
bool parseStopTimes(const char *json, size_t len, StopTimesResult &out, ArduinoJson::Allocator *alloc = nullptr);

bool parseGeocode(const char *json, size_t len, std::vector<StopMatch> &out, ArduinoJson::Allocator *alloc = nullptr);

}  // namespace abf
