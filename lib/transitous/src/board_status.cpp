#include "board_status.h"

namespace abf {

StatusResult evaluateStatus(const StatusInputs &in) {
  StatusResult r;
  if (!in.hasStops) {
    r.status = BoardStatus::NoStops;
    return r;
  }
  if (!in.timeValid) {
    r.status = BoardStatus::NoTime;
    return r;
  }
  r.offline = !in.wifiOnline;
  if (!in.everFetched) {
    if (!in.wifiOnline) {
      r.status = BoardStatus::NoWifi;
    } else {
      r.status = in.lastFetchFailed ? BoardStatus::ApiError : BoardStatus::Loading;
    }
    return r;
  }
  r.ageSeconds = in.now > in.lastSuccess ? in.now - in.lastSuccess : 0;
  r.stale = r.ageSeconds > kStaleAfterS;
  if (in.rows > 0) {
    r.status = BoardStatus::Ok;
  } else if (r.stale) {
    // Alte Daten sind aufgebraucht – Ursache zeigen statt „keine Abfahrten“
    r.status = in.wifiOnline ? BoardStatus::ApiError : BoardStatus::NoWifi;
  } else {
    r.status = BoardStatus::NoDepartures;
  }
  return r;
}

const char *boardStatusName(BoardStatus s) {
  switch (s) {
    case BoardStatus::NoStops:      return "no_stops";
    case BoardStatus::NoTime:       return "no_time";
    case BoardStatus::NoWifi:       return "no_wifi";
    case BoardStatus::Loading:      return "loading";
    case BoardStatus::ApiError:     return "api_error";
    case BoardStatus::NoDepartures: return "no_departures";
    case BoardStatus::Ok:           return "ok";
  }
  return "unknown";
}

const char *boardStatusText(BoardStatus s) {
  switch (s) {
    case BoardStatus::NoStops:      return "Noch keine Haltestelle eingerichtet.";
    case BoardStatus::NoTime:       return "Uhrzeit wird gestellt …";
    case BoardStatus::NoWifi:       return "Keine WLAN-Verbindung.";
    case BoardStatus::Loading:      return "Abfahrten werden geladen …";
    case BoardStatus::ApiError:     return "Fahrplandaten gerade nicht erreichbar.";
    case BoardStatus::NoDepartures: return "Zurzeit keine Abfahrten.";
    case BoardStatus::Ok:           return "";
  }
  return "";
}

void PollSchedule::success(uint32_t now) {
  started_ = true;
  failures_ = 0;
  delay_ = interval_;
  next_ = now + delay_;
}

void PollSchedule::failure(uint32_t now) {
  started_ = true;
  ++failures_;
  uint32_t d = kMinMs;
  for (uint32_t i = 1; i < failures_ && d < maxBackoff_; ++i) d *= 2;
  delay_ = d > maxBackoff_ ? maxBackoff_ : d;
  next_ = now + delay_;
}

}  // namespace abf
