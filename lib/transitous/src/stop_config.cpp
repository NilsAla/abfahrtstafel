#include "stop_config.h"

namespace abf {

namespace {

const char *kModeKeys[] = {"train", "subway", "tram", "bus", "ferry", "other"};

bool stringField(JsonVariantConst v, size_t maxLen, bool required, std::string &out) {
  if (v.isNull()) return !required;
  if (!v.is<const char *>()) return false;
  out = v.as<const char *>();
  if (required && out.empty()) return false;
  return out.size() <= maxLen;
}

}  // namespace

const char *modeKey(uint8_t bit) {
  for (int i = 0; i < 6; ++i) {
    if (bit == (1 << i)) return kModeKeys[i];
  }
  return "other";
}

uint8_t modeFromKey(const std::string &key) {
  for (int i = 0; i < 6; ++i) {
    if (key == kModeKeys[i]) return static_cast<uint8_t>(1 << i);
  }
  return 0;
}

void stopsToJson(const std::vector<StopConfig> &stops, JsonArray out) {
  for (const StopConfig &s : stops) {
    JsonObject o = out.add<JsonObject>();
    o["id"] = s.id;
    o["name"] = s.name;
    o["town"] = s.town;
    o["filter"] = s.filter == StopFilter::All ? "all" : "selected";
    JsonArray modes = o["modes"].to<JsonArray>();
    for (int i = 0; i < 6; ++i) {
      if (s.modes & (1 << i)) modes.add(kModeKeys[i]);
    }
    JsonArray sel = o["select"].to<JsonArray>();
    for (const RouteDirection &r : s.select) {
      JsonObject e = sel.add<JsonObject>();
      e["route"] = r.routeId;
      if (r.direction == kDirectionUnknown) {
        e["dir"] = nullptr;
      } else {
        e["dir"] = r.direction;
      }
      e["line"] = r.line;
      e["to"] = r.to;
    }
    o["walk"] = s.walkMinutes;
  }
}

bool stopsFromJson(JsonArrayConst in, std::vector<StopConfig> &out, std::string &error) {
  if (in.isNull()) {
    error = "Ungültige Haltestellen-Liste.";
    return false;
  }
  if (in.size() > kMaxStops) {
    error = "Höchstens " + std::to_string(kMaxStops) + " Haltestellen möglich.";
    return false;
  }
  std::vector<StopConfig> result;
  for (JsonObjectConst o : in) {
    StopConfig s;
    if (o.isNull() || !stringField(o["id"], 128, true, s.id) || !stringField(o["name"], 80, true, s.name) ||
        !stringField(o["town"], 64, false, s.town)) {
      error = "Haltestelle unvollständig. Bitte neu auswählen.";
      return false;
    }
    for (const StopConfig &other : result) {
      if (other.id == s.id) {
        error = "„" + s.name + "“ ist doppelt eingetragen.";
        return false;
      }
    }

    const std::string filter = o["filter"] | "all";
    if (filter == "all") {
      s.filter = StopFilter::All;
    } else if (filter == "selected") {
      s.filter = StopFilter::Selected;
    } else {
      error = "Unbekannte Filter-Art bei „" + s.name + "“.";
      return false;
    }

    if (!o["modes"].isNull()) {
      s.modes = 0;
      for (JsonVariantConst m : o["modes"].as<JsonArrayConst>()) {
        const uint8_t bit = m.is<const char *>() ? modeFromKey(m.as<const char *>()) : 0;
        if (!bit) {
          error = "Unbekanntes Verkehrsmittel bei „" + s.name + "“.";
          return false;
        }
        s.modes |= bit;
      }
    }
    if (s.filter == StopFilter::All && s.modes == 0) {
      error = "Bitte bei „" + s.name + "“ mindestens ein Verkehrsmittel wählen.";
      return false;
    }

    JsonArrayConst sel = o["select"];
    if (sel.size() > kMaxSelections) {
      error = "Zu viele Richtungen bei „" + s.name + "“ (höchstens " + std::to_string(kMaxSelections) + ").";
      return false;
    }
    for (JsonObjectConst e : sel) {
      RouteDirection r;
      JsonVariantConst dir = e["dir"];
      if (!stringField(e["route"], 128, true, r.routeId) || !stringField(e["line"], 16, false, r.line) ||
          !stringField(e["to"], 48, false, r.to) ||
          !(dir.isNull() || (dir.is<int>() && (dir.as<int>() == 0 || dir.as<int>() == 1)))) {
        error = "Ungültige Richtung bei „" + s.name + "“. Bitte Richtungen neu laden.";
        return false;
      }
      r.direction = dir.isNull() ? kDirectionUnknown : static_cast<uint8_t>(dir.as<int>());
      s.select.push_back(std::move(r));
    }
    if (s.filter == StopFilter::Selected && s.select.empty()) {
      error = "Bitte bei „" + s.name + "“ mindestens eine Richtung wählen.";
      return false;
    }

    JsonVariantConst walk = o["walk"];
    if (!walk.isNull()) {
      if (!walk.is<int>() || walk.as<int>() < 0 || walk.as<int>() > static_cast<int>(kMaxWalkMinutes)) {
        error = "Fußweg bei „" + s.name + "“: 0 bis " + std::to_string(kMaxWalkMinutes) + " Minuten.";
        return false;
      }
      s.walkMinutes = static_cast<uint32_t>(walk.as<int>());
    }
    result.push_back(std::move(s));
  }

  if (stopsToString(result).size() > kMaxStopsJson) {
    error = "Zu viele Einträge – bitte weniger Richtungen auswählen.";
    return false;
  }
  out = std::move(result);
  return true;
}

std::string stopsToString(const std::vector<StopConfig> &stops) {
  JsonDocument doc;
  stopsToJson(stops, doc.to<JsonArray>());
  std::string s;
  serializeJson(doc, s);
  return s;
}

bool stopsFromString(const std::string &json, std::vector<StopConfig> &out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  std::string error;
  return stopsFromJson(doc.as<JsonArrayConst>(), out, error);
}

}  // namespace abf
