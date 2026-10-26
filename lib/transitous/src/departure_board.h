// Aufbereitung der Rohdaten für Anzeige und Richtungsauswahl.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "transit_types.h"

namespace abf {

// Ziel kürzen: Klammerzusätze weg, "Hauptbahnhof" → "Hbf", ", Bahnhof" am Ende weg.
// "Münster(Westf) Hbf" und "Münster Hauptbahnhof" werden beide zu "Münster Hbf".
std::string shortenHeadsign(const std::string &headsign);

struct BoardRow {
  std::string line;
  std::string to;     // gekürztes Ziel
  std::string track;
  uint8_t mode = kModeOther;
  int32_t color = -1;
  int32_t textColor = -1;
  int64_t when = 0;        // maßgebliche Zeit (Ausfall: Plan, sonst Prognose)
  int64_t scheduled = 0;
  int32_t delayMin = 0;    // nur mit Echtzeit sinnvoll
  int32_t minutes = 0;     // bis when, abgerundet
  bool realtime = false;
  bool cancelled = false;
};

// Filtert nach Konfiguration, blendet Verpasstes aus (inkl. Fußweg), ordnet Ausfälle nach Planzeit ein,
// entfernt Dubletten aus mehreren Datenfeeds und sortiert. Wird bei jeder Anzeige mit aktueller Zeit
// neu berechnet, damit auch alte Daten (API-Ausfall) korrekt weiterlaufen.
std::vector<BoardRow> prepareDepartures(const std::vector<Departure> &in, const StopConfig &cfg, int64_t now,
                                        size_t maxRows);

bool matchesSelection(const Departure &d, const StopConfig &cfg);

// Für die Richtungsauswahl in der Web-UI: Linie+Richtung zusammengefasst, nach Ziel gruppiert.
struct DirectionOption {
  std::string routeId;
  uint8_t direction = kDirectionUnknown;
  std::string line;
  std::string to;  // häufigstes gekürztes Ziel
  uint8_t mode = kModeOther;
  uint32_t count = 0;
};

struct DirectionGroup {
  std::string to;
  uint32_t count = 0;
  std::vector<DirectionOption> options;
};

std::vector<DirectionGroup> groupDirections(const std::vector<Departure> &in);

}  // namespace abf
