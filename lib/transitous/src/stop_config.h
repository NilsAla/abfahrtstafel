// JSON-Abbildung der Haltestellen-Konfiguration (für /api/config und NVS-Key "stops").
//
// [{"id":"…","name":"Emsdetten, Bahnhof","town":"Emsdetten","filter":"selected","modes":["train","bus"],
//   "select":[{"route":"de-VBN_34533_2","dir":0,"line":"RE15","to":"Münster Hbf"}],"walk":3}]
#pragma once

#include <ArduinoJson.h>

#include <string>
#include <vector>

#include "transit_types.h"

namespace abf {

void stopsToJson(const std::vector<StopConfig> &stops, JsonArray out);

// Prüft vollständig; bei Fehler bleibt out unverändert, error enthält Klartext.
bool stopsFromJson(JsonArrayConst in, std::vector<StopConfig> &out, std::string &error);

std::string stopsToString(const std::vector<StopConfig> &stops);
bool stopsFromString(const std::string &json, std::vector<StopConfig> &out);

constexpr size_t kMaxStopsJson = 3500;  // NVS-Strings dürfen max. ~4000 Bytes haben

}  // namespace abf
