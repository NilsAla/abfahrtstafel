#include "display_model.h"

#include <date_de.h>
#include <stdio.h>
#include <time.h>

namespace abf {

// --- Nachtzeit und Helligkeit ----------------------------------------------

bool inWindow(uint32_t start, uint32_t end, uint32_t minuteOfDay) {
  start %= kMinutesPerDay;
  end %= kMinutesPerDay;
  minuteOfDay %= kMinutesPerDay;
  if (start == end) return false;
  if (start < end) return minuteOfDay >= start && minuteOfDay < end;
  return minuteOfDay >= start || minuteOfDay < end;  // über Mitternacht
}

Backlight backlightFor(const Config &cfg, bool timeValid, uint32_t minuteOfDay) {
  Backlight b;
  b.night = cfg.nightEnabled && timeValid && inWindow(cfg.nightStart, cfg.nightEnd, minuteOfDay);
  if (b.night) {
    b.percent = static_cast<uint8_t>(cfg.nightBrightness > kBrightnessMax ? kBrightnessMax : cfg.nightBrightness);
    b.theme = DisplayTheme::Night;
  } else {
    uint32_t p = cfg.brightness;
    if (p < kBrightnessMin) p = kBrightnessMin;
    if (p > kBrightnessMax) p = kBrightnessMax;
    b.percent = static_cast<uint8_t>(p);
    b.theme = cfg.theme;
  }
  return b;
}

// --- Vergleich (UI zeichnet nur bei Änderungen neu) -------------------------

bool RowView::operator==(const RowView &o) const {
  return line == o.line && badgeBg == o.badgeBg && badgeFg == o.badgeFg && to == o.to && time == o.time &&
         unit == o.unit && note == o.note && planned == o.planned && noteKind == o.noteKind && track == o.track &&
         cancelled == o.cancelled;
}

bool SectionView::operator==(const SectionView &o) const {
  return title == o.title && rows == o.rows && message == o.message;
}

bool View::operator==(const View &o) const {
  return screen == o.screen && theme == o.theme && backlight == o.backlight && header == o.header &&
         title == o.title && clock == o.clock && date == o.date && wifi == o.wifi && updateAvailable == o.updateAvailable && page == o.page &&
         sections == o.sections && rows == o.rows && banner == o.banner && icon == o.icon &&
         heading == o.heading && text == o.text && steps == o.steps && qr == o.qr &&
         qrCaption == o.qrCaption && progress == o.progress;
}

// --- Bausteine -------------------------------------------------------------

std::string localHhMm(int64_t epoch) {
  const time_t t = static_cast<time_t>(epoch);
  struct tm tm;
  localtime_r(&t, &tm);
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
  return buf;
}

uint32_t localMinuteOfDay(int64_t epoch) {
  const time_t t = static_cast<time_t>(epoch);
  struct tm tm;
  localtime_r(&t, &tm);
  return static_cast<uint32_t>(tm.tm_hour * 60 + tm.tm_min);
}

uint32_t contrastColor(uint32_t bg) {
  // Wahrgenommene Helligkeit (ITU-R BT.601), reicht für Linien-Badges
  const uint32_t r = (bg >> 16) & 0xff;
  const uint32_t g = (bg >> 8) & 0xff;
  const uint32_t b = bg & 0xff;
  const uint32_t lum = (299 * r + 587 * g + 114 * b) / 1000;
  return lum > 150 ? 0x111111 : 0xffffff;
}

uint32_t modeColor(uint8_t mode) {
  switch (mode) {
    case kModeTrain:  return 0x6b7280;
    case kModeSubway: return 0x1d4ed8;
    case kModeTram:   return 0xb91c1c;
    case kModeBus:    return 0x7e3f98;
    case kModeFerry:  return 0x0e7490;
    default:          return 0x6b7280;
  }
}

std::string wifiQrPayload(const std::string &ssid) {
  // Sonderzeichen laut Spezifikation maskieren: \ ; , : "
  std::string esc;
  for (char c : ssid) {
    if (c == '\\' || c == ';' || c == ',' || c == ':' || c == '"') esc += '\\';
    esc += c;
  }
  return "WIFI:S:" + esc + ";T:nopass;;";
}

RowView makeRow(const BoardRow &r, bool compactTrack) {
  RowView v;
  v.line = r.line.empty() ? "?" : r.line;
  v.badgeBg = r.color >= 0 ? static_cast<uint32_t>(r.color) : modeColor(r.mode);
  v.badgeFg = r.textColor >= 0 ? static_cast<uint32_t>(r.textColor) : contrastColor(v.badgeBg);
  if (v.badgeFg == v.badgeBg) v.badgeFg = contrastColor(v.badgeBg);
  v.to = r.to;
  v.cancelled = r.cancelled;

  if (r.cancelled) {
    v.time = localHhMm(r.scheduled ? r.scheduled : r.when);
    v.note = "fällt aus";
    v.noteKind = NoteKind::Cancelled;
  } else {
    if (r.minutes <= 0) {
      v.time = "jetzt";
    } else if (r.minutes < kClockFromMinutes) {
      v.time = std::to_string(r.minutes);
      v.unit = "min";
    } else {
      v.time = localHhMm(r.when);
    }
    // Minuten enthalten die Verspätung schon. Statt „+4“ (wird als Aufschlag gelesen) neue Zeit und
    // durchgestrichene Planzeit; kleine Abweichungen bleiben unauffällig.
    if (r.realtime && r.delayMin >= kLateMinutes) {
      v.note = localHhMm(r.when);
      v.planned = localHhMm(r.scheduled);
      v.noteKind = NoteKind::Delay;
    }
  }

  if (!r.track.empty()) {
    if (compactTrack || r.track.size() > 4) {
      v.track = r.track;
    } else {
      const bool rail = r.mode == kModeTrain || r.mode == kModeSubway;
      v.track = std::string(rail ? "Gleis " : "Steig ") + r.track;
    }
  }
  return v;
}

size_t rowsNeeded(const Config &cfg) {
  const size_t rows = cfg.rows < kRowsMin ? kRowsMin : (cfg.rows > kRowsMax ? kRowsMax : cfg.rows);
  return rows > kSplitRows ? rows : kSplitRows;
}

// --- Bildaufbau ------------------------------------------------------------

namespace {

std::string staleText(const StopInput &s) {
  std::string t = s.status.offline ? "Keine WLAN-Verbindung" : "Fahrplandaten nicht erreichbar";
  if (s.lastSuccess > 0) t += " · Stand " + localHhMm(s.lastSuccess) + " Uhr";
  return t;
}

// Zweite Zeile unter der Klartext-Meldung: was passiert jetzt, was kann man tun
std::string statusHint(BoardStatus st) {
  switch (st) {
    case BoardStatus::NoWifi:
      return "Ist der Router eingeschaltet? Die Tafel verbindet sich automatisch wieder.";
    case BoardStatus::ApiError:
      return "Die Tafel versucht es gleich noch einmal.";
    case BoardStatus::NoDepartures:
      return "Neue Abfahrten erscheinen hier automatisch.";
    case BoardStatus::Loading:
      return "Einen Moment bitte.";
    default:
      return "";
  }
}

Icon statusIcon(BoardStatus st) {
  switch (st) {
    case BoardStatus::NoWifi:       return Icon::NoWifi;
    case BoardStatus::ApiError:     return Icon::Warning;
    case BoardStatus::Loading:      return Icon::Loading;
    case BoardStatus::NoTime:       return Icon::Clock;
    default:                        return Icon::Info;
  }
}

void setupScreen(View &v, const ViewInputs &in, bool fallback) {
  v.screen = Screen::Setup;
  v.wifi = WifiMark::None;
  v.qr = wifiQrPayload(in.apName);
  v.qrCaption = in.apName;
  if (fallback) {
    v.heading = "WLAN nicht erreichbar";
    v.steps = {"Router an? Dann einfach kurz warten.",
               "Neues WLAN? Code scannen oder WLAN „" + in.apName + "“ wählen",
               "Eigenes WLAN wählen, Passwort eingeben"};
  } else {
    v.heading = "Hallo! Kurz einrichten:";
    v.steps = {"Code scannen oder WLAN „" + in.apName + "“ wählen",
               "Eigenes WLAN wählen, Passwort eingeben",
               "Fertig – dann die Haltestelle einstellen"};
  }
}

void messageScreen(View &v, Icon icon, const std::string &heading, const std::string &text) {
  v.screen = Screen::Message;
  v.icon = icon;
  v.heading = heading;
  v.text = text;
}

}  // namespace

View buildView(const ViewInputs &in) {
  View v;
  const Config &cfg = in.cfg;
  const uint32_t minute = in.timeValid ? localMinuteOfDay(in.now) : 0;
  const Backlight bl = backlightFor(cfg, in.timeValid, minute);
  v.theme = bl.theme;
  v.backlight = bl.percent;
  v.rows = static_cast<uint8_t>(rowsNeeded(cfg));
  const bool online = in.wifi == WifiState::Online;

  // 1. Update hat Vorrang: Strom darf jetzt nicht weg
  if (in.update != UpdatePhase::None) {
    v.screen = Screen::Update;
    if (v.backlight == 0) v.backlight = kBrightnessMin;  // auch nachts sichtbar
    if (in.update == UpdatePhase::Rebooting) {
      v.heading = "Neustart …";
      v.text = "Gleich geht es weiter.";
      v.progress = 100;
    } else {
      v.heading = "Neue Version wird installiert";
      v.text = "Bitte Stecker nicht ziehen – dauert etwa eine Minute";
      v.progress = static_cast<int8_t>(in.update == UpdatePhase::Verifying ? 100
                                       : in.updatePercent > 100            ? 100
                                                                           : in.updatePercent);
    }
    return v;
  }

  // 2. Ersteinrichtung immer zeigen, auch nachts (Uhr kann nach Werksreset noch laufen)
  if (in.wifi == WifiState::Provisioning) {
    setupScreen(v, in, false);
    if (v.backlight == 0) v.backlight = kBrightnessMin;
    return v;
  }

  // 3. Nachtabschaltung
  if (v.backlight == 0) {
    v.screen = Screen::Off;
    return v;
  }

  v.clock = in.timeValid ? localHhMm(in.now) : "";
  // Bei zwei Haltestellen muss erkennbar bleiben, welche gerade gezeigt wird
  v.header = cfg.stopName == StopNameStyle::Off && cfg.stops.size() >= 2 ? StopNameStyle::Normal : cfg.stopName;
  if (in.timeValid) {
    const time_t t = static_cast<time_t>(in.now);
    struct tm tm;
    localtime_r(&t, &tm);
    v.date = v.header == StopNameStyle::Off ? formatDateLong(tm) : formatDateShort(tm);
  }
  v.wifi = online ? WifiMark::Ok : WifiMark::Off;
  v.updateAvailable = in.updateAvailable;

  bool anyRows = false;
  for (const StopInput &s : in.stops) anyRows |= !s.rows.empty();

  // 4. Router weg oder gewechselt, nichts mehr zum Anzeigen → Weg zur Neueinrichtung zeigen
  if (in.wifi == WifiState::Fallback && !anyRows) {
    setupScreen(v, in, true);
    return v;
  }

  // 5. Keine Haltestelle
  if (!cfg.stops.empty() && in.stops.empty()) {  // gerade gespeichert, Abruf-Task übernimmt sie noch
    v.title = cfg.stops.front().name;
    messageScreen(v, Icon::Loading, boardStatusText(BoardStatus::Loading), statusHint(BoardStatus::Loading));
    return v;
  }
  if (cfg.stops.empty()) {
    v.title = "Abfahrtstafel";
    if (!online) {
      messageScreen(v, Icon::NoWifi, "Keine WLAN-Verbindung.", statusHint(BoardStatus::NoWifi));
      return v;
    }
    const std::string url = "http://" + (in.hostname.empty() ? std::string("abfahrt") : in.hostname) + ".local";
    v.screen = Screen::NoStops;
    v.wifi = WifiMark::None;
    v.heading = "Fast geschafft!";
    v.steps = {"Handy mit demselben WLAN verbinden",
               "Code scannen oder " + url.substr(7) + " im Browser öffnen",
               "Haltestelle suchen und speichern"};
    v.qr = url;
    v.qrCaption = url.substr(7);
    return v;
  }

  // 6. Uhrzeit fehlt → Minuten nicht berechenbar
  if (!in.timeValid) {
    v.title = in.stops.front().name;
    messageScreen(v, Icon::Clock, "Uhrzeit wird gestellt …",
                  online ? "Einen Moment bitte." : "Dafür braucht die Tafel WLAN.");
    return v;
  }

  // 7. Abfahrten
  const bool split = in.stops.size() >= 2 && cfg.twoStops == TwoStopLayout::Split;
  if (split) {
    v.screen = Screen::Board;
    for (size_t i = 0; i < 2; ++i) {
      const StopInput &s = in.stops[i];
      SectionView sec;
      sec.title = s.name;
      if (s.status.stale && !s.rows.empty()) {
        sec.title += s.lastSuccess > 0 ? " · Stand " + localHhMm(s.lastSuccess) : " · veraltet";
      }
      for (size_t r = 0; r < s.rows.size() && r < kSplitRows; ++r) sec.rows.push_back(makeRow(s.rows[r], true));
      if (sec.rows.empty()) sec.message = boardStatusText(s.status.status);
      v.sections.push_back(std::move(sec));
    }
    return v;
  }

  size_t index = 0;
  if (in.stops.size() >= 2) {
    index = (in.uptimeS / kAlternateSeconds) % 2;
    v.page = std::to_string(index + 1) + "/2";
  }
  const StopInput &s = in.stops[index];
  v.title = s.name;
  if (s.rows.empty()) {
    const BoardStatus st = s.status.status == BoardStatus::Ok ? BoardStatus::NoDepartures : s.status.status;
    messageScreen(v, statusIcon(st), boardStatusText(st), statusHint(st));
    return v;
  }

  v.screen = Screen::Board;
  size_t rows = v.rows;
  if (s.status.stale) {
    v.banner = staleText(s);
    rows -= 1;
  }
  SectionView sec;
  for (size_t r = 0; r < s.rows.size() && r < rows; ++r) sec.rows.push_back(makeRow(s.rows[r], false));
  v.sections.push_back(std::move(sec));
  return v;
}

}  // namespace abf
