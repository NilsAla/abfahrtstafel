// JSON-Abbildung der Konfiguration für /api/config.
#pragma once

#include <ArduinoJson.h>

#include <string>
#include <vector>

#include "app_config.h"

namespace abf {

struct FieldError {
  std::string field;
  std::string message;  // Klartext für die Web-UI
};

void configToJson(const Config &cfg, JsonObject out);

// Übernimmt die im JSON vorhandenen Felder (Teil-Update). Unbekannte Felder werden ignoriert.
// Alles-oder-nichts: Bei einem Fehler bleibt cfg unverändert und errors ist gefüllt.
bool applyConfigJson(JsonObjectConst in, Config &cfg, std::vector<FieldError> &errors);

}  // namespace abf
