#include "motis_parser.h"

#include <string.h>

namespace abf {

namespace {

bool digits(const char *p, int n, int &out) {
  out = 0;
  for (int i = 0; i < n; ++i) {
    if (p[i] < '0' || p[i] > '9') return false;
    out = out * 10 + (p[i] - '0');
  }
  return true;
}

// Tage seit 1970-01-01 für ein Datum im proleptischen Gregorianischen Kalender (H. Hinnant)
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

const char *str(JsonVariantConst v) { return v.is<const char *>() ? v.as<const char *>() : ""; }

uint8_t parseDirection(JsonVariantConst v) {
  if (v.is<int>()) {
    const int d = v.as<int>();
    return (d == 0 || d == 1) ? static_cast<uint8_t>(d) : kDirectionUnknown;
  }
  const char *s = str(v);
  if (strcmp(s, "0") == 0) return 0;
  if (strcmp(s, "1") == 0) return 1;
  return kDirectionUnknown;
}

}  // namespace

bool parseIsoTime(const char *t, int64_t &epoch) {
  if (!t || strlen(t) < 19) return false;
  int y, mo, d, h, mi, s;
  if (!digits(t, 4, y) || t[4] != '-' || !digits(t + 5, 2, mo) || t[7] != '-' || !digits(t + 8, 2, d) ||
      (t[10] != 'T' && t[10] != ' ') || !digits(t + 11, 2, h) || t[13] != ':' || !digits(t + 14, 2, mi) ||
      t[16] != ':' || !digits(t + 17, 2, s)) {
    return false;
  }
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 60) return false;

  const char *p = t + 19;
  if (*p == '.') {
    ++p;
    while (*p >= '0' && *p <= '9') ++p;
  }
  int offset = 0;
  if (*p == 'Z') {
    ++p;
  } else if (*p == '+' || *p == '-') {
    int oh, om;
    if (!digits(p + 1, 2, oh) || p[3] != ':' || !digits(p + 4, 2, om)) return false;
    offset = (oh * 60 + om) * 60 * (*p == '-' ? -1 : 1);
    p += 6;
  } else {
    return false;  // ohne Zeitzone nicht eindeutig
  }
  if (*p != '\0') return false;

  epoch = daysFromCivil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) * 86400 + h * 3600 + mi * 60 + s -
          offset;
  return true;
}

uint8_t modeFromMotis(const char *m) {
  static const struct {
    const char *name;
    uint8_t bit;
  } kMap[] = {
      {"HIGHSPEED_RAIL", kModeTrain}, {"LONG_DISTANCE", kModeTrain}, {"NIGHT_RAIL", kModeTrain},
      {"REGIONAL_RAIL", kModeTrain},  {"REGIONAL_FAST_RAIL", kModeTrain}, {"RAIL", kModeTrain},
      {"SUBURBAN", kModeTrain},       {"SUBWAY", kModeSubway},       {"METRO", kModeSubway},
      {"TRAM", kModeTram},            {"BUS", kModeBus},             {"COACH", kModeBus},
      {"FERRY", kModeFerry},
  };
  for (const auto &e : kMap) {
    if (strcmp(m, e.name) == 0) return e.bit;
  }
  return kModeOther;
}

int32_t parseHexColor(const char *t) {
  if (!t || strlen(t) != 6) return -1;
  int32_t v = 0;
  for (int i = 0; i < 6; ++i) {
    const char c = t[i];
    int n;
    if (c >= '0' && c <= '9') {
      n = c - '0';
    } else if (c >= 'a' && c <= 'f') {
      n = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      n = c - 'A' + 10;
    } else {
      return -1;
    }
    v = v * 16 + n;
  }
  return v;
}

bool parseStopTimes(const char *json, size_t len, StopTimesResult &out, ArduinoJson::Allocator *alloc) {
  JsonDocument filter;
  JsonObject f = filter["stopTimes"][0].to<JsonObject>();
  JsonObject place = f["place"].to<JsonObject>();
  for (const char *k : {"departure", "scheduledDeparture", "track", "scheduledTrack", "cancelled"}) place[k] = true;
  for (const char *k : {"mode", "realTime", "headsign", "routeId", "directionId", "routeShortName", "displayName",
                        "routeColor", "routeTextColor", "cancelled", "tripCancelled"}) {
    f[k] = true;
  }
  filter["nextPageCursor"] = true;

  JsonDocument doc(alloc ? alloc : ArduinoJson::detail::DefaultAllocator::instance());
  if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter))) return false;
  JsonArrayConst arr = doc["stopTimes"];
  if (arr.isNull()) return false;

  out.departures.clear();
  out.departures.reserve(arr.size());
  for (JsonObjectConst st : arr) {
    JsonObjectConst p = st["place"];
    Departure d;
    // Nur Abfahrten: Endhalte haben keine scheduledDeparture
    if (!parseIsoTime(str(p["scheduledDeparture"]), d.scheduled)) continue;
    if (!parseIsoTime(str(p["departure"]), d.expected)) d.expected = d.scheduled;
    d.routeId = str(st["routeId"]);
    d.direction = parseDirection(st["directionId"]);
    d.line = str(st["routeShortName"]);
    if (d.line.empty()) d.line = str(st["displayName"]);
    d.headsign = str(st["headsign"]);
    d.track = str(p["track"]);
    if (d.track.empty()) d.track = str(p["scheduledTrack"]);
    d.mode = modeFromMotis(str(st["mode"]));
    d.color = parseHexColor(str(st["routeColor"]));
    d.textColor = parseHexColor(str(st["routeTextColor"]));
    d.realtime = st["realTime"] | false;
    d.cancelled = (p["cancelled"] | false) || (st["cancelled"] | false) || (st["tripCancelled"] | false);
    out.departures.push_back(std::move(d));
  }
  out.nextPageCursor = str(doc["nextPageCursor"]);
  return true;
}

bool parseGeocode(const char *json, size_t len, std::vector<StopMatch> &out, ArduinoJson::Allocator *alloc) {
  JsonDocument filter;
  JsonObject f = filter[0].to<JsonObject>();
  for (const char *k : {"type", "name", "id", "country"}) f[k] = true;
  f["areas"][0]["name"] = true;
  f["areas"][0]["adminLevel"] = true;
  f["areas"][0]["default"] = true;
  f["modes"] = true;

  JsonDocument doc(alloc ? alloc : ArduinoJson::detail::DefaultAllocator::instance());
  if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter))) return false;
  if (!doc.is<JsonArrayConst>()) return false;

  out.clear();
  for (JsonObjectConst g : doc.as<JsonArrayConst>()) {
    if (strcmp(str(g["type"]), "STOP") != 0) continue;
    StopMatch m;
    m.id = str(g["id"]);
    m.name = str(g["name"]);
    m.country = str(g["country"]);
    if (m.id.empty() || m.name.empty()) continue;
    std::string state;
    std::string fallbackTown;
    for (JsonObjectConst a : g["areas"].as<JsonArrayConst>()) {
      const int level = static_cast<int>(a["adminLevel"] | 0.0);  // kommt als Gleitkommazahl ("8E0")
      const char *name = str(a["name"]);
      if (level == 8) m.town = name;
      if (level == 6) m.region = name;
      if (level == 4) state = name;
      if (a["default"] | false) fallbackTown = name;
    }
    if (m.town.empty()) m.town = fallbackTown;
    if (m.region.empty()) m.region = state;
    for (JsonVariantConst mode : g["modes"].as<JsonArrayConst>()) m.modes |= modeFromMotis(str(mode));
    out.push_back(std::move(m));
  }
  return true;
}

}  // namespace abf
