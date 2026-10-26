// Datenmodell für Abfahrten, Haltestellen und Filter – unabhängig vom Datenanbieter.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace abf {

// Verkehrsmittel-Kategorien (Bitmaske). Feiner als nötig wäre verwirrend für die Endnutzerin.
enum ModeBit : uint8_t {
  kModeTrain = 1 << 0,   // Fern-, Regional-, S-Bahn
  kModeSubway = 1 << 1,  // U-Bahn/Metro
  kModeTram = 1 << 2,
  kModeBus = 1 << 3,     // inkl. Fernbus
  kModeFerry = 1 << 4,
  kModeOther = 1 << 5,
};
constexpr uint8_t kModeAll = 0x3f;

const char *modeKey(uint8_t bit);              // "train", "bus", …
uint8_t modeFromKey(const std::string &key);   // 0 = unbekannt

constexpr uint8_t kDirectionUnknown = 255;

struct Departure {
  std::string routeId;    // eindeutige Linie im Datensatz
  uint8_t direction = kDirectionUnknown;  // GTFS directionId 0/1
  std::string line;       // routeShortName, z. B. "RE15"
  std::string headsign;   // Ziel wie geliefert
  std::string track;      // Gleis/Steig, kann leer sein
  uint8_t mode = kModeOther;
  int32_t color = -1;      // 0xRRGGBB, -1 = keine Angabe
  int32_t textColor = -1;
  int64_t scheduled = 0;   // Unix-Zeit (UTC)
  int64_t expected = 0;    // Echtzeit-Prognose, sonst = scheduled
  bool realtime = false;
  bool cancelled = false;  // Halt oder ganze Fahrt fällt aus
};

struct StopMatch {
  std::string id;
  std::string name;
  std::string town;     // Gemeinde (areas adminLevel 8)
  std::string region;   // Kreis (adminLevel 6), sonst Bundesland
  std::string country;  // "DE", "CH", …
  uint8_t modes = 0;
};

// Gewählte Linie + Richtung
struct RouteDirection {
  std::string routeId;
  uint8_t direction = kDirectionUnknown;
  std::string line;  // nur Anzeige
  std::string to;    // nur Anzeige (gekürztes Ziel)
};

enum class StopFilter { All, Selected };

struct StopConfig {
  std::string id;
  std::string name;
  std::string town;
  StopFilter filter = StopFilter::All;
  uint8_t modes = kModeAll;            // nur bei filter == All
  std::vector<RouteDirection> select;  // nur bei filter == Selected; neue Linien bleiben ausgeblendet
  uint32_t walkMinutes = 0;            // Abfahrten, die man nicht mehr erreicht, ausblenden
};

constexpr size_t kMaxStops = 2;
constexpr size_t kMaxSelections = 24;
constexpr uint32_t kMaxWalkMinutes = 30;

}  // namespace abf
