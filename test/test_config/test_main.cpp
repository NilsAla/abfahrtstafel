// Konfiguration: NVS-Schema, Migration, Validierung, JSON
#include <app_config.h>
#include <config_json.h>
#include <unity.h>

#include <map>
#include <string>

using namespace abf;

namespace {

class MemStore : public KvStore {
 public:
  bool getU32(const char *key, uint32_t &out) const override {
    auto it = u32.find(key);
    if (it == u32.end()) return false;
    out = it->second;
    return true;
  }
  bool putU32(const char *key, uint32_t value) override {
    if (failWrites) return false;
    u32[key] = value;
    return true;
  }
  bool getStr(const char *key, std::string &out) const override {
    auto it = str.find(key);
    if (it == str.end()) return false;
    out = it->second;
    return true;
  }
  bool putStr(const char *key, const std::string &value) override {
    if (failWrites) return false;
    str[key] = value;
    return true;
  }
  std::map<std::string, uint32_t> u32;
  std::map<std::string, std::string> str;
  bool failWrites = false;
};

}  // namespace

void setUp() {}
void tearDown() {}

// --- Migration -------------------------------------------------------------

static void test_migrate_fresh_writes_schema(void) {
  MemStore s;
  MigrationResult r = migrateConfig(s);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(MigrationOutcome::Fresh), static_cast<int>(r.outcome));
  TEST_ASSERT_EQUAL_UINT32(0, r.from);
  TEST_ASSERT_EQUAL_UINT32(kConfigSchema, r.to);
  TEST_ASSERT_EQUAL_UINT32(kConfigSchema, s.u32["schema"]);
}

static void test_migrate_current_is_noop(void) {
  MemStore s;
  s.u32["schema"] = kConfigSchema;
  s.failWrites = true;  // darf nichts schreiben
  TEST_ASSERT_EQUAL_INT(static_cast<int>(MigrationOutcome::Current), static_cast<int>(migrateConfig(s).outcome));
}

static void test_migrate_newer_schema_untouched(void) {
  // Rollback: neuere Firmware hat Schema 7 geschrieben, alte Firmware darf es nicht zurücksetzen
  MemStore s;
  s.u32["schema"] = kConfigSchema + 6;
  s.u32["bright"] = 42;
  s.u32["futureKey"] = 123;
  MigrationResult r = migrateConfig(s);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(MigrationOutcome::Newer), static_cast<int>(r.outcome));
  TEST_ASSERT_EQUAL_UINT32(kConfigSchema + 6, s.u32["schema"]);
  TEST_ASSERT_EQUAL_UINT32(123, s.u32["futureKey"]);
  TEST_ASSERT_EQUAL_UINT32(42, loadConfig(s).brightness);

  // Speichern aus der alten Firmware senkt das Schema nicht
  Config c = loadConfig(s);
  c.brightness = 50;
  TEST_ASSERT_TRUE(saveConfig(s, c));
  TEST_ASSERT_EQUAL_UINT32(kConfigSchema + 6, s.u32["schema"]);
  TEST_ASSERT_EQUAL_UINT32(123, s.u32["futureKey"]);
}

static void test_migrate_write_failure_reported(void) {
  MemStore s;
  s.failWrites = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(MigrationOutcome::Failed), static_cast<int>(migrateConfig(s).outcome));
}

// --- Laden/Speichern -------------------------------------------------------

static void test_load_empty_gives_defaults(void) {
  MemStore s;
  int invalid = -1;
  Config c = loadConfig(s, &invalid);
  Config d;
  TEST_ASSERT_EQUAL_INT(0, invalid);
  TEST_ASSERT_EQUAL_STRING(d.hostname.c_str(), c.hostname.c_str());
  TEST_ASSERT_EQUAL_UINT32(d.brightness, c.brightness);
  TEST_ASSERT_EQUAL_UINT32(d.nightStart, c.nightStart);
  TEST_ASSERT_TRUE(c.updateCheck);
}

static void test_save_load_roundtrip(void) {
  MemStore s;
  Config c;
  c.hostname = "kueche";
  c.brightness = 33;
  c.nightEnabled = true;
  c.nightStart = 23 * 60 + 15;
  c.nightEnd = 5 * 60 + 45;
  c.nightBrightness = 10;
  c.updateCheck = false;
  TEST_ASSERT_TRUE(saveConfig(s, c));

  Config l = loadConfig(s);
  TEST_ASSERT_EQUAL_STRING("kueche", l.hostname.c_str());
  TEST_ASSERT_EQUAL_UINT32(33, l.brightness);
  TEST_ASSERT_TRUE(l.nightEnabled);
  TEST_ASSERT_EQUAL_UINT32(23 * 60 + 15, l.nightStart);
  TEST_ASSERT_EQUAL_UINT32(5 * 60 + 45, l.nightEnd);
  TEST_ASSERT_EQUAL_UINT32(10, l.nightBrightness);
  TEST_ASSERT_FALSE(l.updateCheck);
}

static void test_load_invalid_values_fall_back(void) {
  MemStore s;
  s.str["host"] = "Bad Name!";
  s.u32["bright"] = 0;
  s.u32["nightOn"] = 7;
  s.u32["nightStart"] = 24 * 60;
  s.u32["nightBright"] = 101;
  int invalid = 0;
  Config c = loadConfig(s, &invalid);
  Config d;
  TEST_ASSERT_EQUAL_INT(5, invalid);
  TEST_ASSERT_EQUAL_STRING(d.hostname.c_str(), c.hostname.c_str());
  TEST_ASSERT_EQUAL_UINT32(d.brightness, c.brightness);
  TEST_ASSERT_FALSE(c.nightEnabled);
  TEST_ASSERT_EQUAL_UINT32(d.nightStart, c.nightStart);
  TEST_ASSERT_EQUAL_UINT32(d.nightBrightness, c.nightBrightness);
}

// --- Hilfsfunktionen -------------------------------------------------------

static void test_hostname_rules(void) {
  TEST_ASSERT_TRUE(isValidHostname("abfahrt"));
  TEST_ASSERT_TRUE(isValidHostname("tafel-2"));
  TEST_ASSERT_TRUE(isValidHostname("a"));
  TEST_ASSERT_FALSE(isValidHostname(""));
  TEST_ASSERT_FALSE(isValidHostname("-tafel"));
  TEST_ASSERT_FALSE(isValidHostname("tafel-"));
  TEST_ASSERT_FALSE(isValidHostname("Tafel"));
  TEST_ASSERT_FALSE(isValidHostname("küche"));
  TEST_ASSERT_FALSE(isValidHostname("tafel.local"));
  TEST_ASSERT_FALSE(isValidHostname(std::string(33, 'a')));
  TEST_ASSERT_TRUE(isValidHostname(std::string(32, 'a')));
  TEST_ASSERT_EQUAL_STRING("abfahrt-kueche", normalizeHostname("  Abfahrt-Kueche \t").c_str());
}

static void test_hhmm(void) {
  uint32_t m = 0;
  TEST_ASSERT_TRUE(parseHhMm("22:00", m));
  TEST_ASSERT_EQUAL_UINT32(1320, m);
  TEST_ASSERT_TRUE(parseHhMm("6:05", m));
  TEST_ASSERT_EQUAL_UINT32(365, m);
  TEST_ASSERT_TRUE(parseHhMm("00:00", m));
  TEST_ASSERT_EQUAL_UINT32(0, m);
  TEST_ASSERT_TRUE(parseHhMm("23:59", m));
  TEST_ASSERT_EQUAL_UINT32(1439, m);
  TEST_ASSERT_FALSE(parseHhMm("24:00", m));
  TEST_ASSERT_FALSE(parseHhMm("12:60", m));
  TEST_ASSERT_FALSE(parseHhMm("1200", m));
  TEST_ASSERT_FALSE(parseHhMm("12:0", m));
  TEST_ASSERT_FALSE(parseHhMm(":12", m));
  TEST_ASSERT_FALSE(parseHhMm("123:00", m));
  TEST_ASSERT_FALSE(parseHhMm("ab:cd", m));
  TEST_ASSERT_FALSE(parseHhMm("", m));
  TEST_ASSERT_EQUAL_STRING("06:05", formatHhMm(365).c_str());
  TEST_ASSERT_EQUAL_STRING("23:59", formatHhMm(1439).c_str());
}

// --- JSON ------------------------------------------------------------------

static void test_json_roundtrip(void) {
  Config c;
  c.nightStart = 21 * 60 + 30;
  JsonDocument doc;
  configToJson(c, doc.to<JsonObject>());
  TEST_ASSERT_EQUAL_STRING("abfahrt", doc["hostname"]);
  TEST_ASSERT_EQUAL_STRING("21:30", doc["night_start"]);
  TEST_ASSERT_EQUAL_INT(80, doc["brightness"].as<int>());

  Config back;
  back.brightness = 1;  // wird überschrieben
  std::vector<FieldError> errors;
  TEST_ASSERT_TRUE(applyConfigJson(doc.as<JsonObjectConst>(), back, errors));
  TEST_ASSERT_EQUAL_UINT32(80, back.brightness);
  TEST_ASSERT_EQUAL_UINT32(21 * 60 + 30, back.nightStart);
}

static void test_json_partial_update(void) {
  Config c;
  JsonDocument doc;
  deserializeJson(doc, R"({"brightness": 40, "unknown": 1})");
  std::vector<FieldError> errors;
  TEST_ASSERT_TRUE(applyConfigJson(doc.as<JsonObjectConst>(), c, errors));
  TEST_ASSERT_EQUAL_UINT32(40, c.brightness);
  TEST_ASSERT_EQUAL_STRING("abfahrt", c.hostname.c_str());
}

static void test_json_hostname_normalized(void) {
  Config c;
  JsonDocument doc;
  deserializeJson(doc, R"({"hostname": " Flur "})");
  std::vector<FieldError> errors;
  TEST_ASSERT_TRUE(applyConfigJson(doc.as<JsonObjectConst>(), c, errors));
  TEST_ASSERT_EQUAL_STRING("flur", c.hostname.c_str());
}

static void test_json_errors_are_atomic(void) {
  Config c;
  JsonDocument doc;
  deserializeJson(doc,
                  R"({"brightness": 50, "hostname": "nicht gültig", "night_start": "25:00",
                      "night_enabled": "ja", "night_brightness": 101, "update_check": 0})");
  std::vector<FieldError> errors;
  TEST_ASSERT_FALSE(applyConfigJson(doc.as<JsonObjectConst>(), c, errors));
  TEST_ASSERT_EQUAL_size_t(5, errors.size());
  TEST_ASSERT_EQUAL_STRING("hostname", errors[0].field.c_str());
  TEST_ASSERT_EQUAL_UINT32(80, c.brightness);  // gültiges Feld nicht übernommen
}

static void test_json_brightness_type_and_range(void) {
  Config c;
  std::vector<FieldError> errors;
  const char *bad[] = {R"({"brightness": 4})", R"({"brightness": 101})", R"({"brightness": "50"})",
                       R"({"brightness": 50.5})", R"({"brightness": -1})", R"({"brightness": 1e12})"};
  for (const char *json : bad) {
    JsonDocument doc;
    deserializeJson(doc, json);
    TEST_ASSERT_FALSE_MESSAGE(applyConfigJson(doc.as<JsonObjectConst>(), c, errors), json);
  }
  JsonDocument doc;
  deserializeJson(doc, R"({"brightness": 5, "night_brightness": 0})");
  TEST_ASSERT_TRUE(applyConfigJson(doc.as<JsonObjectConst>(), c, errors));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_migrate_fresh_writes_schema);
  RUN_TEST(test_migrate_current_is_noop);
  RUN_TEST(test_migrate_newer_schema_untouched);
  RUN_TEST(test_migrate_write_failure_reported);
  RUN_TEST(test_load_empty_gives_defaults);
  RUN_TEST(test_save_load_roundtrip);
  RUN_TEST(test_load_invalid_values_fall_back);
  RUN_TEST(test_hostname_rules);
  RUN_TEST(test_hhmm);
  RUN_TEST(test_json_roundtrip);
  RUN_TEST(test_json_partial_update);
  RUN_TEST(test_json_hostname_normalized);
  RUN_TEST(test_json_errors_are_atomic);
  RUN_TEST(test_json_brightness_type_and_range);
  return UNITY_END();
}
