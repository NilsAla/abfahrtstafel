#include "app_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stop_config.h>

namespace abf {

namespace {

constexpr const char *kKeySchema = "schema";
constexpr const char *kKeyHostname = "host";
constexpr const char *kKeyBrightness = "bright";
constexpr const char *kKeyNightOn = "nightOn";
constexpr const char *kKeyNightStart = "nightStart";
constexpr const char *kKeyNightEnd = "nightEnd";
constexpr const char *kKeyNightBright = "nightBright";
constexpr const char *kKeyUpdateCheck = "updCheck";
constexpr const char *kKeyStops = "stops";
constexpr const char *kKeyTheme = "theme";
constexpr const char *kKeyRows = "rows";
constexpr const char *kKeyTwoStops = "twoStops";
constexpr const char *kKeyStopName = "stopName";

// Migrationsschritte: kSteps[n] hebt Schema n auf n+1.
// Schema 1 ist das erste veröffentlichte, daher noch keine Schritte.
using MigrationStep = bool (*)(KvStore &);
constexpr MigrationStep kSteps[] = {
    nullptr,  // 0 → 1: Erststart, nur Schema schreiben
};
static_assert(sizeof(kSteps) / sizeof(kSteps[0]) == kConfigSchema,
              "Für jede Schema-Version einen Migrationsschritt eintragen");

void readU32(const KvStore &store, const char *key, uint32_t &field, bool (*valid)(uint32_t),
             int &invalid) {
  uint32_t v;
  if (!store.getU32(key, v)) return;
  if (valid(v)) {
    field = v;
  } else {
    ++invalid;
  }
}

void readBool(const KvStore &store, const char *key, bool &field, int &invalid) {
  uint32_t v;
  if (!store.getU32(key, v)) return;
  if (v <= 1) {
    field = v == 1;
  } else {
    ++invalid;
  }
}

bool validBrightness(uint32_t v) { return v >= kBrightnessMin && v <= kBrightnessMax; }
bool validNightBrightness(uint32_t v) { return v <= kBrightnessMax; }
bool validMinutes(uint32_t v) { return v < kMinutesPerDay; }
bool validRows(uint32_t v) { return v >= kRowsMin && v <= kRowsMax; }
bool validEnum2(uint32_t v) { return v <= 1; }
bool validEnum3(uint32_t v) { return v <= 2; }

}  // namespace

MigrationResult migrateConfig(KvStore &store) {
  uint32_t stored = 0;
  const bool found = store.getU32(kKeySchema, stored);
  if (found && stored == kConfigSchema) return {MigrationOutcome::Current, stored, stored};
  if (found && stored > kConfigSchema) return {MigrationOutcome::Newer, stored, stored};

  const uint32_t from = found ? stored : 0;
  for (uint32_t v = from; v < kConfigSchema; ++v) {
    if (kSteps[v] && !kSteps[v](store)) return {MigrationOutcome::Failed, from, v};
  }
  if (!store.putU32(kKeySchema, kConfigSchema)) return {MigrationOutcome::Failed, from, from};
  return {found ? MigrationOutcome::Migrated : MigrationOutcome::Fresh, from, kConfigSchema};
}

Config loadConfig(const KvStore &store, int *invalidCount) {
  Config cfg;
  int invalid = 0;

  std::string host;
  if (store.getStr(kKeyHostname, host)) {
    if (isValidHostname(host)) {
      cfg.hostname = host;
    } else {
      ++invalid;
    }
  }
  readU32(store, kKeyBrightness, cfg.brightness, validBrightness, invalid);
  readBool(store, kKeyNightOn, cfg.nightEnabled, invalid);
  readU32(store, kKeyNightStart, cfg.nightStart, validMinutes, invalid);
  readU32(store, kKeyNightEnd, cfg.nightEnd, validMinutes, invalid);
  readU32(store, kKeyNightBright, cfg.nightBrightness, validNightBrightness, invalid);
  readBool(store, kKeyUpdateCheck, cfg.updateCheck, invalid);
  readU32(store, kKeyRows, cfg.rows, validRows, invalid);
  uint32_t v = 0;
  if (store.getU32(kKeyTheme, v)) {
    if (validEnum2(v)) cfg.theme = static_cast<DisplayTheme>(v); else ++invalid;
  }
  if (store.getU32(kKeyTwoStops, v)) {
    if (validEnum2(v)) cfg.twoStops = static_cast<TwoStopLayout>(v); else ++invalid;
  }
  if (store.getU32(kKeyStopName, v)) {
    if (validEnum3(v)) cfg.stopName = static_cast<StopNameStyle>(v); else ++invalid;
  }

  std::string stops;
  if (store.getStr(kKeyStops, stops) && !stopsFromString(stops, cfg.stops)) {
    cfg.stops.clear();
    ++invalid;
  }

  if (invalidCount) *invalidCount = invalid;
  return cfg;
}

bool saveConfig(KvStore &store, const Config &cfg) {
  bool ok = store.putStr(kKeyHostname, cfg.hostname);
  ok &= store.putU32(kKeyBrightness, cfg.brightness);
  ok &= store.putU32(kKeyNightOn, cfg.nightEnabled ? 1 : 0);
  ok &= store.putU32(kKeyNightStart, cfg.nightStart);
  ok &= store.putU32(kKeyNightEnd, cfg.nightEnd);
  ok &= store.putU32(kKeyNightBright, cfg.nightBrightness);
  ok &= store.putU32(kKeyUpdateCheck, cfg.updateCheck ? 1 : 0);
  ok &= store.putStr(kKeyStops, stopsToString(cfg.stops));
  ok &= store.putU32(kKeyTheme, static_cast<uint32_t>(cfg.theme));
  ok &= store.putU32(kKeyRows, cfg.rows);
  ok &= store.putU32(kKeyTwoStops, static_cast<uint32_t>(cfg.twoStops));
  ok &= store.putU32(kKeyStopName, static_cast<uint32_t>(cfg.stopName));
  return ok;
}

const char *themeKey(DisplayTheme t) { return t == DisplayTheme::Night ? "night" : "dark"; }

bool themeFromKey(const std::string &key, DisplayTheme &out) {
  if (key == "dark") {
    out = DisplayTheme::Dark;
  } else if (key == "night") {
    out = DisplayTheme::Night;
  } else {
    return false;
  }
  return true;
}

const char *twoStopKey(TwoStopLayout l) { return l == TwoStopLayout::Alternate ? "alternate" : "split"; }

bool twoStopFromKey(const std::string &key, TwoStopLayout &out) {
  if (key == "split") {
    out = TwoStopLayout::Split;
  } else if (key == "alternate") {
    out = TwoStopLayout::Alternate;
  } else {
    return false;
  }
  return true;
}

const char *stopNameKey(StopNameStyle s) {
  switch (s) {
    case StopNameStyle::Large: return "large";
    case StopNameStyle::Off:   return "off";
    default:                   return "normal";
  }
}

bool stopNameFromKey(const std::string &key, StopNameStyle &out) {
  for (StopNameStyle s : {StopNameStyle::Normal, StopNameStyle::Large, StopNameStyle::Off}) {
    if (key == stopNameKey(s)) {
      out = s;
      return true;
    }
  }
  return false;
}

bool isValidHostname(const std::string &name) {
  if (name.empty() || name.size() > 32) return false;
  if (name.front() == '-' || name.back() == '-') return false;
  for (char c : name) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    if (!ok) return false;
  }
  return true;
}

std::string normalizeHostname(const std::string &name) {
  size_t begin = 0;
  size_t end = name.size();
  while (begin < end && isspace(static_cast<unsigned char>(name[begin]))) ++begin;
  while (end > begin && isspace(static_cast<unsigned char>(name[end - 1]))) --end;
  std::string out = name.substr(begin, end - begin);
  for (char &c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return out;
}

bool parseHhMm(const std::string &text, uint32_t &minutes) {
  // Erlaubt "H:MM" und "HH:MM"
  const size_t colon = text.find(':');
  if (colon == std::string::npos || colon == 0 || colon > 2 || text.size() != colon + 3) return false;
  uint32_t h = 0;
  uint32_t m = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    if (i == colon) continue;
    const char c = text[i];
    if (c < '0' || c > '9') return false;
    if (i < colon) {
      h = h * 10 + static_cast<uint32_t>(c - '0');
    } else {
      m = m * 10 + static_cast<uint32_t>(c - '0');
    }
  }
  if (h > 23 || m > 59) return false;
  minutes = h * 60 + m;
  return true;
}

std::string formatHhMm(uint32_t minutes) {
  minutes %= kMinutesPerDay;
  char buf[6];
  snprintf(buf, sizeof(buf), "%02u:%02u", static_cast<unsigned>(minutes / 60),
           static_cast<unsigned>(minutes % 60));
  return buf;
}

}  // namespace abf
