// Firmware-Update aus GitHub Releases, Selbsttest und Rollback.
// Ablauflogik liegt in lib/ota (abf::UpdateFlow), hier nur HTTPS, Flash und Tasks.
#pragma once

#include <ArduinoJson.h>
#include <display_model.h>

namespace ota {

void begin();  // Selbsttest vorbereiten, Ergebnis des letzten Updates ermitteln
void loop();   // Selbsttest-Entscheidung, täglicher Check

bool requestCheck();    // false, wenn schon etwas läuft
bool requestInstall();  // false, wenn kein Angebot da ist oder etwas läuft
bool busy();            // Prüfen, Download, Verifizieren oder Neustart läuft

struct DisplayInfo {
  abf::UpdatePhase phase = abf::UpdatePhase::None;  // nur Installation, nicht die Suche
  uint8_t percent = 0;
  bool available = false;  // neue Version gefunden (Hinweis in der Statuszeile)
};
DisplayInfo displayInfo();

void buildStatus(JsonObject out);  // Inhalt von /api/update

}  // namespace ota
