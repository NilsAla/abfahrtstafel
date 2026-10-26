// Konfiguration zur Laufzeit: NVS-Anbindung (Preferences) und threadsicherer Zugriff.
#pragma once

#include <app_config.h>
#include <config_json.h>

#include <vector>

namespace config {

void begin();  // Migration + Laden, loggt das Ergebnis

abf::Config get();

struct UpdateResult {
  bool ok = false;
  bool saveFailed = false;
  bool restartRequired = false;  // z. B. Hostname geändert
  std::vector<abf::FieldError> errors;
  abf::Config cfg;
};

UpdateResult applyJson(JsonObjectConst in);

}  // namespace config
