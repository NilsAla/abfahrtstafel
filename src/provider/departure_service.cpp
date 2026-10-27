// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
#include "departure_service.h"

#include <Arduino.h>
#include <board_status.h>
#include <date_de.h>
#include <departure_board.h>
#include <stop_config.h>
#include <time.h>
#include <time_berlin.h>

#include <atomic>
#include <mutex>

#include "../config/config_service.h"
#include "../log/app_log.h"
#include "../net/network.h"
#include "../ota/ota.h"
#include "provider.h"

namespace departures {

namespace {

constexpr uint32_t kPollIntervalMs = 60 * 1000;
constexpr uint32_t kCountAll = 20;
constexpr uint32_t kCountSelected = 40;   // gefiltert bleiben sonst zu wenige übrig
constexpr uint32_t kCountDirections = 100;
constexpr uint32_t kLookupCacheMs = 5 * 60 * 1000;
constexpr size_t kMinQueryLength = 2;

struct Slot {
  abf::StopConfig cfg;
  std::vector<abf::Departure> data;
  abf::PollSchedule poll{kPollIntervalMs};
  int64_t lastSuccess = 0;
  bool everFetched = false;
  bool lastFailed = false;
};

enum class LookupState { Idle, Pending, Done, Error };

struct Lookup {
  LookupState state = LookupState::Idle;
  std::string key;
  uint32_t doneAt = 0;
};

struct SearchLookup : Lookup {
  std::vector<abf::StopMatch> results;
};

struct DirectionsLookup : Lookup {
  bool more = false;
  std::vector<abf::Departure> acc;
  std::string cursor;
};

std::mutex sMutex;
std::vector<Slot> sSlots;
uint32_t sConfigRevision = UINT32_MAX;
SearchLookup sSearch;
DirectionsLookup sDirections;
std::atomic<int64_t> sLastSuccess{0};
TaskHandle_t sTask = nullptr;

const char *lookupStateName(LookupState s) {
  switch (s) {
    case LookupState::Idle:    return "idle";
    case LookupState::Pending: return "pending";
    case LookupState::Done:    return "done";
    case LookupState::Error:   return "error";
  }
  return "idle";
}

void wake() {
  if (sTask) xTaskNotifyGive(sTask);
}

std::string trim(const std::string &s) {
  const size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

std::string localHhMm(int64_t epoch) {
  const time_t t = static_cast<time_t>(epoch);
  struct tm tm;
  localtime_r(&t, &tm);
  char buf[8];
  strftime(buf, sizeof(buf), "%H:%M", &tm);
  return buf;
}

void colorToJson(JsonObject o, const char *key, int32_t color) {
  if (color < 0) {
    o[key] = nullptr;
    return;
  }
  char buf[8];
  snprintf(buf, sizeof(buf), "#%06lx", static_cast<unsigned long>(color));
  o[key] = buf;
}

// Konfiguration übernehmen; Daten bleiben erhalten, wenn die Haltestelle gleich bleibt
void syncConfig() {
  const uint32_t rev = config::revision();
  if (rev == sConfigRevision) return;
  const abf::Config cfg = config::get();
  const uint32_t now = millis();
  std::lock_guard<std::mutex> lock(sMutex);
  std::vector<Slot> next;
  for (const abf::StopConfig &stop : cfg.stops) {
    Slot slot;
    for (Slot &old : sSlots) {
      if (old.cfg.id == stop.id) {
        slot = std::move(old);
        break;
      }
    }
    slot.cfg = stop;
    slot.poll.trigger(now);  // Filter oder Anzahl können sich geändert haben
    next.push_back(std::move(slot));
  }
  sSlots = std::move(next);
  sConfigRevision = rev;
  LOGI("dep", "%u Haltestelle(n) konfiguriert", static_cast<unsigned>(sSlots.size()));
}

void runSearch() {
  std::string query;
  {
    std::lock_guard<std::mutex> lock(sMutex);
    if (sSearch.state != LookupState::Pending) return;
    query = sSearch.key;
  }
  std::vector<abf::StopMatch> results;
  const bool ok = provider::active().searchStops(query, results);
  std::lock_guard<std::mutex> lock(sMutex);
  if (sSearch.key != query) return;  // inzwischen neue Suche
  sSearch.state = ok ? LookupState::Done : LookupState::Error;
  sSearch.results = std::move(results);
  sSearch.doneAt = millis();
}

void runDirections() {
  std::string stopId;
  std::string cursor;
  {
    std::lock_guard<std::mutex> lock(sMutex);
    if (sDirections.state != LookupState::Pending) return;
    stopId = sDirections.key;
    if (sDirections.more) cursor = sDirections.cursor;
  }
  abf::StopTimesResult result;
  const bool ok = provider::active().departures(stopId, kCountDirections, cursor, result);
  std::lock_guard<std::mutex> lock(sMutex);
  if (sDirections.key != stopId) return;
  if (!ok) {
    sDirections.state = LookupState::Error;
    return;
  }
  if (!sDirections.more) sDirections.acc.clear();
  sDirections.acc.insert(sDirections.acc.end(), result.departures.begin(), result.departures.end());
  sDirections.cursor = result.nextPageCursor;
  sDirections.state = LookupState::Done;
  sDirections.doneAt = millis();
}

void pollStops() {
  const uint32_t now = millis();
  for (size_t i = 0;; ++i) {
    abf::StopConfig cfg;
    {
      std::lock_guard<std::mutex> lock(sMutex);
      if (i >= sSlots.size()) return;
      if (!sSlots[i].poll.due(now)) continue;
      cfg = sSlots[i].cfg;
    }
    const uint32_t count = cfg.filter == abf::StopFilter::All ? kCountAll : kCountSelected;
    abf::StopTimesResult result;
    const bool ok = provider::active().departures(cfg.id, count, "", result);

    std::lock_guard<std::mutex> lock(sMutex);
    if (i >= sSlots.size() || sSlots[i].cfg.id != cfg.id) return;  // Konfiguration hat sich geändert
    Slot &slot = sSlots[i];
    if (ok) {
      slot.data = std::move(result.departures);
      slot.lastSuccess = time(nullptr);
      slot.everFetched = true;
      slot.lastFailed = false;
      slot.poll.success(millis());
      sLastSuccess = slot.lastSuccess;
    } else {
      slot.lastFailed = true;
      slot.poll.failure(millis());
      LOGW("dep", "Abruf für \"%s\" fehlgeschlagen, nächster Versuch in %lu s", cfg.name.c_str(),
           static_cast<unsigned long>(slot.poll.nextDelay() / 1000));
    }
  }
}

void taskMain(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
    syncConfig();
    if (net::state() != abf::WifiState::Online || ota::busy()) continue;
    runSearch();
    runDirections();
    pollStops();
  }
}

}  // namespace

void begin() {
  const BaseType_t ok = xTaskCreatePinnedToCore(taskMain, "departures", 12 * 1024, nullptr, 1, &sTask, 1);
  if (ok != pdPASS) LOGE("dep", "Task konnte nicht gestartet werden");
}

void buildBoard(JsonObject out, size_t maxRows) {
  const int64_t now = time(nullptr);
  const bool timeValid = abf::isTimePlausible(now);
  const bool online = net::state() == abf::WifiState::Online;
  const abf::StopNameStyle stopName = config::get().stopName;

  std::lock_guard<std::mutex> lock(sMutex);
  out["time_valid"] = timeValid;
  out["stop_name"] = abf::stopNameStyleKey(stopName);
  if (timeValid) {
    const time_t t = static_cast<time_t>(now);
    struct tm tm;
    localtime_r(&t, &tm);
    out["now"] = localHhMm(now);
    out["date"] = abf::formatDateShort(tm);
    out["date_long"] = abf::formatDateLong(tm);
  }

  abf::StatusInputs base;
  base.hasStops = !sSlots.empty();
  base.timeValid = timeValid;
  base.wifiOnline = online;
  base.now = now;
  const abf::StatusResult overall = abf::evaluateStatus(base);
  if (overall.status == abf::BoardStatus::NoStops || overall.status == abf::BoardStatus::NoTime) {
    out["status"] = abf::boardStatusName(overall.status);
    out["text"] = abf::boardStatusText(overall.status);
  } else {
    out["status"] = "ok";
  }

  JsonArray stops = out["stops"].to<JsonArray>();
  for (const Slot &slot : sSlots) {
    JsonObject s = stops.add<JsonObject>();
    s["id"] = slot.cfg.id;
    s["name"] = slot.cfg.name;
    std::vector<abf::BoardRow> rows;
    if (timeValid) rows = abf::prepareDepartures(slot.data, slot.cfg, now, maxRows);

    abf::StatusInputs in = base;
    in.everFetched = slot.everFetched;
    in.lastFetchFailed = slot.lastFailed;
    in.lastSuccess = slot.lastSuccess;
    in.rows = rows.size();
    const abf::StatusResult st = abf::evaluateStatus(in);
    s["status"] = abf::boardStatusName(st.status);
    s["text"] = abf::boardStatusText(st.status);
    s["stale"] = st.stale;
    s["offline"] = st.offline;
    if (slot.everFetched) s["age_s"] = st.ageSeconds;

    JsonArray arr = s["rows"].to<JsonArray>();
    for (const abf::BoardRow &r : rows) {
      JsonObject o = arr.add<JsonObject>();
      o["line"] = r.line;
      o["to"] = r.to;
      o["track"] = r.track;
      o["mode"] = abf::modeKey(r.mode);
      colorToJson(o, "color", r.color);
      colorToJson(o, "text_color", r.textColor);
      o["time"] = localHhMm(r.when);
      o["scheduled"] = localHhMm(r.scheduled);
      o["minutes"] = r.minutes;
      o["delay"] = r.delayMin;
      o["realtime"] = r.realtime;
      o["cancelled"] = r.cancelled;
    }
  }
}

void lookupStops(const std::string &rawQuery, JsonObject out) {
  const std::string query = trim(rawQuery);
  if (query.size() < kMinQueryLength) {
    out["state"] = "error";
    out["message"] = "Bitte mindestens 2 Zeichen eingeben.";
    return;
  }
  std::lock_guard<std::mutex> lock(sMutex);
  const bool fresh = millis() - sSearch.doneAt < kLookupCacheMs;
  if (sSearch.key != query || sSearch.state == LookupState::Idle ||
      (sSearch.state == LookupState::Done && !fresh)) {
    sSearch.key = query;
    sSearch.state = LookupState::Pending;
    sSearch.results.clear();
    wake();
  }
  out["state"] = lookupStateName(sSearch.state);
  if (sSearch.state == LookupState::Error) {
    out["message"] = "Suche gerade nicht möglich. Bitte später erneut versuchen.";
    sSearch.state = LookupState::Idle;  // nächster Aufruf versucht es neu
    return;
  }
  if (sSearch.state != LookupState::Done) return;
  JsonArray arr = out["results"].to<JsonArray>();
  for (const abf::StopMatch &m : sSearch.results) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = m.id;
    o["name"] = m.name;
    o["town"] = m.town;
    o["region"] = m.region;
    o["country"] = m.country;
    JsonArray modes = o["modes"].to<JsonArray>();
    for (uint8_t bit = 1; bit && bit <= abf::kModeOther; bit <<= 1) {
      if (m.modes & bit) modes.add(abf::modeKey(bit));
    }
  }
}

void lookupDirections(const std::string &stopId, bool more, JsonObject out) {
  if (stopId.empty()) {
    out["state"] = "error";
    out["message"] = "Keine Haltestelle angegeben.";
    return;
  }
  std::lock_guard<std::mutex> lock(sMutex);
  const bool fresh = millis() - sDirections.doneAt < kLookupCacheMs;
  if (sDirections.key != stopId || sDirections.state == LookupState::Idle ||
      (sDirections.state == LookupState::Done && !fresh && !more)) {
    sDirections.key = stopId;
    sDirections.more = false;
    sDirections.acc.clear();
    sDirections.cursor.clear();
    sDirections.state = LookupState::Pending;
    wake();
  } else if (more && sDirections.state == LookupState::Done && !sDirections.cursor.empty()) {
    sDirections.more = true;
    sDirections.state = LookupState::Pending;
    wake();
  }
  out["state"] = lookupStateName(sDirections.state);
  if (sDirections.state == LookupState::Error) {
    out["message"] = "Fahrplandaten gerade nicht erreichbar. Bitte später erneut versuchen.";
    sDirections.state = LookupState::Idle;
    return;
  }
  if (sDirections.state != LookupState::Done) return;

  out["departures"] = sDirections.acc.size();
  out["can_more"] = !sDirections.cursor.empty();
  if (!sDirections.acc.empty()) {
    int64_t last = 0;
    for (const abf::Departure &d : sDirections.acc) last = d.scheduled > last ? d.scheduled : last;
    out["until"] = localHhMm(last);
  }
  JsonArray groups = out["groups"].to<JsonArray>();
  for (const abf::DirectionGroup &g : abf::groupDirections(sDirections.acc)) {
    JsonObject go = groups.add<JsonObject>();
    go["to"] = g.to;
    go["count"] = g.count;
    JsonArray opts = go["options"].to<JsonArray>();
    for (const abf::DirectionOption &o : g.options) {
      JsonObject oo = opts.add<JsonObject>();
      oo["route"] = o.routeId;
      if (o.direction == abf::kDirectionUnknown) {
        oo["dir"] = nullptr;
      } else {
        oo["dir"] = o.direction;
      }
      oo["line"] = o.line;
      oo["to"] = o.to;
      oo["mode"] = abf::modeKey(o.mode);
      oo["count"] = o.count;
    }
  }
}

int64_t lastSuccess() { return sLastSuccess; }

}  // namespace departures
