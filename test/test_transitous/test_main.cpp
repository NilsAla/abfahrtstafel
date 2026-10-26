// Transitous/MOTIS: Parser, Aufbereitung, Richtungsauswahl, Haltestellen-Konfiguration, Status
#include <board_status.h>
#include <departure_board.h>
#include <motis_parser.h>
#include <motis_urls.h>
#include <stop_config.h>
#include <string.h>
#include <unity.h>

#include <algorithm>

#include "fixtures.h"

using namespace abf;

void setUp() {}
void tearDown() {}

#define ASSERT_ENUM(expected, actual) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual))

constexpr int64_t kNow = 1793013600;  // 2026-10-26 11:20:00 UTC = 12:20 MEZ, Zeitpunkt des Abrufs
constexpr const char *kRouteRE15 = "de-VBN_34533_2";
constexpr const char *kRouteRB65 = "de-DELFI_3694958_106";
constexpr const char *kRouteRE7 = "de-DELFI_de:nrw:re7:_106";

static std::vector<Departure> fixtureDepartures() {
  StopTimesResult r;
  TEST_ASSERT_TRUE(parseStopTimes(kFixture_stoptimes_emsdetten_bf_2026_10_26,
                                  strlen(kFixture_stoptimes_emsdetten_bf_2026_10_26), r));
  return r.departures;
}

// --- Zeit ------------------------------------------------------------------

static void test_iso_time(void) {
  int64_t t = 0;
  TEST_ASSERT_TRUE(parseIsoTime("2026-10-26T11:21:00Z", t));
  TEST_ASSERT_EQUAL_INT64(1793013660, t);
  TEST_ASSERT_TRUE(parseIsoTime("2026-10-26T12:21:00+01:00", t));
  TEST_ASSERT_EQUAL_INT64(1793013660, t);
  TEST_ASSERT_TRUE(parseIsoTime("2026-10-26T10:21:00-01:00", t));
  TEST_ASSERT_EQUAL_INT64(1793013660, t);
  TEST_ASSERT_TRUE(parseIsoTime("2026-10-26T11:21:00.250Z", t));
  TEST_ASSERT_EQUAL_INT64(1793013660, t);
  TEST_ASSERT_TRUE(parseIsoTime("2024-02-29T12:00:00Z", t));
  TEST_ASSERT_EQUAL_INT64(1709208000, t);
  TEST_ASSERT_TRUE(parseIsoTime("2026-02-28T23:59:59Z", t));
  TEST_ASSERT_EQUAL_INT64(1772323199, t);
  TEST_ASSERT_TRUE(parseIsoTime("1970-01-01T00:00:00Z", t));
  TEST_ASSERT_EQUAL_INT64(0, t);
  TEST_ASSERT_TRUE(parseIsoTime("2100-03-01T00:00:00Z", t));
  TEST_ASSERT_EQUAL_INT64(4107542400LL, t);

  const char *bad[] = {"", "2026-10-26", "2026-10-26T11:21:00", "2026-13-01T00:00:00Z", "2026-10-26T24:00:00Z",
                       "2026-10-26T11:21:00Zx", "2026-10-26T11:21:00+0100", "26-10-26T11:21:00Z", nullptr};
  for (const char *b : bad) TEST_ASSERT_FALSE_MESSAGE(parseIsoTime(b, t), b ? b : "null");
}

static void test_mode_and_color(void) {
  TEST_ASSERT_EQUAL_UINT8(kModeTrain, modeFromMotis("REGIONAL_RAIL"));
  TEST_ASSERT_EQUAL_UINT8(kModeTrain, modeFromMotis("HIGHSPEED_RAIL"));
  TEST_ASSERT_EQUAL_UINT8(kModeTrain, modeFromMotis("SUBURBAN"));
  TEST_ASSERT_EQUAL_UINT8(kModeSubway, modeFromMotis("SUBWAY"));
  TEST_ASSERT_EQUAL_UINT8(kModeTram, modeFromMotis("TRAM"));
  TEST_ASSERT_EQUAL_UINT8(kModeBus, modeFromMotis("COACH"));
  TEST_ASSERT_EQUAL_UINT8(kModeFerry, modeFromMotis("FERRY"));
  TEST_ASSERT_EQUAL_UINT8(kModeOther, modeFromMotis("AIRPLANE"));
  TEST_ASSERT_EQUAL_INT32(0xea9d22, parseHexColor("ea9d22"));
  TEST_ASSERT_EQUAL_INT32(0x8FD400, parseHexColor("8FD400"));
  TEST_ASSERT_EQUAL_INT32(-1, parseHexColor(""));
  TEST_ASSERT_EQUAL_INT32(-1, parseHexColor("#ea9d22"));
  TEST_ASSERT_EQUAL_INT32(-1, parseHexColor("xyz123"));
}

// --- Parser ----------------------------------------------------------------

static void test_parse_stoptimes_fixture(void) {
  StopTimesResult r;
  TEST_ASSERT_TRUE(parseStopTimes(kFixture_stoptimes_emsdetten_bf_2026_10_26,
                                  strlen(kFixture_stoptimes_emsdetten_bf_2026_10_26), r));
  TEST_ASSERT_EQUAL_size_t(20, r.departures.size());
  TEST_ASSERT_EQUAL_STRING("LATER|1793019900", r.nextPageCursor.c_str());

  const Departure &bus = r.departures[0];
  TEST_ASSERT_EQUAL_STRING("R75", bus.line.c_str());
  TEST_ASSERT_EQUAL_STRING("Burgsteinfurt, Bahnhof", bus.headsign.c_str());
  TEST_ASSERT_EQUAL_STRING("A", bus.track.c_str());
  TEST_ASSERT_EQUAL_UINT8(kModeBus, bus.mode);
  TEST_ASSERT_EQUAL_UINT8(0, bus.direction);
  TEST_ASSERT_FALSE(bus.realtime);
  TEST_ASSERT_FALSE(bus.cancelled);
  TEST_ASSERT_EQUAL_INT64(1793013660, bus.scheduled);
  TEST_ASSERT_EQUAL_INT64(bus.scheduled, bus.expected);
  TEST_ASSERT_EQUAL_INT32(-1, bus.color);

  const Departure &re15 = r.departures[1];
  TEST_ASSERT_EQUAL_STRING("RE15", re15.line.c_str());
  TEST_ASSERT_EQUAL_STRING(kRouteRE15, re15.routeId.c_str());
  TEST_ASSERT_EQUAL_UINT8(kModeTrain, re15.mode);
  TEST_ASSERT_TRUE(re15.realtime);
  TEST_ASSERT_EQUAL_INT64(4 * 60, re15.expected - re15.scheduled);
  TEST_ASSERT_EQUAL_INT32(0xea9d22, re15.color);
  TEST_ASSERT_EQUAL_INT32(0xffffff, re15.textColor);
  TEST_ASSERT_EQUAL_STRING("1", re15.track.c_str());

  // Linie ohne Zugnummer, obwohl displayName "RB65 (52322)" ist
  TEST_ASSERT_EQUAL_STRING("RB65", r.departures[4].line.c_str());
  TEST_ASSERT_EQUAL_UINT8(1, r.departures[4].direction);

  size_t cancelled = 0;
  for (const Departure &d : r.departures) cancelled += d.cancelled ? 1 : 0;
  TEST_ASSERT_EQUAL_size_t(5, cancelled);

  // Halt aus zweitem Feed ohne Gleisangabe
  TEST_ASSERT_EQUAL_STRING("", r.departures[9].track.c_str());
  TEST_ASSERT_EQUAL_STRING("RE15", r.departures[9].line.c_str());
}

static void test_parse_stoptimes_edge_cases(void) {
  StopTimesResult r;
  TEST_ASSERT_FALSE(parseStopTimes("", 0, r));
  TEST_ASSERT_FALSE(parseStopTimes("{\"error\":\"x\"}", 13, r));
  const char *empty = R"({"stopTimes":[],"place":{}})";
  TEST_ASSERT_TRUE(parseStopTimes(empty, strlen(empty), r));
  TEST_ASSERT_EQUAL_size_t(0, r.departures.size());

  // Endhalt ohne Abfahrtszeit wird übersprungen, fehlende Prognose = Plan, directionId als Zahl
  const char *odd = R"({"stopTimes":[
      {"place":{"arrival":"2026-10-26T11:21:00Z","scheduledArrival":"2026-10-26T11:21:00Z"},"routeShortName":"X"},
      {"place":{"scheduledDeparture":"2026-10-26T11:30:00Z"},"displayName":"Y 1","directionId":1,"mode":"TRAM"}]})";
  TEST_ASSERT_TRUE(parseStopTimes(odd, strlen(odd), r));
  TEST_ASSERT_EQUAL_size_t(1, r.departures.size());
  TEST_ASSERT_EQUAL_STRING("Y 1", r.departures[0].line.c_str());
  TEST_ASSERT_EQUAL_UINT8(1, r.departures[0].direction);
  TEST_ASSERT_EQUAL_UINT8(kModeTram, r.departures[0].mode);
  TEST_ASSERT_EQUAL_INT64(r.departures[0].scheduled, r.departures[0].expected);
}

static void test_parse_geocode_fixture(void) {
  std::vector<StopMatch> m;
  TEST_ASSERT_TRUE(parseGeocode(kFixture_geocode_emsdetten_bahnhof, strlen(kFixture_geocode_emsdetten_bahnhof), m));
  TEST_ASSERT_EQUAL_size_t(5, m.size());
  TEST_ASSERT_EQUAL_STRING("de-DELFI_de:05566:13729_G", m[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("Emsdetten, Bahnhof", m[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("Emsdetten", m[0].town.c_str());
  TEST_ASSERT_EQUAL_STRING("Kreis Steinfurt", m[0].region.c_str());
  TEST_ASSERT_EQUAL_STRING("DE", m[0].country.c_str());
  TEST_ASSERT_EQUAL_UINT8(kModeTrain | kModeBus, m[0].modes);
  TEST_ASSERT_EQUAL_STRING("Olten", m[1].town.c_str());
  TEST_ASSERT_EQUAL_STRING("CH", m[1].country.c_str());
  TEST_ASSERT_EQUAL_STRING("Bezirk Uster", m[3].region.c_str());

  TEST_ASSERT_FALSE(parseGeocode("{}", 2, m));
  TEST_ASSERT_TRUE(parseGeocode("[]", 2, m));
  TEST_ASSERT_EQUAL_size_t(0, m.size());
}

// --- Aufbereitung ----------------------------------------------------------

static void test_shorten_headsign(void) {
  TEST_ASSERT_EQUAL_STRING("Münster Hbf", shortenHeadsign("Münster(Westf) Hbf").c_str());
  TEST_ASSERT_EQUAL_STRING("Münster Hbf", shortenHeadsign("Münster Hauptbahnhof").c_str());
  TEST_ASSERT_EQUAL_STRING("Rheine", shortenHeadsign("Rheine, Bahnhof").c_str());
  TEST_ASSERT_EQUAL_STRING("Emden Hbf", shortenHeadsign("Emden Hbf").c_str());
  TEST_ASSERT_EQUAL_STRING("Hamm Hbf", shortenHeadsign("Hamm (Westf), Hauptbahnhof").c_str());
  TEST_ASSERT_EQUAL_STRING("Emsdetten", shortenHeadsign("Emsdetten Bahnhof").c_str());
  TEST_ASSERT_EQUAL_STRING("Lübeck, ZOB", shortenHeadsign("Lübeck, ZOB").c_str());
  TEST_ASSERT_EQUAL_STRING("Bahnhof", shortenHeadsign("Bahnhof").c_str());  // nicht leer kürzen
  TEST_ASSERT_EQUAL_STRING("(X)", shortenHeadsign("(X)").c_str());
}

static StopConfig allConfig() {
  StopConfig c;
  c.id = "de-DELFI_de:05566:13729_G";
  c.name = "Emsdetten, Bahnhof";
  return c;
}

static void test_prepare_all(void) {
  const std::vector<BoardRow> rows = prepareDepartures(fixtureDepartures(), allConfig(), kNow, 50);
  // Bereits vergangene Ausfälle (Plan 11:16Z, 10:41Z) fallen weg, die übrigen 3 Ausfälle stehen zur Planzeit
  TEST_ASSERT_EQUAL_size_t(18, rows.size());
  TEST_ASSERT_EQUAL_STRING("R75", rows[0].line.c_str());
  TEST_ASSERT_EQUAL_INT32(1, rows[0].minutes);
  TEST_ASSERT_EQUAL_STRING("Burgsteinfurt", rows[0].to.c_str());
  TEST_ASSERT_FALSE(rows[0].realtime);

  // RE7 nach Rheine, Plan 12:41 MEZ, fällt aus → eingeordnet bei 12:41 statt 14:04
  auto re7 = std::find_if(rows.begin(), rows.end(), [](const BoardRow &r) { return r.cancelled; });
  TEST_ASSERT_TRUE(re7 != rows.end());
  TEST_ASSERT_EQUAL_STRING("RE7", re7->line.c_str());
  TEST_ASSERT_EQUAL_INT64(1793014860, re7->when);
  TEST_ASSERT_EQUAL_INT32(21, re7->minutes);
  TEST_ASSERT_EQUAL_INT32(0, re7->delayMin);

  // Sortierung nach maßgeblicher Zeit
  for (size_t i = 1; i < rows.size(); ++i) TEST_ASSERT_TRUE(rows[i - 1].when <= rows[i].when);

  // RE15 nach Emden mit +24 min
  auto emden = std::find_if(rows.begin(), rows.end(), [](const BoardRow &r) { return r.to == "Emden Hbf"; });
  TEST_ASSERT_EQUAL_INT32(24, emden->delayMin);
  TEST_ASSERT_EQUAL_INT32(26, emden->minutes);

  TEST_ASSERT_EQUAL_size_t(4, prepareDepartures(fixtureDepartures(), allConfig(), kNow, 4).size());
}

static void test_prepare_walk_offset(void) {
  StopConfig c = allConfig();
  c.walkMinutes = 5;
  const std::vector<BoardRow> rows = prepareDepartures(fixtureDepartures(), c, kNow, 50);
  TEST_ASSERT_EQUAL_size_t(17, rows.size());  // R75 in 1 min ist nicht mehr erreichbar
  for (const BoardRow &r : rows) TEST_ASSERT_TRUE(r.minutes >= 5);
  TEST_ASSERT_EQUAL_STRING("RE7", rows[0].line.c_str());  // Ausfall um 12:41 steht vor RE15 um 12:42
  TEST_ASSERT_TRUE(rows[0].cancelled);
}

static void test_prepare_modes(void) {
  StopConfig c = allConfig();
  c.modes = kModeBus;
  const std::vector<BoardRow> rows = prepareDepartures(fixtureDepartures(), c, kNow, 50);
  TEST_ASSERT_EQUAL_size_t(5, rows.size());
  for (const BoardRow &r : rows) TEST_ASSERT_EQUAL_UINT8(kModeBus, r.mode);
}

static void test_prepare_selected_direction(void) {
  StopConfig c = allConfig();
  c.filter = StopFilter::Selected;
  c.select.push_back({kRouteRE15, 0, "RE15", "Münster Hbf"});
  c.select.push_back({kRouteRB65, 0, "RB65", "Münster Hbf"});
  const std::vector<BoardRow> rows = prepareDepartures(fixtureDepartures(), c, kNow, 50);
  TEST_ASSERT_EQUAL_size_t(5, rows.size());
  for (const BoardRow &r : rows) TEST_ASSERT_EQUAL_STRING("Münster Hbf", r.to.c_str());
  TEST_ASSERT_EQUAL_STRING("RE15", rows[0].line.c_str());
  TEST_ASSERT_EQUAL_INT32(4, rows[0].delayMin);

  // Modus-Filter gilt nur bei "alle"
  c.modes = kModeBus;
  TEST_ASSERT_EQUAL_size_t(5, prepareDepartures(fixtureDepartures(), c, kNow, 50).size());
}

static void test_prepare_dedup_prefers_realtime_and_track(void) {
  Departure a;
  a.routeId = "r";
  a.line = "RE15";
  a.headsign = "Emden Hbf";
  a.scheduled = a.expected = kNow + 600;
  Departure b = a;
  b.realtime = true;
  b.expected = kNow + 720;
  b.track = "2";
  const std::vector<BoardRow> rows = prepareDepartures({a, b}, allConfig(), kNow, 10);
  TEST_ASSERT_EQUAL_size_t(1, rows.size());
  TEST_ASSERT_TRUE(rows[0].realtime);
  TEST_ASSERT_EQUAL_STRING("2", rows[0].track.c_str());
  TEST_ASSERT_EQUAL_INT32(2, rows[0].delayMin);
}

static void test_prepare_stale_data_shrinks(void) {
  // API-Ausfall: alte Daten laufen mit der Uhr weiter
  const std::vector<BoardRow> later = prepareDepartures(fixtureDepartures(), allConfig(), kNow + 3600, 50);
  for (const BoardRow &r : later) TEST_ASSERT_TRUE(r.when >= kNow + 3600);
  TEST_ASSERT_TRUE(later.size() < 18);
  TEST_ASSERT_EQUAL_size_t(0, prepareDepartures(fixtureDepartures(), allConfig(), kNow + 86400, 50).size());
}

static void test_group_directions(void) {
  const std::vector<DirectionGroup> g = groupDirections(fixtureDepartures());
  auto find = [&](const char *to) {
    return std::find_if(g.begin(), g.end(), [&](const DirectionGroup &x) { return x.to == to; });
  };
  auto ms = find("Münster Hbf");
  TEST_ASSERT_TRUE(ms != g.end());
  TEST_ASSERT_EQUAL_size_t(2, ms->options.size());
  TEST_ASSERT_EQUAL_STRING("RB65", ms->options[0].line.c_str());
  TEST_ASSERT_EQUAL_STRING("RE15", ms->options[1].line.c_str());
  TEST_ASSERT_EQUAL_STRING(kRouteRE15, ms->options[1].routeId.c_str());
  TEST_ASSERT_EQUAL_UINT8(0, ms->options[1].direction);

  auto rheine = find("Rheine");
  TEST_ASSERT_TRUE(rheine != g.end());
  TEST_ASSERT_EQUAL_size_t(2, rheine->options.size());  // RB65 und RE7

  TEST_ASSERT_TRUE(find("Emden Hbf") != g.end());
  TEST_ASSERT_TRUE(find("Burgsteinfurt") != g.end());
  TEST_ASSERT_TRUE(find("Greven") != g.end());
  TEST_ASSERT_TRUE(find("Emsdetten") != g.end());
  auto krefeld = find("Krefeld Hbf");  // RE7 Richtung 0
  TEST_ASSERT_TRUE(krefeld != g.end());
  TEST_ASSERT_EQUAL_STRING(kRouteRE7, krefeld->options[0].routeId.c_str());
  TEST_ASSERT_EQUAL_size_t(7, g.size());

  // Bahn vor Bus
  TEST_ASSERT_EQUAL_UINT8(kModeTrain, g.front().options.front().mode);
  TEST_ASSERT_EQUAL_UINT8(kModeBus, g.back().options.front().mode);

  // Jede Abfahrt des Fixtures ist genau einer Option zugeordnet
  uint32_t total = 0;
  for (const DirectionGroup &x : g) total += x.count;
  TEST_ASSERT_EQUAL_UINT32(20, total);
}

// --- Haltestellen-Konfiguration -------------------------------------------

static void test_stops_roundtrip(void) {
  StopConfig a = allConfig();
  a.town = "Emsdetten";
  a.filter = StopFilter::Selected;
  a.select.push_back({kRouteRE15, 0, "RE15", "Münster Hbf"});
  a.select.push_back({"x", kDirectionUnknown, "1", "Ring"});
  a.walkMinutes = 4;
  StopConfig b;
  b.id = "bus";
  b.name = "Rathaus";
  b.modes = kModeBus | kModeTram;

  const std::string json = stopsToString({a, b});
  std::vector<StopConfig> back;
  TEST_ASSERT_TRUE(stopsFromString(json, back));
  TEST_ASSERT_EQUAL_size_t(2, back.size());
  TEST_ASSERT_EQUAL_STRING("Emsdetten", back[0].town.c_str());
  ASSERT_ENUM(StopFilter::Selected, back[0].filter);
  TEST_ASSERT_EQUAL_size_t(2, back[0].select.size());
  TEST_ASSERT_EQUAL_UINT8(0, back[0].select[0].direction);
  TEST_ASSERT_EQUAL_UINT8(kDirectionUnknown, back[0].select[1].direction);
  TEST_ASSERT_EQUAL_UINT32(4, back[0].walkMinutes);
  ASSERT_ENUM(StopFilter::All, back[1].filter);
  TEST_ASSERT_EQUAL_UINT8(kModeBus | kModeTram, back[1].modes);
  TEST_ASSERT_EQUAL_STRING(json.c_str(), stopsToString(back).c_str());
}

static void test_stops_validation(void) {
  struct Case {
    const char *json;
    bool ok;
  } cases[] = {
      {R"([])", true},
      {R"([{"id":"a","name":"A"}])", true},
      {R"([{"id":"a","name":"A"},{"id":"b","name":"B"},{"id":"c","name":"C"}])", false},  // > 2
      {R"([{"id":"a","name":"A"},{"id":"a","name":"A2"}])", false},                       // doppelt
      {R"([{"name":"A"}])", false},
      {R"([{"id":"","name":"A"}])", false},
      {R"([{"id":"a","name":"A","filter":"some"}])", false},
      {R"([{"id":"a","name":"A","modes":[]}])", false},
      {R"([{"id":"a","name":"A","modes":["rocket"]}])", false},
      {R"([{"id":"a","name":"A","filter":"selected"}])", false},  // keine Richtung
      {R"([{"id":"a","name":"A","filter":"selected","select":[{"route":"r","dir":2}]}])", false},
      {R"([{"id":"a","name":"A","filter":"selected","select":[{"route":"r","dir":"0"}]}])", false},
      {R"([{"id":"a","name":"A","filter":"selected","select":[{"route":"r","dir":null}]}])", true},
      {R"([{"id":"a","name":"A","walk":31}])", false},
      {R"([{"id":"a","name":"A","walk":-1}])", false},
      {R"([{"id":"a","name":"A","walk":30}])", true},
  };
  for (const Case &c : cases) {
    JsonDocument doc;
    deserializeJson(doc, c.json);
    std::vector<StopConfig> out;
    std::string error;
    TEST_ASSERT_EQUAL_MESSAGE(c.ok, stopsFromJson(doc.as<JsonArrayConst>(), out, error), c.json);
    if (!c.ok) TEST_ASSERT_TRUE_MESSAGE(!error.empty(), c.json);
  }
}

static void test_stops_size_limit(void) {
  JsonDocument doc;
  for (int s = 0; s < 2; ++s) {
    JsonObject o = doc.add<JsonObject>();
    o["id"] = std::string(120, 'a' + s);
    o["name"] = std::string(80, 'N');
    o["filter"] = "selected";
    for (int i = 0; i < 24; ++i) {
      JsonObject e = o["select"].add<JsonObject>();
      e["route"] = std::string(120, 'r') + std::to_string(i);
      e["dir"] = 0;
      e["line"] = "RE15";
      e["to"] = std::string(40, 'z');
    }
  }
  std::vector<StopConfig> out;
  std::string error;
  TEST_ASSERT_FALSE(stopsFromJson(doc.as<JsonArrayConst>(), out, error));
  TEST_ASSERT_TRUE(error.find("weniger Richtungen") != std::string::npos);
}

// --- Status und Abruf-Takt -------------------------------------------------

static void test_status(void) {
  StatusInputs in;
  ASSERT_ENUM(BoardStatus::NoStops, evaluateStatus(in).status);
  in.hasStops = true;
  ASSERT_ENUM(BoardStatus::NoTime, evaluateStatus(in).status);
  in.timeValid = true;
  in.now = kNow;
  ASSERT_ENUM(BoardStatus::NoWifi, evaluateStatus(in).status);
  in.wifiOnline = true;
  ASSERT_ENUM(BoardStatus::Loading, evaluateStatus(in).status);
  in.lastFetchFailed = true;
  ASSERT_ENUM(BoardStatus::ApiError, evaluateStatus(in).status);

  in.everFetched = true;
  in.lastFetchFailed = false;
  in.lastSuccess = kNow - 30;
  in.rows = 3;
  StatusResult r = evaluateStatus(in);
  ASSERT_ENUM(BoardStatus::Ok, r.status);
  TEST_ASSERT_FALSE(r.stale);
  TEST_ASSERT_EQUAL_INT64(30, r.ageSeconds);

  in.rows = 0;
  ASSERT_ENUM(BoardStatus::NoDepartures, evaluateStatus(in).status);

  // Veraltet: mit Daten weiter anzeigen, ohne Daten Ursache nennen
  in.lastSuccess = kNow - 600;
  in.rows = 2;
  in.wifiOnline = false;
  r = evaluateStatus(in);
  ASSERT_ENUM(BoardStatus::Ok, r.status);
  TEST_ASSERT_TRUE(r.stale);
  TEST_ASSERT_TRUE(r.offline);
  in.rows = 0;
  ASSERT_ENUM(BoardStatus::NoWifi, evaluateStatus(in).status);
  in.wifiOnline = true;
  ASSERT_ENUM(BoardStatus::ApiError, evaluateStatus(in).status);

  for (int s = 0; s <= static_cast<int>(BoardStatus::Ok); ++s) {
    TEST_ASSERT_TRUE(strlen(boardStatusName(static_cast<BoardStatus>(s))) > 0);
  }
}

static void test_poll_schedule(void) {
  PollSchedule p(60000, 600000);
  TEST_ASSERT_TRUE(p.due(0));
  p.success(1000);
  TEST_ASSERT_FALSE(p.due(60999));
  TEST_ASSERT_TRUE(p.due(61000));
  const uint32_t expected[] = {30000, 60000, 120000, 240000, 480000, 600000, 600000};
  for (uint32_t e : expected) {
    p.failure(0);
    TEST_ASSERT_EQUAL_UINT32(e, p.nextDelay());
  }
  p.success(0);
  TEST_ASSERT_EQUAL_UINT32(0, p.failures());
  TEST_ASSERT_EQUAL_UINT32(60000, p.nextDelay());
  p.trigger(5);
  TEST_ASSERT_TRUE(p.due(5));

  PollSchedule tooFast(1000);  // Nutzungsregeln: nie schneller als 30 s
  tooFast.success(0);
  TEST_ASSERT_EQUAL_UINT32(30000, tooFast.nextDelay());

  PollSchedule wrap(60000);  // millis()-Überlauf
  wrap.success(0xFFFFFFFFu - 1000);
  TEST_ASSERT_FALSE(wrap.due(10000));
  TEST_ASSERT_TRUE(wrap.due(59000));
}

// --- URLs ------------------------------------------------------------------

static void test_urls(void) {
  TEST_ASSERT_EQUAL_STRING("Emsdetten%20Bahnhof", urlEncode("Emsdetten Bahnhof").c_str());
  TEST_ASSERT_EQUAL_STRING("L%C3%BCbeck%2C%20ZOB", urlEncode("Lübeck, ZOB").c_str());
  TEST_ASSERT_EQUAL_STRING("a-b_c.d~e", urlEncode("a-b_c.d~e").c_str());
  TEST_ASSERT_EQUAL_STRING(
      "https://api.transitous.org/api/v6/stoptimes?stopId=de-DELFI_de%3A05566%3A13729_G&n=20",
      stopTimesUrl("de-DELFI_de:05566:13729_G", 20).c_str());
  TEST_ASSERT_EQUAL_STRING(
      "https://api.transitous.org/api/v6/stoptimes?stopId=x&n=100&pageCursor=LATER%7C1793019900",
      stopTimesUrl("x", 100, "LATER|1793019900").c_str());
  TEST_ASSERT_EQUAL_STRING("https://api.transitous.org/api/v1/geocode?text=Emsdetten%20Bahnhof&type=STOP&language=de&numResults=8",
                           geocodeUrl("Emsdetten Bahnhof", 8).c_str());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_iso_time);
  RUN_TEST(test_mode_and_color);
  RUN_TEST(test_parse_stoptimes_fixture);
  RUN_TEST(test_parse_stoptimes_edge_cases);
  RUN_TEST(test_parse_geocode_fixture);
  RUN_TEST(test_shorten_headsign);
  RUN_TEST(test_prepare_all);
  RUN_TEST(test_prepare_walk_offset);
  RUN_TEST(test_prepare_modes);
  RUN_TEST(test_prepare_selected_direction);
  RUN_TEST(test_prepare_dedup_prefers_realtime_and_track);
  RUN_TEST(test_prepare_stale_data_shrinks);
  RUN_TEST(test_group_directions);
  RUN_TEST(test_stops_roundtrip);
  RUN_TEST(test_stops_validation);
  RUN_TEST(test_stops_size_limit);
  RUN_TEST(test_status);
  RUN_TEST(test_poll_schedule);
  RUN_TEST(test_urls);
  return UNITY_END();
}
