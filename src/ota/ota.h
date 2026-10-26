// Firmware-Update aus GitHub Releases, Selbsttest und Rollback.
// Ablauflogik liegt in lib/ota (abf::UpdateFlow), hier nur HTTPS, Flash und Tasks.
#pragma once

#include <ArduinoJson.h>

namespace ota {

void begin();  // Selbsttest vorbereiten, Ergebnis des letzten Updates ermitteln
void loop();   // Selbsttest-Entscheidung, täglicher Check

bool requestCheck();    // false, wenn schon etwas läuft
bool requestInstall();  // false, wenn kein Angebot da ist oder etwas läuft
bool busy();            // für die UI-Schicht (M4: „Update läuft“)

void buildStatus(JsonObject out);  // Inhalt von /api/update

}  // namespace ota
