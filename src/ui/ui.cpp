// Display-Oberfläche: zeichnet abf::View mit LVGL.
//
// Bei jeder Änderung wird der Bildschirm komplett neu aufgebaut. Das passiert höchstens einmal pro
// Sekunde (meist einmal pro Minute) und hält den Code frei von Zustands-Abgleich. Labels hängen
// direkt am Bildschirm, Flächen nur als Hintergrund – so wird nichts an Containergrenzen abgeschnitten.
#include "ui.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ui_fonts.h"

namespace ui {

namespace {

using abf::DisplayTheme;
using abf::Icon;
using abf::NoteKind;
using abf::RowView;
using abf::Screen;
using abf::SectionView;
using abf::StopNameStyle;
using abf::View;
using abf::WifiMark;

constexpr int32_t kMargin = 16;
constexpr int32_t kHeader = 44;       // Kopfzeile Vollbild
constexpr int32_t kHeaderLarge = 66;  // mit großem Haltestellennamen
constexpr int32_t kHeaderBare = 36;   // ohne Haltestellennamen, nur Datum und Uhr
constexpr int32_t kSplitHeader = 32;  // Kopfzeile je Haltestelle (geteilt)
constexpr int32_t kBanner = 40;       // Hinweisleiste unten
constexpr int32_t kRight = kWidth - kMargin;

struct Palette {
  uint32_t screen;     // Hintergrund
  uint32_t header;     // Kopfzeile
  uint32_t row;        // Zeilen abwechselnd
  uint32_t rowAlt;
  uint32_t text;
  uint32_t dim;        // Nebentexte
  uint32_t accent;     // Symbole, Leiste
  uint32_t late;       // Verspätung: Minuten und neue Abfahrtszeit
  uint32_t cancel;     // „fällt aus“
  uint32_t struck;     // durchgestrichene Abfahrt
  uint32_t bannerText;
  uint32_t track;      // Hintergrund Fortschrittsbalken
  uint32_t qrBox;      // Fläche hinter dem QR-Code
};

// Dunkel: Nachtblau wie die Mockups. Nacht: Bernstein auf Schwarz, kaum Blauanteil.
constexpr Palette kDark = {0x0b1a2e, 0x050f1e, 0x0b1a2e, 0x12243c, 0xf2f4f7, 0x9fb1c9,
                           0xffb020, 0xff5a4a, 0xff7a6b, 0x8193ab, 0x1a1306, 0x1e3858, 0xffffff};
constexpr Palette kNight = {0x000000, 0x000000, 0x090806, 0x15110b, 0xffb000, 0xc28a1c,
                            0xffe08a, 0xff6a4a, 0xff7a6b, 0x8f6a1e, 0x000000, 0x2a2010, 0xf2e2c0};

// Maße je Zeilen-Layout
struct RowMetrics {
  const lv_font_t *time;
  const lv_font_t *unit;
  const lv_font_t *badge;
  const lv_font_t *dest;
  int32_t timeRight;  // rechte Kante der Zeitspalte
  int32_t badgeW;
  int32_t badgeH;
};

const RowMetrics kRows4 = {&ui_font_time4, &ui_font_small, &ui_font_badge4, &ui_font_row4, 140, 104, 46};
const RowMetrics kRows3 = {&ui_font_time3, &ui_font_body, &ui_font_badge3, &ui_font_row3, 172, 132, 60};
const RowMetrics kRowsSplit = {&ui_font_time4, &ui_font_small, &ui_font_badge4, &ui_font_row4, 140, 104, 44};

View sLast;
bool sValid = false;

// Statische Linienpunkte: lv_line merkt sich nur den Zeiger
lv_point_precise_t sSlashSmall[2];
lv_point_precise_t sSlashBig[2];

// --- Bausteine -------------------------------------------------------------

lv_color_t c(uint32_t rgb) { return lv_color_hex(rgb); }

lv_obj_t *box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t bg, int32_t radius = 0) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, c(bg), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, radius, 0);
  return o;
}

lv_obj_t *label(lv_obj_t *parent, const std::string &text, const lv_font_t *font, uint32_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_remove_style_all(l);
  lv_label_set_text(l, text.c_str());
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, c(color), 0);
  return l;
}

// Einzeilig mit „…“ am Ende. Ohne feste Höhe würde LVGL umbrechen statt zu kürzen.
void singleLine(lv_obj_t *l, const lv_font_t *font, int32_t width) {
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_size(l, width, font->line_height);
}

int32_t textWidth(const std::string &text, const lv_font_t *font) {
  lv_point_t size;
  lv_text_get_size(&size, text.c_str(), font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return size.x;
}

int32_t textHeight(const std::string &text, const lv_font_t *font, int32_t maxWidth) {
  lv_point_t size;
  lv_text_get_size(&size, text.c_str(), font, 0, 0, maxWidth, LV_TEXT_FLAG_NONE);
  return size.y;
}

// Höhe eines Großbuchstabens bzw. einer Ziffer über der Grundlinie
int32_t capHeight(const lv_font_t *font) {
  lv_font_glyph_dsc_t g;
  if (lv_font_get_glyph_dsc(font, &g, 'H', 0) && g.box_h > 0) return g.box_h;
  if (lv_font_get_glyph_dsc(font, &g, '0', 0) && g.box_h > 0) return g.box_h;
  return font->line_height - font->base_line;
}

// Label so setzen, dass seine Grundlinie bei baselineY liegt
void placeBaseline(lv_obj_t *l, const lv_font_t *font, int32_t x, int32_t baselineY) {
  lv_obj_set_pos(l, x, baselineY - (font->line_height - font->base_line));
}

// Rechtsbündig an rightX; gibt die linke Kante zurück
int32_t placeRight(lv_obj_t *l, const std::string &text, const lv_font_t *font, int32_t rightX, int32_t baselineY) {
  const int32_t w = textWidth(text, font);
  placeBaseline(l, font, rightX - w, baselineY);
  return rightX - w;
}

void slash(lv_obj_t *parent, lv_point_precise_t *pts, int32_t x, int32_t y, int32_t size, int32_t width,
           uint32_t color) {
  pts[0] = {static_cast<lv_value_precise_t>(x), static_cast<lv_value_precise_t>(y)};
  pts[1] = {static_cast<lv_value_precise_t>(x + size), static_cast<lv_value_precise_t>(y + size)};
  lv_obj_t *line = lv_line_create(parent);
  lv_obj_remove_style_all(line);
  lv_line_set_points(line, pts, 2);
  lv_obj_set_style_line_width(line, width, 0);
  lv_obj_set_style_line_color(line, c(color), 0);
  lv_obj_set_style_line_rounded(line, true, 0);
}

// --- Kopfzeile -------------------------------------------------------------

// Rechts: [WLAN] [Update] [1/2] Uhrzeit. Gibt die linke Kante des belegten Bereichs zurück.
int32_t headerRight(lv_obj_t *scr, const View &v, const Palette &p, int32_t top, int32_t height, bool icons) {
  int32_t x = kRight;
  const int32_t clockBase = top + (height + capHeight(&ui_font_clock)) / 2;
  const int32_t smallBase = top + (height + capHeight(&ui_font_small)) / 2;
  if (!v.clock.empty()) {
    x = placeRight(label(scr, v.clock, &ui_font_clock, p.text), v.clock, &ui_font_clock, x, clockBase) - 20;
  }
  // Kurzes Datum neben der Uhr; ohne Haltestellennamen steht das lange Datum links (siehe header())
  if (!v.date.empty() && v.header != StopNameStyle::Off) {
    x = placeRight(label(scr, v.date, &ui_font_small, p.dim), v.date, &ui_font_small, x, smallBase) - 20;
  }
  if (!icons) return x;
  if (!v.page.empty()) {
    x = placeRight(label(scr, v.page, &ui_font_small, p.dim), v.page, &ui_font_small, x, smallBase) - 18;
  }
  if (v.updateAvailable) {
    x = placeRight(label(scr, UI_ICON_DOWNLOAD, &ui_font_small, p.accent), UI_ICON_DOWNLOAD, &ui_font_small, x,
                   smallBase) - 18;
  }
  if (v.wifi != WifiMark::None) {
    const bool off = v.wifi == WifiMark::Off;
    const int32_t left = placeRight(label(scr, UI_ICON_WIFI, &ui_font_small, off ? p.accent : p.dim), UI_ICON_WIFI,
                                    &ui_font_small, x, smallBase);
    if (off) {
      const int32_t s = textWidth(UI_ICON_WIFI, &ui_font_small);
      slash(scr, sSlashSmall, left, smallBase - s + 2, s - 2, 3, p.accent);
    }
    x = left - 18;
  }
  return x;
}

int32_t headerHeight(const View &v) {
  switch (v.header) {
    case StopNameStyle::Large: return kHeaderLarge;
    case StopNameStyle::Off:   return kHeaderBare;
    default:                   return kHeader;
  }
}

void header(lv_obj_t *scr, const View &v, const Palette &p, const std::string &title, int32_t top, int32_t height,
            bool icons) {
  box(scr, 0, top, kWidth, height, p.header);
  const int32_t rightEdge = headerRight(scr, v, p, top, height, icons);
  // Links: Haltestelle (klein und grau oder groß und hell) bzw. bei ausgeblendetem Namen das lange Datum
  const bool bare = v.header == StopNameStyle::Off;
  const std::string &text = bare ? v.date : title;
  if (text.empty()) return;
  const bool large = v.header == StopNameStyle::Large && height >= kHeaderLarge;
  const lv_font_t *font = large ? &ui_font_title : &ui_font_small;
  lv_obj_t *t = label(scr, text, font, large ? p.text : p.dim);
  singleLine(t, font, std::max<int32_t>(40, rightEdge - 20 - (kMargin + 4)));
  placeBaseline(t, font, kMargin + 4, top + (height + capHeight(font)) / 2);
}

// --- Abfahrtszeile ---------------------------------------------------------

struct Line {
  std::string text;
  const lv_font_t *font;
  uint32_t color;
  bool struck = false;
};

void row(lv_obj_t *scr, const RowView &r, const RowMetrics &m, const Palette &p, int32_t top, int32_t height,
         bool alt) {
  box(scr, 0, top, kWidth, height, alt ? p.rowAlt : p.row);
  const int32_t center = top + height / 2;
  const int32_t base = center + capHeight(m.dest) / 2;
  const uint32_t mainColor = r.cancelled ? p.struck : p.text;
  const bool late = r.noteKind == NoteKind::Delay;

  // Zeit: große Zahl + „min“ auf gemeinsamer Grundlinie, rechtsbündig. Rot bei Verspätung:
  // die Minuten enthalten sie schon, nichts muss gerechnet werden.
  int32_t timeRight = m.timeRight;
  if (!r.unit.empty()) {
    const uint32_t unitColor = r.cancelled ? p.struck : late ? p.late : p.dim;
    timeRight = placeRight(label(scr, r.unit, m.unit, unitColor), r.unit, m.unit, timeRight, base) - 6;
  }
  lv_obj_t *time = label(scr, r.time, m.time, late ? p.late : mainColor);
  placeRight(time, r.time, m.time, timeRight, base);
  if (r.cancelled) lv_obj_set_style_text_decor(time, LV_TEXT_DECOR_STRIKETHROUGH, 0);

  // Linie: farbiges Badge, wächst bei langen Namen bis 170 px
  const int32_t badgeX = m.timeRight + 18;
  const int32_t badgeW = std::min<int32_t>(170, std::max(m.badgeW, textWidth(r.line, m.badge) + 20));
  lv_obj_t *badge = box(scr, badgeX, top + (height - m.badgeH) / 2, badgeW, m.badgeH, r.badgeBg, 7);
  lv_obj_t *bl = label(badge, r.line, m.badge, r.badgeFg);
  lv_obj_align(bl, LV_ALIGN_CENTER, 0, 1);

  // Rechte Spalte: Ausfall, Verspätung (Planzeit durchgestrichen über neuer Zeit) oder Gleis.
  // Für Planzeit, neue Zeit und Gleis reicht die Zeilenhöhe nicht – bei Verspätung entfällt das Gleis.
  std::vector<Line> lines;
  if (r.noteKind == NoteKind::Cancelled) {
    lines.push_back({"fällt", &ui_font_bold, p.cancel});
    lines.push_back({"aus", &ui_font_bold, p.cancel});
  } else if (late) {
    lines.push_back({r.planned, &ui_font_small, p.dim, true});
    lines.push_back({r.note, &ui_font_bold, p.late});
  } else if (!r.track.empty()) {
    lines.push_back({r.track, m.unit, p.dim});
  }
  int32_t rightW = 0;
  int32_t stackH = 0;
  constexpr int32_t kGap = 10;
  for (const Line &l : lines) {
    rightW = std::max(rightW, textWidth(l.text, l.font));
    stackH += capHeight(l.font);
  }
  if (lines.size() > 1) stackH += kGap * static_cast<int32_t>(lines.size() - 1);
  int32_t y = center - stackH / 2;
  for (const Line &l : lines) {
    y += capHeight(l.font);
    lv_obj_t *t = label(scr, l.text, l.font, l.color);
    if (l.struck) lv_obj_set_style_text_decor(t, LV_TEXT_DECOR_STRIKETHROUGH, 0);
    placeRight(t, l.text, l.font, kRight, y);
    y += kGap;
  }

  // Ziel: füllt den Rest, zu lange Namen enden mit …
  const int32_t destX = badgeX + badgeW + 20;
  const int32_t destW = kRight - (rightW > 0 ? rightW + 20 : 0) - destX;
  lv_obj_t *dest = label(scr, r.to, m.dest, mainColor);
  singleLine(dest, m.dest, std::max<int32_t>(40, destW));
  placeBaseline(dest, m.dest, destX, base);
  if (r.cancelled) lv_obj_set_style_text_decor(dest, LV_TEXT_DECOR_STRIKETHROUGH, 0);
}

// --- Bildschirme -----------------------------------------------------------

void drawBoard(lv_obj_t *scr, const View &v, const Palette &p) {
  if (v.sections.size() >= 2) {  // geteilt: je Haltestelle Kopf + 2 Zeilen
    const int32_t half = kHeight / 2;
    const int32_t rowH = (half - kSplitHeader) / static_cast<int32_t>(abf::kSplitRows);
    for (size_t i = 0; i < v.sections.size() && i < 2; ++i) {
      const SectionView &s = v.sections[i];
      const int32_t top = static_cast<int32_t>(i) * half;
      View head = v;
      if (i > 0) {
        head.clock.clear();
        head.date.clear();
      }
      header(scr, head, p, s.title, top, kSplitHeader, i == 0);
      if (s.rows.empty()) {
        box(scr, 0, top + kSplitHeader, kWidth, half - kSplitHeader, p.row);
        lv_obj_t *m = label(scr, s.message, &ui_font_body, p.dim);
        singleLine(m, &ui_font_body, kWidth - 2 * kMargin - 4);
        placeBaseline(m, &ui_font_body, kMargin + 4,
                      top + kSplitHeader + (half - kSplitHeader + capHeight(&ui_font_body)) / 2);
        continue;
      }
      for (size_t r = 0; r < s.rows.size(); ++r) {
        row(scr, s.rows[r], kRowsSplit, p, top + kSplitHeader + static_cast<int32_t>(r) * rowH, rowH, r % 2 == 1);
      }
    }
    return;
  }

  const int32_t headH = headerHeight(v);
  header(scr, v, p, v.title, 0, headH, true);
  const RowMetrics &m = v.rows <= 3 ? kRows3 : kRows4;
  const int32_t bottom = v.banner.empty() ? kHeight : kHeight - kBanner;
  const int32_t slots = v.banner.empty() ? v.rows : v.rows - 1;
  const int32_t rowH = (bottom - headH) / std::max<int32_t>(1, slots);
  box(scr, 0, headH, kWidth, bottom - headH, p.row);
  if (!v.sections.empty()) {
    const SectionView &s = v.sections.front();
    for (size_t r = 0; r < s.rows.size(); ++r) {
      row(scr, s.rows[r], m, p, headH + static_cast<int32_t>(r) * rowH, rowH, r % 2 == 1);
    }
  }
  if (!v.banner.empty()) {
    box(scr, 0, bottom, kWidth, kBanner, p.accent);
    lv_obj_t *b = label(scr, v.banner, &ui_font_bold, p.bannerText);
    singleLine(b, &ui_font_bold, kWidth - 2 * kMargin);
    lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_CENTER, 0);
    placeBaseline(b, &ui_font_bold, kMargin, bottom + (kBanner + capHeight(&ui_font_bold)) / 2);
  }
}

const char *iconGlyph(Icon i) {
  switch (i) {
    case Icon::Warning: return UI_ICON_WARNING;
    case Icon::Clock:   return UI_ICON_CLOCK;
    case Icon::Loading: return UI_ICON_SYNC;
    case Icon::NoWifi:  return UI_ICON_WIFI;
    case Icon::Info:    return UI_ICON_INFO;
    default:            return "";
  }
}

// Symbol, Überschrift und Text untereinander, vertikal zentriert zwischen top und bottom
struct Symbol {
  std::string glyph;
  uint32_t color;
  bool slashed;  // durchgestrichen (WLAN weg)
};

Symbol symbolFor(Icon icon, const Palette &p) {
  const bool calm = icon == Icon::Info || icon == Icon::Loading || icon == Icon::Clock;
  return {iconGlyph(icon), calm ? p.dim : p.accent, icon == Icon::NoWifi};
}

void centeredStack(lv_obj_t *scr, const Palette &p, const Symbol &sym, const std::string &heading,
                   const std::string &text, int32_t top, int32_t bottom, int8_t progress) {
  constexpr int32_t kTextW = kWidth - 4 * kMargin;
  constexpr int32_t kBarH = 22;
  const std::string &glyph = sym.glyph;
  const int32_t iconH = glyph.empty() ? 0 : capHeight(&ui_font_icon);
  const int32_t headH = heading.empty() ? 0 : textHeight(heading, &ui_font_title, kTextW);
  const int32_t textH = text.empty() ? 0 : textHeight(text, &ui_font_body, kTextW);
  const int32_t barH = progress >= 0 ? kBarH + 26 : 0;
  int32_t total = iconH + headH + textH + barH;
  if (iconH && headH) total += 22;
  if (headH && textH) total += 10;
  int32_t y = top + (bottom - top - total) / 2;

  if (iconH) {
    lv_obj_t *l = label(scr, glyph, &ui_font_icon, sym.color);
    const int32_t w = textWidth(glyph, &ui_font_icon);
    placeBaseline(l, &ui_font_icon, (kWidth - w) / 2, y + iconH);
    if (sym.slashed) slash(scr, sSlashBig, (kWidth - w) / 2 + 4, y + 2, iconH - 4, 7, p.accent);
    y += iconH + (headH ? 22 : 0);
  }
  if (headH) {
    lv_obj_t *l = label(scr, heading, &ui_font_title, p.text);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, kTextW);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, 2 * kMargin, y);
    y += headH + (textH || barH ? 10 : 0);
  }
  if (progress >= 0) {
    y += 12;
    const int32_t barW = kWidth - 120;
    box(scr, 60, y, barW, kBarH, p.track, kBarH / 2);
    if (progress > 0) box(scr, 60, y, std::max<int32_t>(kBarH, barW * progress / 100), kBarH, p.accent, kBarH / 2);
    y += kBarH + 14;
  }
  if (textH) {
    lv_obj_t *l = label(scr, text, &ui_font_body, p.dim);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, kTextW);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, 2 * kMargin, y);
  }
}

void drawMessage(lv_obj_t *scr, const View &v, const Palette &p) {
  constexpr int32_t kAddressH = 44;
  const int32_t headH = headerHeight(v);
  header(scr, v, p, v.title, 0, headH, true);
  const int32_t bottom = v.address.empty() ? kHeight : kHeight - kAddressH;
  centeredStack(scr, p, symbolFor(v.icon, p), v.heading, v.text, headH, bottom, -1);
  if (!v.address.empty()) {
    // Adresse der Web-UI, damit man die Tafel auch ohne funktionierendes .local findet
    lv_obj_t *a = label(scr, v.address, &ui_font_small, p.dim);
    singleLine(a, &ui_font_small, kWidth - 2 * kMargin);
    lv_obj_set_style_text_align(a, LV_TEXT_ALIGN_CENTER, 0);
    placeBaseline(a, &ui_font_small, kMargin, kHeight - (kAddressH - capHeight(&ui_font_small)) / 2);
  }
}

void drawUpdate(lv_obj_t *scr, const View &v, const Palette &p) {
  std::string text = v.text;
  if (v.progress >= 0 && v.progress < 100) text = std::to_string(v.progress) + " % · " + text;
  centeredStack(scr, p, {UI_ICON_DOWNLOAD, p.accent, false}, v.heading, text, 0, kHeight, v.progress);
}

// QR links, nummerierte Schritte rechts
void drawSetup(lv_obj_t *scr, const View &v, const Palette &p) {
  constexpr int32_t kBox = 232;
  constexpr int32_t kBoxX = 28;
  constexpr int32_t kBoxY = 22;
  lv_obj_t *qrBox = box(scr, kBoxX, kBoxY, kBox, kBox, p.qrBox, 14);
  lv_obj_t *qr = lv_qrcode_create(qrBox);
  lv_qrcode_set_size(qr, kBox - 28);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, c(p.qrBox));
  lv_qrcode_update(qr, v.qr.data(), static_cast<uint32_t>(v.qr.size()));
  lv_obj_center(qr);
  if (!v.qrCaption.empty()) {
    lv_obj_t *cap = label(scr, v.qrCaption, &ui_font_small, p.dim);
    singleLine(cap, &ui_font_small, kBox + 20);
    lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, 0);
    placeBaseline(cap, &ui_font_small, kBoxX - 10, kBoxY + kBox + 8 + capHeight(&ui_font_small));
  }

  constexpr int32_t kTextX = kBoxX + kBox + 36;
  constexpr int32_t kNum = 40;
  constexpr int32_t kStepX = kTextX + kNum + 16;
  constexpr int32_t kStepW = kRight - kStepX;
  // Schritte in Fließtextgröße; passt das nicht, eine Nummer kleiner
  const lv_font_t *font = &ui_font_body;
  auto measure = [&](const lv_font_t *f) {
    int32_t t = capHeight(&ui_font_title) + 22;
    for (const std::string &s : v.steps) t += textHeight(s, f, kStepW) + 12;
    return t - 12;
  };
  int32_t total = measure(font);
  if (total > kHeight - 24) {
    font = &ui_font_small;
    total = measure(font);
  }
  const int32_t lineH = font->line_height;
  int32_t y = std::max<int32_t>(10, (kHeight - total) / 2);

  lv_obj_t *h = label(scr, v.heading, &ui_font_title, p.text);
  placeBaseline(h, &ui_font_title, kTextX, y + capHeight(&ui_font_title));
  y += capHeight(&ui_font_title) + 22;

  int n = 1;
  for (const std::string &s : v.steps) {
    const int32_t sh = textHeight(s, font, kStepW);
    // Nummer mittig zur ersten Textzeile
    lv_obj_t *num = box(scr, kTextX, y + (lineH - kNum) / 2, kNum, kNum, p.accent, LV_RADIUS_CIRCLE);
    lv_obj_t *nl = label(num, std::to_string(n++), &ui_font_bold, p.bannerText);
    lv_obj_align(nl, LV_ALIGN_CENTER, 0, 1);
    lv_obj_t *t = label(scr, s, font, p.text);
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(t, kStepW);
    lv_obj_set_pos(t, kStepX, y);
    y += sh + 12;
  }
}

}  // namespace

void invalidate() { sValid = false; }

bool show(const View &v) {
  if (sValid && v == sLast) return false;
  sLast = v;
  sValid = true;

  const Palette &p = v.theme == DisplayTheme::Night ? kNight : kDark;
  lv_obj_t *scr = lv_screen_active();
  lv_obj_clean(scr);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr, c(v.screen == Screen::Off ? 0x000000 : p.screen), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  switch (v.screen) {
    case Screen::Off:      break;
    case Screen::Setup:
    case Screen::NoStops:  drawSetup(scr, v, p); break;
    case Screen::Message:  drawMessage(scr, v, p); break;
    case Screen::Board:    drawBoard(scr, v, p); break;
    case Screen::Update:   drawUpdate(scr, v, p); break;
  }
  return true;
}

}  // namespace ui
