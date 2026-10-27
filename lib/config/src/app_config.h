// Gerätekonfiguration: Datenmodell, Validierung, NVS-Schema und Migration.
// Board-unabhängig; der Zugriff auf NVS läuft über das KvStore-Interface.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include <transit_types.h>

namespace abf {

// Schema-Version der Konfiguration in NVS.
// Regeln (siehe docs/ARCHITEKTUR.md):
//  - Neue Felder bekommen neue Keys mit Default → kein Schema-Bump nötig.
//  - Bump nur, wenn sich die Bedeutung bestehender Keys ändert; dann Migrationsschritt ergänzen.
//  - Ältere Firmware (Rollback) muss ein neueres Schema lesen können: Keys nie umdeuten, sondern neu anlegen.
constexpr uint32_t kConfigSchema = 1;

// Haltestellenname in der Kopfzeile der Tafel. Werte sind NVS-Inhalt: nie umnummerieren.
enum class StopNameStyle : uint32_t {
  Normal = 0,  // klein und grau, wie bisher
  Large = 1,   // groß, höhere Kopfzeile
  Off = 2,     // nur Datum und Uhr (z. B. Etikett am Gehäuse); bei 2 Haltestellen trotzdem sichtbar
};

struct Config {
  std::string hostname = "abfahrt";  // mDNS/DHCP-Name → http://abfahrt.local
  uint32_t brightness = 80;          // Prozent, 5..100
  bool nightEnabled = false;         // Nachtmodus (Umsetzung ab M4)
  uint32_t nightStart = 22 * 60;     // Minuten ab Mitternacht, Ortszeit
  uint32_t nightEnd = 6 * 60;
  uint32_t nightBrightness = 0;      // Prozent, 0 = Display aus
  bool updateCheck = true;           // täglicher Update-Hinweis (ab M2)
  StopNameStyle stopName = StopNameStyle::Normal;
  std::vector<StopConfig> stops;     // max. 2 Haltestellen (ab M3), in NVS als JSON
};

constexpr uint32_t kBrightnessMin = 5;
constexpr uint32_t kBrightnessMax = 100;
constexpr uint32_t kMinutesPerDay = 24 * 60;

// Schlüssel-Wert-Speicher (NVS). Keys max. 15 Zeichen.
class KvStore {
 public:
  virtual ~KvStore() = default;
  virtual bool getU32(const char *key, uint32_t &out) const = 0;
  virtual bool putU32(const char *key, uint32_t value) = 0;
  virtual bool getStr(const char *key, std::string &out) const = 0;
  virtual bool putStr(const char *key, const std::string &value) = 0;
};

enum class MigrationOutcome {
  Fresh,     // kein Schema gefunden → Erststart, Schema geschrieben
  Current,   // Schema aktuell, nichts zu tun
  Migrated,  // ältere Version auf aktuelle gehoben
  Newer,     // neueres Schema (Rollback): unverändert lassen, bekannte Keys lesen
  Failed,    // Schreiben fehlgeschlagen
};

struct MigrationResult {
  MigrationOutcome outcome;
  uint32_t from;  // gefundenes Schema (0 = keins)
  uint32_t to;    // Schema nach der Migration
};

MigrationResult migrateConfig(KvStore &store);

// Liest alle bekannten Keys. Fehlende oder ungültige Werte → Default.
// invalidCount (optional) zählt verworfene Werte.
Config loadConfig(const KvStore &store, int *invalidCount = nullptr);

// Schreibt alle Felder. Schema-Key wird nicht angefasst (macht migrateConfig).
bool saveConfig(KvStore &store, const Config &cfg);

// Hostname: 1..32 Zeichen, a-z, 0-9, '-', nicht am Anfang/Ende.
bool isValidHostname(const std::string &name);
std::string normalizeHostname(const std::string &name);  // trimmt, Kleinbuchstaben

// StopNameStyle ↔ "normal" | "large" | "off" (Web-UI)
const char *stopNameStyleKey(StopNameStyle style);
bool parseStopNameStyle(const std::string &key, StopNameStyle &out);

// "HH:MM" ↔ Minuten ab Mitternacht
bool parseHhMm(const std::string &text, uint32_t &minutes);
std::string formatHhMm(uint32_t minutes);

}  // namespace abf
