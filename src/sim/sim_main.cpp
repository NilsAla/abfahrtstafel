// Simulator (env:sim): rendert alle Display-Zustände headless als PPM-Dateien.
// Aufruf über scripts/render_screens.py, das die Bilder als PNG nach docs/screens/ schreibt.
//
// Verwendet dieselbe Kette wie die Firmware: ViewInputs → abf::buildView() → ui::show() → LVGL.
// Farbtiefe wie am Gerät (RGB565), damit Farbstufen realistisch aussehen.
#include <lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <string>
#include <vector>

#include <display_model.h>
#include <time_berlin.h>

#include "../ui/ui.h"

using namespace abf;

namespace {

uint16_t sFrame[ui::kWidth * ui::kHeight];
uint32_t sTick = 0;

void flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px) {
  const uint16_t *src = reinterpret_cast<const uint16_t *>(px);
  for (int32_t y = area->y1; y <= area->y2; ++y) {
    for (int32_t x = area->x1; x <= area->x2; ++x) sFrame[y * ui::kWidth + x] = *src++;
  }
  lv_display_flush_ready(disp);
}

bool writePpm(const std::string &path) {
  FILE *f = fopen(path.c_str(), "wb");
  if (!f) return false;
  fprintf(f, "P6\n%d %d\n255\n", static_cast<int>(ui::kWidth), static_cast<int>(ui::kHeight));
  for (uint16_t c : sFrame) {
    const uint8_t rgb[3] = {static_cast<uint8_t>(((c >> 11) & 0x1f) * 255 / 31),
                            static_cast<uint8_t>(((c >> 5) & 0x3f) * 255 / 63),
                            static_cast<uint8_t>((c & 0x1f) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  return true;
}

// --- Beispieldaten (Emsdetten, Bahnhof – wie in den Mockups) ------------------

constexpr int64_t kNow = 1793100000;  // Di 27.10.2026 12:20 MEZ

BoardRow dep(const char *line, const char *to, int32_t minutes, uint8_t mode, int32_t color, int32_t textColor,
             const char *track, int32_t delay = 0) {
  BoardRow r;
  r.line = line;
  r.to = to;
  r.mode = mode;
  r.color = color;
  r.textColor = textColor;
  r.track = track;
  r.minutes = minutes;
  r.when = kNow + minutes * 60;
  r.scheduled = r.when - delay * 60;
  r.delayMin = delay;
  r.realtime = true;
  return r;
}

BoardRow cancelled(BoardRow r) {
  r.cancelled = true;
  return r;
}

std::vector<BoardRow> emsdetten() {
  return {dep("R75", "Burgsteinfurt", 1, kModeBus, 0x8e44a8, 0xffffff, "A"),
          cancelled(dep("RE7", "Rheine", 21, kModeTrain, 0x5a8a1c, 0x111111, "2")),
          dep("RE15", "Münster Hbf", 22, kModeTrain, 0xeea025, 0xffffff, "1", 4),
          dep("R61", "Greven", 23, kModeBus, 0x8e44a8, 0xffffff, "B"),
          dep("RB65", "Rheine", 29, kModeTrain, -1, -1, "2")};
}

std::vector<BoardRow> richtungMuenster() {
  return {dep("RE15", "Münster Hbf", 22, kModeTrain, 0xeea025, 0xffffff, "1", 4),
          dep("RB65", "Münster Hbf", 42, kModeTrain, -1, -1, "1"),
          dep("RB65", "Münster Hbf", 67, kModeTrain, -1, -1, "1"),
          dep("RE15", "Münster Hbf", 82, kModeTrain, 0xeea025, 0xffffff, "1")};
}

std::vector<BoardRow> busse() {
  return {dep("R61", "Greven", 23, kModeBus, 0x8e44a8, 0xffffff, "B"),
          dep("260", "Emsdetten Mitte", 56, kModeBus, 0x8e44a8, 0xffffff, "B")};
}

std::vector<BoardRow> langeNamen() {
  return {dep("ICE 1012", "Hamburg-Altona über Bremen Hbf, Osnabrück Hbf", 0, kModeTrain, 0xffffff, 0xd01020, "13"),
          dep("S1", "Ahrensburg Gartenholz (Bus SEV ab Ahrensburg)", 4, kModeTrain, 0x1d8f4a, 0xffffff, "1"),
          dep("5", "Lübeck ZOB/Hauptbahnhof", 7, kModeBus, -1, -1, "Bstg. 12"),
          dep("RE8", "Hamburg Hbf", 14, kModeTrain, 0xe2001a, 0xffffff, "5", 12)};
}

StopInput stop(const char *name, std::vector<BoardRow> rows, BoardStatus st = BoardStatus::Ok) {
  StopInput s;
  s.name = name;
  s.rows = std::move(rows);
  s.status.status = st;
  s.lastSuccess = kNow - 40;
  return s;
}

ViewInputs base() {
  ViewInputs in;
  in.wifi = WifiState::Online;
  in.apName = "Abfahrtstafel-AB0C";
  in.hostname = "abfahrt";
  in.timeValid = true;
  in.now = kNow;
  return in;
}

void addStop(ViewInputs &in, StopInput s) {
  StopConfig c;
  c.id = "id-" + s.name;
  c.name = s.name;
  in.cfg.stops.push_back(c);
  in.stops.push_back(std::move(s));
}

struct Scenario {
  const char *file;
  ViewInputs in;
};

std::vector<Scenario> scenarios() {
  std::vector<Scenario> out;

  ViewInputs in = base();
  addStop(in, stop("Emsdetten, Bahnhof", emsdetten()));
  out.push_back({"board-4-zeilen", in});

  ViewInputs night = in;
  night.cfg.theme = DisplayTheme::Night;
  out.push_back({"board-4-zeilen-nacht", night});

  // Haltestellenname: groß bzw. ausgeblendet (nur Datum und Uhr)
  ViewInputs large = in;
  large.cfg.stopName = StopNameStyle::Large;
  out.push_back({"kopfzeile-gross", large});
  large.cfg.rows = 3;
  out.push_back({"kopfzeile-gross-3-zeilen", large});
  ViewInputs bare = in;
  bare.cfg.stopName = StopNameStyle::Off;
  out.push_back({"kopfzeile-aus", bare});
  bare.cfg.theme = DisplayTheme::Night;
  out.push_back({"kopfzeile-aus-nacht", bare});

  in = base();
  in.cfg.rows = 3;
  addStop(in, stop("Emsdetten · Richtung Münster", richtungMuenster()));
  out.push_back({"board-3-zeilen", in});
  in.cfg.theme = DisplayTheme::Night;
  out.push_back({"board-3-zeilen-nacht", in});

  in = base();
  addStop(in, stop("Bahnhof · Richtung Münster", richtungMuenster()));
  addStop(in, stop("Bahnhof · Busse", busse()));
  out.push_back({"zwei-haltestellen-geteilt", in});

  ViewInputs splitEmpty = in;
  splitEmpty.stops[1].rows.clear();
  splitEmpty.stops[1].status.status = BoardStatus::NoDepartures;
  splitEmpty.stops[0].status.stale = true;
  splitEmpty.stops[0].lastSuccess = kNow - 6 * 60;
  out.push_back({"zwei-haltestellen-geteilt-veraltet", splitEmpty});

  in.cfg.twoStops = TwoStopLayout::Alternate;
  in.updateAvailable = true;
  in.uptimeS = 0;
  out.push_back({"zwei-haltestellen-wechsel-1", in});
  in.uptimeS = kAlternateSeconds;
  out.push_back({"zwei-haltestellen-wechsel-2", in});

  in = base();
  in.wifi = WifiState::Connecting;
  addStop(in, stop("Emsdetten, Bahnhof", emsdetten()));
  in.stops[0].status.stale = true;
  in.stops[0].status.offline = true;
  in.stops[0].lastSuccess = kNow - 6 * 60;
  out.push_back({"veraltet-kein-wlan", in});
  in.wifi = WifiState::Online;
  in.stops[0].status.offline = false;
  in.cfg.theme = DisplayTheme::Night;
  out.push_back({"veraltet-api-nacht", in});

  in = base();
  addStop(in, stop("Lübeck Hbf", langeNamen()));
  out.push_back({"lange-namen", in});

  in = base();
  in.wifi = WifiState::Provisioning;
  in.timeValid = false;
  out.push_back({"einrichtung-wlan", in});

  in = base();
  in.setupRestart = true;
  out.push_back({"einrichtung-neustart", in});

  in = base();
  in.ip = "192.168.178.23";
  out.push_back({"einrichtung-haltestelle", in});

  in = base();
  in.wifi = WifiState::Fallback;
  addStop(in, stop("Emsdetten, Bahnhof", {}, BoardStatus::NoWifi));
  out.push_back({"wlan-nicht-erreichbar", in});

  in = base();
  in.timeValid = false;
  addStop(in, stop("Emsdetten, Bahnhof", {}, BoardStatus::NoTime));
  out.push_back({"meldung-uhrzeit", in});

  in = base();
  addStop(in, stop("Emsdetten, Bahnhof", {}, BoardStatus::Loading));
  out.push_back({"meldung-laden", in});

  in = base();
  addStop(in, stop("Emsdetten, Bahnhof", {}, BoardStatus::ApiError));
  out.push_back({"meldung-api-fehler", in});

  in = base();
  in.wifi = WifiState::Connecting;
  addStop(in, stop("Emsdetten, Bahnhof", {}, BoardStatus::NoWifi));
  out.push_back({"meldung-kein-wlan", in});

  in = base();
  addStop(in, stop("Emsdetten, Bahnhof", {}, BoardStatus::NoDepartures));
  out.push_back({"meldung-keine-abfahrten", in});
  in.cfg.theme = DisplayTheme::Night;
  out.push_back({"meldung-keine-abfahrten-nacht", in});

  in = base();
  addStop(in, stop("Emsdetten, Bahnhof", emsdetten()));
  in.update = UpdatePhase::Downloading;
  in.updatePercent = 64;
  out.push_back({"update-laeuft", in});
  in.cfg.theme = DisplayTheme::Night;
  in.update = UpdatePhase::Rebooting;
  out.push_back({"update-neustart-nacht", in});

  return out;
}

}  // namespace

int main(int argc, char **argv) {
  const std::string outDir = argc > 1 ? argv[1] : ".";
  setenv("TZ", kTzBerlin, 1);
  tzset();

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return sTick; });
  lv_display_t *disp = lv_display_create(ui::kWidth, ui::kHeight);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  static uint16_t buf[ui::kWidth * ui::kHeight];
  lv_display_set_buffers(disp, buf, nullptr, sizeof(buf), LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(disp, flush);

  int failed = 0;
  for (const Scenario &s : scenarios()) {
    ui::show(buildView(s.in));
    sTick += 1000;
    lv_refr_now(disp);
    const std::string path = outDir + "/" + s.file + ".ppm";
    if (writePpm(path)) {
      printf("%s\n", path.c_str());
    } else {
      fprintf(stderr, "Kann %s nicht schreiben\n", path.c_str());
      ++failed;
    }
  }
  return failed ? 1 : 0;
}
