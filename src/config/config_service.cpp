#include "config_service.h"

#include <Preferences.h>

#include <atomic>
#include <mutex>

#include "../log/app_log.h"

namespace config {

namespace {

constexpr const char *kNamespace = "cfg";

class NvsStore : public abf::KvStore {
 public:
  explicit NvsStore(Preferences &p) : p_(p) {}

  bool getU32(const char *key, uint32_t &out) const override {
    if (!p_.isKey(key)) return false;
    out = p_.getUInt(key);
    return true;
  }
  bool putU32(const char *key, uint32_t value) override {
    return p_.putUInt(key, value) == sizeof(uint32_t);
  }
  bool getStr(const char *key, std::string &out) const override {
    if (!p_.isKey(key)) return false;
    out = p_.getString(key).c_str();
    return true;
  }
  bool putStr(const char *key, const std::string &value) override {
    return p_.putString(key, value.c_str()) == value.size();
  }

 private:
  Preferences &p_;
};

std::mutex sMutex;
abf::Config sConfig;
std::string sBootHostname;
std::atomic<uint32_t> sRevision{0};

const char *outcomeText(abf::MigrationOutcome o) {
  switch (o) {
    case abf::MigrationOutcome::Fresh:    return "Erststart, Schema angelegt";
    case abf::MigrationOutcome::Current:  return "aktuell";
    case abf::MigrationOutcome::Migrated: return "migriert";
    case abf::MigrationOutcome::Newer:    return "neueres Schema (Rollback?), nur bekannte Felder genutzt";
    case abf::MigrationOutcome::Failed:   return "FEHLER beim Schreiben";
  }
  return "?";
}

}  // namespace

void begin() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    LOGE("config", "NVS nicht verfügbar, nutze Standardwerte");
    std::lock_guard<std::mutex> lock(sMutex);
    sBootHostname = sConfig.hostname;
    return;
  }
  NvsStore store(prefs);
  const abf::MigrationResult m = abf::migrateConfig(store);
  int invalid = 0;
  abf::Config cfg = abf::loadConfig(store, &invalid);
  prefs.end();

  LOGI("config", "Schema %lu → %lu: %s", static_cast<unsigned long>(m.from), static_cast<unsigned long>(m.to),
       outcomeText(m.outcome));
  if (invalid > 0) LOGW("config", "%d ungültige Werte verworfen (Standard genutzt)", invalid);

  std::lock_guard<std::mutex> lock(sMutex);
  sConfig = cfg;
  sBootHostname = cfg.hostname;
}

abf::Config get() {
  std::lock_guard<std::mutex> lock(sMutex);
  return sConfig;
}

uint32_t revision() { return sRevision; }

UpdateResult applyJson(JsonObjectConst in) {
  UpdateResult r;
  std::lock_guard<std::mutex> lock(sMutex);
  abf::Config next = sConfig;
  if (!abf::applyConfigJson(in, next, r.errors)) {
    r.cfg = sConfig;
    return r;
  }

  Preferences prefs;
  bool saved = prefs.begin(kNamespace, false);
  if (saved) {
    NvsStore store(prefs);
    saved = abf::saveConfig(store, next);
    prefs.end();
  }
  if (!saved) {
    LOGE("config", "Speichern in NVS fehlgeschlagen");
    r.saveFailed = true;
    r.cfg = sConfig;
    return r;
  }

  sConfig = next;
  ++sRevision;
  r.ok = true;
  r.cfg = next;
  r.restartRequired = next.hostname != sBootHostname;
  LOGI("config", "Einstellungen gespeichert");
  return r;
}

}  // namespace config
