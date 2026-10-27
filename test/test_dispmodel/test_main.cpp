// Display-Logik: Nachtzeit, Helligkeit, Stil, Zeilentexte, Bildschirmwahl
#include <display_model.h>
#include <time_berlin.h>
#include <unity.h>

#include <stdlib.h>
#include <time.h>

using namespace abf;

namespace {

constexpr int64_t kNow = 1793100000;      // Di 27.10.2026 12:20 MEZ
constexpr int64_t kSummer = 1782941400;   // Mi 01.07.2026 23:30 MESZ

BoardRow row(const char *line, const char *to, int32_t minutes) {
  BoardRow r;
  r.line = line;
  r.to = to;
  r.mode = kModeTrain;
  r.minutes = minutes;
  r.when = kNow + minutes * 60;
  r.scheduled = r.when;
  r.realtime = true;
  return r;
}

ViewInputs boardInputs(size_t stops = 1, size_t rows = 4) {
  ViewInputs in;
  in.wifi = WifiState::Online;
  in.apName = "Abfahrtstafel-AB0C";
  in.hostname = "abfahrt";
  in.timeValid = true;
  in.now = kNow;
  for (size_t i = 0; i < stops; ++i) {
    StopConfig c;
    c.id = "s" + std::to_string(i);
    c.name = i == 0 ? "Emsdetten, Bahnhof" : "Busbahnhof";
    in.cfg.stops.push_back(c);
    StopInput s;
    s.name = c.name;
    s.status.status = BoardStatus::Ok;
    s.lastSuccess = kNow - 30;
    for (size_t r = 0; r < rows; ++r) s.rows.push_back(row("RE15", "Münster Hbf", 5 + 10 * static_cast<int32_t>(r)));
    in.stops.push_back(s);
  }
  return in;
}

}  // namespace

void setUp() {
  setenv("TZ", kTzBerlin, 1);
  tzset();
}
void tearDown() {}

// --- Nachtzeit -------------------------------------------------------------

static void test_window_same_day(void) {
  TEST_ASSERT_TRUE(inWindow(8 * 60, 12 * 60, 8 * 60));
  TEST_ASSERT_TRUE(inWindow(8 * 60, 12 * 60, 11 * 60 + 59));
  TEST_ASSERT_FALSE(inWindow(8 * 60, 12 * 60, 12 * 60));
  TEST_ASSERT_FALSE(inWindow(8 * 60, 12 * 60, 7 * 60));
}

static void test_window_over_midnight(void) {
  TEST_ASSERT_TRUE(inWindow(22 * 60, 6 * 60, 22 * 60));
  TEST_ASSERT_TRUE(inWindow(22 * 60, 6 * 60, 0));
  TEST_ASSERT_TRUE(inWindow(22 * 60, 6 * 60, 5 * 60 + 59));
  TEST_ASSERT_FALSE(inWindow(22 * 60, 6 * 60, 6 * 60));
  TEST_ASSERT_FALSE(inWindow(22 * 60, 6 * 60, 12 * 60));
  TEST_ASSERT_FALSE(inWindow(7 * 60, 7 * 60, 7 * 60));  // leeres Fenster
}

static void test_backlight_day_uses_theme(void) {
  Config c;
  c.brightness = 70;
  c.theme = DisplayTheme::Night;
  Backlight b = backlightFor(c, true, 12 * 60);
  TEST_ASSERT_EQUAL_UINT8(70, b.percent);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayTheme::Night), static_cast<int>(b.theme));
  TEST_ASSERT_FALSE(b.night);

  c.theme = DisplayTheme::Dark;
  c.nightEnabled = true;  // Tagsüber egal
  b = backlightFor(c, true, 12 * 60);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayTheme::Dark), static_cast<int>(b.theme));
}

static void test_backlight_night_forces_night_theme(void) {
  Config c;
  c.nightEnabled = true;
  c.nightStart = 22 * 60;
  c.nightEnd = 6 * 60;
  c.nightBrightness = 15;
  Backlight b = backlightFor(c, true, 23 * 60);
  TEST_ASSERT_TRUE(b.night);
  TEST_ASSERT_EQUAL_UINT8(15, b.percent);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayTheme::Night), static_cast<int>(b.theme));

  // Ohne Uhrzeit keine Nacht
  b = backlightFor(c, false, 23 * 60);
  TEST_ASSERT_FALSE(b.night);
  TEST_ASSERT_EQUAL_UINT8(80, b.percent);
}

static void test_local_time_dst(void) {
  TEST_ASSERT_EQUAL_STRING("12:20", localHhMm(kNow).c_str());
  TEST_ASSERT_EQUAL_STRING("23:30", localHhMm(kSummer).c_str());
  TEST_ASSERT_EQUAL_UINT32(23 * 60 + 30, localMinuteOfDay(kSummer));
}

// --- Zeilen ----------------------------------------------------------------

static void test_row_minutes_and_clock(void) {
  RowView v = makeRow(row("RE15", "Münster Hbf", 22), false);
  TEST_ASSERT_EQUAL_STRING("22", v.time.c_str());
  TEST_ASSERT_EQUAL_STRING("min", v.unit.c_str());

  v = makeRow(row("RE15", "Münster Hbf", 0), false);
  TEST_ASSERT_EQUAL_STRING("jetzt", v.time.c_str());
  TEST_ASSERT_EQUAL_STRING("", v.unit.c_str());

  v = makeRow(row("RB65", "Münster Hbf", 67), false);
  TEST_ASSERT_EQUAL_STRING("13:27", v.time.c_str());
  TEST_ASSERT_EQUAL_STRING("", v.unit.c_str());
}

static void test_row_delay_and_cancel(void) {
  BoardRow r = row("RE15", "Münster Hbf", 22);
  r.delayMin = 4;
  RowView v = makeRow(r, false);
  TEST_ASSERT_EQUAL_STRING("+4", v.note.c_str());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(NoteKind::Delay), static_cast<int>(v.noteKind));

  r.realtime = false;  // Verspätung ohne Echtzeit ist bedeutungslos
  TEST_ASSERT_EQUAL_STRING("", makeRow(r, false).note.c_str());

  BoardRow c = row("RE7", "Rheine", 21);
  c.cancelled = true;
  c.scheduled = kNow + 21 * 60;
  v = makeRow(c, false);
  TEST_ASSERT_TRUE(v.cancelled);
  TEST_ASSERT_EQUAL_STRING("12:41", v.time.c_str());
  TEST_ASSERT_EQUAL_STRING("fällt aus", v.note.c_str());
}

static void test_row_track_and_colors(void) {
  BoardRow r = row("RE15", "Münster Hbf", 5);
  r.track = "1";
  TEST_ASSERT_EQUAL_STRING("Gleis 1", makeRow(r, false).track.c_str());
  TEST_ASSERT_EQUAL_STRING("1", makeRow(r, true).track.c_str());
  r.mode = kModeBus;
  r.track = "B";
  TEST_ASSERT_EQUAL_STRING("Steig B", makeRow(r, false).track.c_str());
  r.track = "Bstg. 12";
  TEST_ASSERT_EQUAL_STRING("Bstg. 12", makeRow(r, false).track.c_str());

  RowView v = makeRow(r, false);  // keine Linienfarbe → Bus-Ersatzfarbe, Text hell
  TEST_ASSERT_EQUAL_HEX32(modeColor(kModeBus), v.badgeBg);
  TEST_ASSERT_EQUAL_HEX32(0xffffff, v.badgeFg);

  r.color = 0xffcc00;  // gelb → dunkle Schrift
  v = makeRow(r, false);
  TEST_ASSERT_EQUAL_HEX32(0x111111, v.badgeFg);
  r.textColor = 0xffcc00;  // gleiche Farbe wie Hintergrund → lesbar machen
  TEST_ASSERT_EQUAL_HEX32(0x111111, makeRow(r, false).badgeFg);
}

static void test_wifi_qr_escaping(void) {
  TEST_ASSERT_EQUAL_STRING("WIFI:S:Abfahrtstafel-AB0C;T:nopass;;", wifiQrPayload("Abfahrtstafel-AB0C").c_str());
  TEST_ASSERT_EQUAL_STRING("WIFI:S:a\\;b\\:c;T:nopass;;", wifiQrPayload("a;b:c").c_str());
}

// --- Bildschirmwahl --------------------------------------------------------

static void test_view_board_four_and_three_rows(void) {
  ViewInputs in = boardInputs(1, 5);
  View v = buildView(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Screen::Board), static_cast<int>(v.screen));
  TEST_ASSERT_EQUAL_STRING("Emsdetten, Bahnhof", v.title.c_str());
  TEST_ASSERT_EQUAL_STRING("12:20", v.clock.c_str());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(WifiMark::Ok), static_cast<int>(v.wifi));
  TEST_ASSERT_EQUAL_size_t(1, v.sections.size());
  TEST_ASSERT_EQUAL_size_t(4, v.sections[0].rows.size());

  in.cfg.rows = 3;
  v = buildView(in);
  TEST_ASSERT_EQUAL_UINT8(3, v.rows);
  TEST_ASSERT_EQUAL_size_t(3, v.sections[0].rows.size());
}

static void test_view_stale_banner(void) {
  ViewInputs in = boardInputs(1, 4);
  in.wifi = WifiState::Connecting;
  in.stops[0].status.stale = true;
  in.stops[0].status.offline = true;
  in.stops[0].lastSuccess = kNow - 6 * 60;
  View v = buildView(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Screen::Board), static_cast<int>(v.screen));
  TEST_ASSERT_EQUAL_STRING("Keine WLAN-Verbindung · Stand 12:14 Uhr", v.banner.c_str());
  TEST_ASSERT_EQUAL_size_t(3, v.sections[0].rows.size());  // Platz für die Leiste
  TEST_ASSERT_EQUAL_INT(static_cast<int>(WifiMark::Off), static_cast<int>(v.wifi));

  in.stops[0].status.offline = false;
  v = buildView(in);
  TEST_ASSERT_EQUAL_STRING("Fahrplandaten nicht erreichbar · Stand 12:14 Uhr", v.banner.c_str());
}

static void test_view_split_and_alternate(void) {
  ViewInputs in = boardInputs(2, 4);
  View v = buildView(in);
  TEST_ASSERT_EQUAL_size_t(2, v.sections.size());
  TEST_ASSERT_EQUAL_size_t(2, v.sections[0].rows.size());
  TEST_ASSERT_EQUAL_STRING("Busbahnhof", v.sections[1].title.c_str());

  in.stops[1].rows.clear();
  in.stops[1].status.status = BoardStatus::NoDepartures;
  v = buildView(in);
  TEST_ASSERT_EQUAL_STRING("Zurzeit keine Abfahrten.", v.sections[1].message.c_str());

  in = boardInputs(2, 4);
  in.cfg.twoStops = TwoStopLayout::Alternate;
  in.uptimeS = 3;
  v = buildView(in);
  TEST_ASSERT_EQUAL_STRING("1/2", v.page.c_str());
  TEST_ASSERT_EQUAL_STRING("Emsdetten, Bahnhof", v.title.c_str());
  in.uptimeS = kAlternateSeconds + 1;
  v = buildView(in);
  TEST_ASSERT_EQUAL_STRING("2/2", v.page.c_str());
  TEST_ASSERT_EQUAL_STRING("Busbahnhof", v.title.c_str());
}

static void test_view_setup_and_nostops(void) {
  ViewInputs in = boardInputs(0, 0);
  in.wifi = WifiState::Provisioning;
  View v = buildView(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Screen::Setup), static_cast<int>(v.screen));
  TEST_ASSERT_EQUAL_STRING("WIFI:S:Abfahrtstafel-AB0C;T:nopass;;", v.qr.c_str());
  TEST_ASSERT_EQUAL_size_t(3, v.steps.size());

  in.wifi = WifiState::Online;
  v = buildView(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Screen::NoStops), static_cast<int>(v.screen));
  TEST_ASSERT_EQUAL_STRING("http://abfahrt.local", v.qr.c_str());

  in.wifi = WifiState::Connecting;
  v = buildView(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Screen::Message), static_cast<int>(v.screen));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Icon::NoWifi), static_cast<int>(v.icon));
}

static void test_view_stop_saved_but_not_loaded(void) {
  ViewInputs in = boardInputs(1, 0);
  in.stops.clear();  // Konfiguration hat schon eine Haltestelle, Abruf-Task noch nicht
  View v = buildView(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Screen::Message), static_cast<int>(v.screen));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Icon::Loading), static_cast<int>(v.icon));
  TEST_ASSERT_EQUAL_STRING("Emsdetten, Bahnhof", v.title.c_str());
}

static void test_view_fallback_only_without_data(void) {
  ViewInputs in = boardInputs(1, 4);
  in.wifi = WifiState::Fallback;
  in.stops[0].status.stale = true;
  in.stops[0].status.offline = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Screen::Board), static_cast<int>(buildView(in).screen));
  in.stops[0].rows.clear();
  View v = buildView(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Screen::Setup), static_cast<int>(v.screen));
  TEST_ASSERT_EQUAL_STRING("WLAN nicht erreichbar", v.heading.c_str());
}

static void test_view_messages(void) {
  ViewInputs in = boardInputs(1, 0);
  in.stops[0].status.status = BoardStatus::ApiError;
  View v = buildView(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Screen::Message), static_cast<int>(v.screen));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Icon::Warning), static_cast<int>(v.icon));
  TEST_ASSERT_EQUAL_STRING("Fahrplandaten gerade nicht erreichbar.", v.heading.c_str());
  TEST_ASSERT_TRUE(v.text.size() > 0);

  in.stops[0].status.status = BoardStatus::Ok;  // Daten da, aber alles gefiltert
  v = buildView(in);
  TEST_ASSERT_EQUAL_STRING("Zurzeit keine Abfahrten.", v.heading.c_str());

  in.timeValid = false;
  v = buildView(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Icon::Clock), static_cast<int>(v.icon));
  TEST_ASSERT_EQUAL_STRING("", v.clock.c_str());
}

static void test_view_night_off_and_update(void) {
  ViewInputs in = boardInputs(1, 4);
  in.cfg.nightEnabled = true;
  in.cfg.nightStart = 12 * 60;
  in.cfg.nightEnd = 13 * 60;
  in.cfg.nightBrightness = 0;
  View v = buildView(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Screen::Off), static_cast<int>(v.screen));
  TEST_ASSERT_EQUAL_UINT8(0, v.backlight);

  in.cfg.nightBrightness = 10;
  v = buildView(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Screen::Board), static_cast<int>(v.screen));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayTheme::Night), static_cast<int>(v.theme));

  in.cfg.nightBrightness = 0;
  in.update = UpdatePhase::Downloading;
  in.updatePercent = 64;
  v = buildView(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Screen::Update), static_cast<int>(v.screen));
  TEST_ASSERT_EQUAL_INT8(64, v.progress);
  TEST_ASSERT_TRUE(v.backlight > 0);  // Update auch nachts sichtbar
}

static void test_view_equality_detects_changes(void) {
  ViewInputs in = boardInputs(1, 4);
  View a = buildView(in);
  View b = buildView(in);
  TEST_ASSERT_TRUE(a == b);
  in.now += 60;  // eine Minute später: Uhr und Minuten ändern sich
  in.stops[0].rows[0].minutes -= 1;
  b = buildView(in);
  TEST_ASSERT_TRUE(a != b);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_window_same_day);
  RUN_TEST(test_window_over_midnight);
  RUN_TEST(test_backlight_day_uses_theme);
  RUN_TEST(test_backlight_night_forces_night_theme);
  RUN_TEST(test_local_time_dst);
  RUN_TEST(test_row_minutes_and_clock);
  RUN_TEST(test_row_delay_and_cancel);
  RUN_TEST(test_row_track_and_colors);
  RUN_TEST(test_wifi_qr_escaping);
  RUN_TEST(test_view_board_four_and_three_rows);
  RUN_TEST(test_view_stale_banner);
  RUN_TEST(test_view_split_and_alternate);
  RUN_TEST(test_view_setup_and_nostops);
  RUN_TEST(test_view_stop_saved_but_not_loaded);
  RUN_TEST(test_view_fallback_only_without_data);
  RUN_TEST(test_view_messages);
  RUN_TEST(test_view_night_off_and_update);
  RUN_TEST(test_view_equality_detects_changes);
  return UNITY_END();
}
