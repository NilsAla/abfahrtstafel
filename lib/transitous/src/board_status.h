// Zustand der Abfahrtsanzeige (Klartext für Display/Web-UI) und Abruf-Takt mit Backoff.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace abf {

enum class BoardStatus {
  NoStops,       // noch nichts eingerichtet
  NoTime,        // Uhrzeit noch nicht gestellt → Minuten nicht berechenbar
  NoWifi,        // kein WLAN und noch nie Daten
  Loading,       // erster Abruf läuft
  ApiError,      // Abruf fehlgeschlagen und keine Daten
  NoDepartures,  // Daten da, aber nichts in Sicht
  Ok,
};

struct StatusInputs {
  bool hasStops = false;
  bool timeValid = false;
  bool wifiOnline = false;
  bool everFetched = false;     // mindestens ein erfolgreicher Abruf
  bool lastFetchFailed = false;
  int64_t lastSuccess = 0;      // Unix-Zeit des letzten erfolgreichen Abrufs
  int64_t now = 0;
  size_t rows = 0;              // sichtbare Abfahrten nach Aufbereitung
};

struct StatusResult {
  BoardStatus status = BoardStatus::Loading;
  bool stale = false;      // Daten älter als kStaleAfterS → Datenalter anzeigen
  int64_t ageSeconds = 0;  // nur bei everFetched
  bool offline = false;    // Grund für veraltete Daten: WLAN weg (sonst API)
};

constexpr int64_t kStaleAfterS = 3 * 60;

StatusResult evaluateStatus(const StatusInputs &in);
const char *boardStatusName(BoardStatus s);   // Schlüssel für die API
const char *boardStatusText(BoardStatus s);   // Klartext

// Abruf-Takt: normal alle intervalMs (≥ 30 s laut Nutzungsregeln), nach Fehlern exponentiell
// 30 s, 60 s, 120 s … bis maxBackoffMs.
class PollSchedule {
 public:
  explicit PollSchedule(uint32_t intervalMs = 60000, uint32_t maxBackoffMs = 600000)
      : interval_(intervalMs < kMinMs ? kMinMs : intervalMs), maxBackoff_(maxBackoffMs) {}

  bool due(uint32_t now) const { return !started_ || static_cast<int32_t>(now - next_) >= 0; }
  void success(uint32_t now);
  void failure(uint32_t now);
  void trigger(uint32_t now) { next_ = now; started_ = true; }  // z. B. nach Konfigurationsänderung
  uint32_t failures() const { return failures_; }
  uint32_t nextDelay() const { return delay_; }

  static constexpr uint32_t kMinMs = 30000;

 private:
  uint32_t interval_;
  uint32_t maxBackoff_;
  uint32_t next_ = 0;
  uint32_t delay_ = 0;
  uint32_t failures_ = 0;
  bool started_ = false;
};

}  // namespace abf
