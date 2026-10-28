// Display: Was wird angezeigt? Reine Logik ohne LVGL, nativ getestet.
//
// buildView() fasst den Gerätezustand (WLAN, Uhrzeit, Update, Abfahrten, Einstellungen) zu einer
// fertigen Bildbeschreibung zusammen: welcher Bildschirm, welche Texte, welche Farben, welche
// Helligkeit. Die LVGL-Schicht (src/ui) zeichnet diese Beschreibung nur noch.
#pragma once

#include <app_config.h>
#include <board_status.h>
#include <departure_board.h>
#include <stdint.h>
#include <wifi_supervisor.h>

#include <string>
#include <vector>

namespace abf {

// --- Nachtzeit und Helligkeit ----------------------------------------------

// Liegt minuteOfDay im Fenster [start, end)? Fenster über Mitternacht (22:00–06:00) erlaubt.
// start == end → nie.
bool inWindow(uint32_t start, uint32_t end, uint32_t minuteOfDay);

struct Backlight {
  uint8_t percent = 80;  // 0 = Display aus
  DisplayTheme theme = DisplayTheme::Dark;
  bool night = false;    // Nachtzeitfenster aktiv
};

// Ohne gestellte Uhr gibt es keine Nacht (Gerät bleibt bedienbar).
Backlight backlightFor(const Config &cfg, bool timeValid, uint32_t minuteOfDay);

// --- Bildbeschreibung ------------------------------------------------------

enum class Screen : uint8_t {
  Off,      // Nachtabschaltung: schwarz, Hintergrundbeleuchtung aus
  Setup,    // WLAN einrichten (QR für das Einrichtungs-WLAN)
  NoStops,  // im WLAN, aber keine Haltestelle (QR für die Web-UI)
  Message,  // Klartext (Fehler, Laden, keine Abfahrten)
  Board,    // Abfahrten
  Update,   // Firmware wird installiert
};

enum class Icon : uint8_t { None, Info, Warning, Clock, Loading, NoWifi };
enum class WifiMark : uint8_t { None, Ok, Off };
enum class NoteKind : uint8_t { None, Delay, Cancelled };

struct RowView {
  std::string line;
  uint32_t badgeBg = 0;  // 0xRRGGBB
  uint32_t badgeFg = 0;
  std::string to;
  std::string time;      // "22", "jetzt", "13:27"
  std::string unit;      // "min" oder leer
  std::string note;      // neue Abfahrtszeit bei Verspätung ("12:42") bzw. "fällt aus"
  std::string planned;   // Planzeit bei Verspätung ("12:38", wird durchgestrichen)
  NoteKind noteKind = NoteKind::None;
  std::string track;     // "Gleis 1", "Steig B" bzw. kurz "B"
  bool cancelled = false;
  bool operator==(const RowView &o) const;
};

struct SectionView {
  std::string title;           // Haltestelle (bzw. leer, wenn im Kopf)
  std::vector<RowView> rows;
  std::string message;         // statt Zeilen (z. B. „Zurzeit keine Abfahrten.“)
  bool operator==(const SectionView &o) const;
};

struct View {
  Screen screen = Screen::Message;
  DisplayTheme theme = DisplayTheme::Dark;
  uint8_t backlight = 80;

  // Kopfzeile
  StopNameStyle header = StopNameStyle::Normal;  // Off: Titel ausgeblendet, langes Datum links
  std::string title;           // Haltestelle bzw. Gerätename
  std::string clock;           // "12:20" oder leer
  std::string date;            // "Di 27.10." bzw. bei Off "Dienstag, 27. Oktober"; leer ohne Uhrzeit
  WifiMark wifi = WifiMark::None;
  bool updateAvailable = false;
  std::string page;            // "1/2" bei Wechselanzeige

  // Board
  std::vector<SectionView> sections;
  uint8_t rows = 4;            // Zeilen-Layout (3 = große Schrift), geteilt immer 2 je Haltestelle
  std::string banner;          // gelbe Leiste unten (veraltete Daten)

  // Setup / NoStops / Message / Update
  Icon icon = Icon::None;
  std::string heading;
  std::string text;
  std::vector<std::string> steps;
  std::string qr;              // Inhalt des QR-Codes
  std::string qrCaption;
  int8_t progress = -1;        // 0..100, -1 = ohne Balken

  bool operator==(const View &o) const;
  bool operator!=(const View &o) const { return !(*this == o); }
};

struct StopInput {
  std::string name;                 // aus der Konfiguration
  std::vector<BoardRow> rows;       // prepareDepartures(), so viele wie maximal angezeigt
  StatusResult status;
  int64_t lastSuccess = 0;          // Unix-Zeit
};

enum class UpdatePhase : uint8_t { None, Downloading, Verifying, Rebooting };

struct ViewInputs {
  Config cfg;
  WifiState wifi = WifiState::Connecting;
  std::string apName;               // "Abfahrtstafel-AB0C"
  std::string hostname;             // aktiver Hostname (für http://<name>.local)
  bool timeValid = false;
  int64_t now = 0;                  // Unix-Zeit
  UpdatePhase update = UpdatePhase::None;
  uint8_t updatePercent = 0;
  bool updateAvailable = false;
  std::vector<StopInput> stops;     // gleiche Reihenfolge wie cfg.stops
  uint32_t uptimeS = 0;             // für die Wechselanzeige
};

constexpr uint32_t kAlternateSeconds = 10;    // Wechselanzeige: Sekunden je Haltestelle
constexpr int32_t kClockFromMinutes = 60;      // ab hier Uhrzeit statt Minuten
constexpr size_t kSplitRows = 2;               // geteilte Ansicht: Zeilen je Haltestelle
constexpr int32_t kLateMinutes = 2;            // ab hier gilt eine Abfahrt als verspätet (rot)

// Wie viele Abfahrten je Haltestelle aufbereitet werden müssen
size_t rowsNeeded(const Config &cfg);

View buildView(const ViewInputs &in);

// Bausteine (getestet)
std::string localHhMm(int64_t epoch);   // Ortszeit (TZ muss gesetzt sein)
uint32_t localMinuteOfDay(int64_t epoch);
RowView makeRow(const BoardRow &r, bool compactTrack);
uint32_t contrastColor(uint32_t bg);    // Schwarz oder Weiß
uint32_t modeColor(uint8_t mode);       // Ersatzfarbe ohne Linienfarbe
std::string wifiQrPayload(const std::string &ssid);  // WIFI:-Schema für offenes WLAN

}  // namespace abf
