#include "config_json.h"

#include <stop_config.h>

namespace abf {

namespace {

constexpr const char *kMsgBool = "Bitte ein- oder ausschalten.";
constexpr const char *kMsgTime = "Bitte eine Uhrzeit im Format HH:MM angeben.";

void applyPercent(JsonObjectConst in, const char *field, uint32_t min, uint32_t max, uint32_t &target,
                  std::vector<FieldError> &errors) {
  JsonVariantConst v = in[field];
  if (v.isNull()) return;
  if (!v.is<int>() || v.as<int>() < static_cast<int>(min) || v.as<int>() > static_cast<int>(max)) {
    errors.push_back({field, "Bitte einen Wert von " + std::to_string(min) + " bis " +
                                 std::to_string(max) + " % angeben."});
    return;
  }
  target = static_cast<uint32_t>(v.as<int>());
}

void applyBool(JsonObjectConst in, const char *field, bool &target, std::vector<FieldError> &errors) {
  JsonVariantConst v = in[field];
  if (v.isNull()) return;
  if (!v.is<bool>()) {
    errors.push_back({field, kMsgBool});
    return;
  }
  target = v.as<bool>();
}

void applyTime(JsonObjectConst in, const char *field, uint32_t &target, std::vector<FieldError> &errors) {
  JsonVariantConst v = in[field];
  if (v.isNull()) return;
  uint32_t minutes;
  if (!v.is<const char *>() || !parseHhMm(v.as<const char *>(), minutes)) {
    errors.push_back({field, kMsgTime});
    return;
  }
  target = minutes;
}

}  // namespace

void configToJson(const Config &cfg, JsonObject out) {
  out["hostname"] = cfg.hostname;
  out["brightness"] = cfg.brightness;
  out["night_enabled"] = cfg.nightEnabled;
  out["night_start"] = formatHhMm(cfg.nightStart);
  out["night_end"] = formatHhMm(cfg.nightEnd);
  out["night_brightness"] = cfg.nightBrightness;
  out["update_check"] = cfg.updateCheck;
  out["stop_name"] = stopNameStyleKey(cfg.stopName);
  stopsToJson(cfg.stops, out["stops"].to<JsonArray>());
}

bool applyConfigJson(JsonObjectConst in, Config &cfg, std::vector<FieldError> &errors) {
  errors.clear();
  Config next = cfg;

  JsonVariantConst host = in["hostname"];
  if (!host.isNull()) {
    const std::string name = host.is<const char *>() ? normalizeHostname(host.as<const char *>()) : "";
    if (isValidHostname(name)) {
      next.hostname = name;
    } else {
      errors.push_back({"hostname",
                        "Nur Kleinbuchstaben a–z, Ziffern und Bindestrich, 1 bis 32 Zeichen, "
                        "kein Bindestrich am Anfang oder Ende."});
    }
  }
  applyPercent(in, "brightness", kBrightnessMin, kBrightnessMax, next.brightness, errors);
  applyBool(in, "night_enabled", next.nightEnabled, errors);
  applyTime(in, "night_start", next.nightStart, errors);
  applyTime(in, "night_end", next.nightEnd, errors);
  applyPercent(in, "night_brightness", 0, kBrightnessMax, next.nightBrightness, errors);
  applyBool(in, "update_check", next.updateCheck, errors);

  JsonVariantConst stopName = in["stop_name"];
  if (!stopName.isNull() &&
      (!stopName.is<const char *>() || !parseStopNameStyle(stopName.as<const char *>(), next.stopName))) {
    errors.push_back({"stop_name", "Bitte eine der angebotenen Varianten wählen."});
  }

  JsonVariantConst stops = in["stops"];
  if (!stops.isNull()) {
    std::string error;
    if (!stops.is<JsonArrayConst>() || !stopsFromJson(stops.as<JsonArrayConst>(), next.stops, error)) {
      errors.push_back({"stops", error.empty() ? "Ungültige Haltestellen-Liste." : error});
    }
  }

  if (!errors.empty()) return false;
  cfg = next;
  return true;
}

}  // namespace abf
