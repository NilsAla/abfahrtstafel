// Display-Oberfläche: zeichnet abf::View mit LVGL.
//
// Bei jeder Änderung wird der Bildschirm komplett neu aufgebaut. Das passiert höchstens einmal pro
// Sekunde (meist einmal pro Minute) und hält den Code frei von Zustands-Abgleich. Labels hängen
// direkt am Bildschirm, Flächen nur als Hintergrund – so wird nichts an Containergrenzen abgeschnitten.
#include "ui.h"

#include <algorithm>
#include <string>
#include <vector>

#include <tokens.h>

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

// wannbus-Design (docs/branding/BRANDING.md, Abschnitt 5): Karten mit runden Ecken auf ruhigem Grund,
// Linien-Badge in Creme bzw. Navy, Haltestellen-Ring im Kopf. Schriften bewusst groß (am Gerät geprüft):
// keine kleine Unterzeile, Verspätung färbt die Minuten.
constexpr int32_t kPadX = 12;
constexpr int32_t kPadY = 8;
constexpr int32_t kGap = 6;            // zwischen Kopf und Karten bzw. Karten untereinander
constexpr int32_t kHead = 36;          // Kopfzeile Vollbild
constexpr int32_t kHeadLarge = 50;     // mit großem Haltestellennamen
constexpr int32_t kHeadSplit = 30;     // Kopf je Haltestelle (geteilt)
constexpr int32_t kRing = 28;          // Haltestellen-Ring
constexpr int32_t kBannerH = 40;       // Hinweis unten (veraltete Daten)
constexpr int32_t kRight = kWidth - kPadX;

struct Palette {
  uint32_t screen;     // Hintergrund
  uint32_t card;
  uint32_t cardHot;    // erste Abfahrt
  uint32_t text;
  uint32_t muted;      // Nebentexte
  uint32_t teal;       // „pünktlich“, Ring, Aktionsfarbe (Textkontrast)
  uint32_t coral;      // Verspätung, Ausfall, Fehler (Textkontrast)
  uint32_t border;     // Fortschrittsbalken-Hintergrund
  uint32_t badgeBg;
  uint32_t badgeText;
};

Palette paletteFor(const abf::brand::ThemeTokens &t) {
  return {t.bgDisplay, t.card, t.cardHot, t.text, t.muted, t.tealText, t.coralText, t.border, t.badgeBg, t.badgeText};
}

const Palette kNight = paletteFor(abf::brand::kNight);
const Palette kDay = paletteFor(abf::brand::kDay);

// Maße je Zeilen-Layout
struct RowMetrics {
  const lv_font_t *dest;
  const lv_font_t *time;
  const lv_font_t *badge;
  const lv_font_t *unit;  // „min“, Gleis
  int32_t badgeH;
  int32_t badgeMinW;
  int32_t radius;
  int32_t padX;
};

const RowMetrics kRows3 = {&ui_font_dest3, &ui_font_min3, &ui_font_badge, &ui_font_body, 58, 80, 18, 18};
const RowMetrics kRows4 = {&ui_font_dest4, &ui_font_min4, &ui_font_badge_s, &ui_font_small, 44, 64, 14, 14};

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

// Rechts: [WLAN] [Update] [1/2] [Datum] Uhrzeit. Gibt die linke Kante des belegten Bereichs zurück.
int32_t headerRight(lv_obj_t *scr, const View &v, const Palette &p, int32_t top, int32_t height, bool icons) {
  int32_t x = kRight;
  const int32_t clockBase = top + (height + capHeight(&ui_font_clock)) / 2;
  const int32_t smallBase = top + (height + capHeight(&ui_font_small)) / 2;
  if (!v.clock.empty()) {
    x = placeRight(label(scr, v.clock, &ui_font_clock, p.text), v.clock, &ui_font_clock, x, clockBase) - 18;
  }
  // Kurzes Datum neben der Uhr; ohne Haltestellennamen steht das lange Datum links (siehe header())
  if (!v.date.empty() && v.header != StopNameStyle::Off) {
    x = placeRight(label(scr, v.date, &ui_font_small, p.muted), v.date, &ui_font_small, x, smallBase) - 18;
  }
  if (!icons) return x;
  if (!v.page.empty()) {
    x = placeRight(label(scr, v.page, &ui_font_small, p.muted), v.page, &ui_font_small, x, smallBase) - 16;
  }
  if (v.updateAvailable) {
    x = placeRight(label(scr, UI_ICON_DOWNLOAD, &ui_font_small, p.teal), UI_ICON_DOWNLOAD, &ui_font_small, x,
                   smallBase) - 16;
  }
  if (v.wifi != WifiMark::None) {
    const bool off = v.wifi == WifiMark::Off;
    const int32_t left = placeRight(label(scr, UI_ICON_WIFI, &ui_font_small, p.muted), UI_ICON_WIFI, &ui_font_small, x,
                                    smallBase);
    if (off) {
      const int32_t s = textWidth(UI_ICON_WIFI, &ui_font_small);
      slash(scr, sSlashSmall, left, smallBase - s + 2, s - 2, 3, p.coral);
    }
    x = left - 16;
  }
  return x;
}

int32_t headerHeight(const View &v) { return v.header == StopNameStyle::Large ? kHeadLarge : kHead; }

// Links: Haltestellen-Ring + Name (bzw. bei ausgeblendetem Namen das lange Datum), rechts Symbole und Uhr
void header(lv_obj_t *scr, const View &v, const Palette &p, const std::string &title, int32_t top, int32_t height,
            bool icons) {
  const int32_t rightEdge = headerRight(scr, v, p, top, height, icons);
  const bool bare = v.header == StopNameStyle::Off;
  const std::string &text = bare ? v.date : title;
  if (text.empty()) return;
  int32_t x = kPadX;
  if (!bare) {
    const int32_t ring = std::min(kRing, height);
    lv_obj_t *o = box(scr, x, top + (height - ring) / 2, ring, ring, p.screen, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_color(o, c(abf::brand::kTeal), 0);
    lv_obj_set_style_border_width(o, 3, 0);
    lv_obj_t *h = label(o, "H", &ui_font_ring, p.text);
    lv_obj_align(h, LV_ALIGN_CENTER, 0, 1);
    x += ring + 12;
  }
  const bool large = v.header == StopNameStyle::Large && height >= kHeadLarge;
  const lv_font_t *font = large ? &ui_font_dest4 : &ui_font_small;
  lv_obj_t *t = label(scr, text, font, large ? p.text : p.muted);
  singleLine(t, font, std::max<int32_t>(40, rightEdge - 16 - x));
  placeBaseline(t, font, x, top + (height + capHeight(font)) / 2);
}

// --- Abfahrtskarte ---------------------------------------------------------

// Badge-Breite einer Zeile; alle Karten einer Haltestelle nutzen die breiteste, damit die Ziele fluchten
int32_t badgeWidth(const RowView &r, const RowMetrics &m) {
  return std::min<int32_t>(170, std::max(m.badgeMinW, textWidth(r.line, m.badge) + 24));
}

void row(lv_obj_t *scr, const RowView &r, const RowMetrics &m, const Palette &p, int32_t x, int32_t top, int32_t w,
         int32_t height, int32_t badgeW, bool hot) {
  box(scr, x, top, w, height, hot ? p.cardHot : p.card, m.radius);
  const bool late = r.noteKind == NoteKind::Delay;
  const uint32_t mainColor = r.cancelled ? p.muted : p.text;

  // Rechts: Minuten + „min“ (bzw. Uhrzeit, „jetzt“) auf gemeinsamer Grundlinie, daneben Steig/Gleis.
  // Verspätung: Minuten in Koralle (sie enthalten die Verspätung schon). Ausfall: Zeit durchgestrichen + „fällt aus“.
  const int32_t timeBase = top + (height + capHeight(m.time)) / 2;
  int32_t right = x + w - m.padX;
  if (r.cancelled) {
    right = placeRight(label(scr, "fällt aus", &ui_font_bold, p.coral), "fällt aus", &ui_font_bold, right, timeBase) -
            14;
  } else if (!r.track.empty()) {
    right = placeRight(label(scr, r.track, m.unit, p.muted), r.track, m.unit, right, timeBase) - 14;
  }
  const uint32_t timeColor = late ? p.coral : mainColor;
  if (!r.unit.empty()) {
    right = placeRight(label(scr, r.unit, m.unit, late ? p.coral : p.muted), r.unit, m.unit, right, timeBase) - 6;
  }
  lv_obj_t *time = label(scr, r.time, m.time, timeColor);
  const int32_t timeLeft = placeRight(time, r.time, m.time, right, timeBase);
  if (r.cancelled) lv_obj_set_style_text_decor(time, LV_TEXT_DECOR_STRIKETHROUGH, 0);

  // Links: Linien-Badge (gemeinsame Breite der Haltestelle, siehe badgeWidth)
  const int32_t badgeX = x + m.padX;
  lv_obj_t *badge = box(scr, badgeX, top + (height - m.badgeH) / 2, badgeW, m.badgeH, p.badgeBg, 11);
  lv_obj_t *bl = label(badge, r.line, m.badge, p.badgeText);
  lv_obj_align(bl, LV_ALIGN_CENTER, 0, 1);

  // Mitte: Ziel, zu lange Namen enden mit …
  const int32_t destX = badgeX + badgeW + 16;
  lv_obj_t *dest = label(scr, r.to, m.dest, mainColor);
  singleLine(dest, m.dest, std::max<int32_t>(40, timeLeft - 16 - destX));
  placeBaseline(dest, m.dest, destX, top + (height + capHeight(m.dest)) / 2);
  if (r.cancelled) lv_obj_set_style_text_decor(dest, LV_TEXT_DECOR_STRIKETHROUGH, 0);
}

// Karten untereinander zwischen top und bottom; erste Karte hervorgehoben
void cards(lv_obj_t *scr, const SectionView &s, const RowMetrics &m, const Palette &p, int32_t top, int32_t bottom,
           int32_t slots) {
  const int32_t rowH = (bottom - top - (slots - 1) * kGap) / std::max<int32_t>(1, slots);
  if (s.rows.empty()) {
    lv_obj_t *msg = label(scr, s.message, &ui_font_body, p.muted);
    singleLine(msg, &ui_font_body, kRight - kPadX);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    placeBaseline(msg, &ui_font_body, kPadX, top + (bottom - top + capHeight(&ui_font_body)) / 2);
    return;
  }
  const size_t n = std::min(s.rows.size(), static_cast<size_t>(std::max<int32_t>(0, slots)));
  int32_t badgeW = 0;
  for (size_t i = 0; i < n; ++i) badgeW = std::max(badgeW, badgeWidth(s.rows[i], m));
  for (size_t i = 0; i < n; ++i) {
    row(scr, s.rows[i], m, p, kPadX, top + static_cast<int32_t>(i) * (rowH + kGap), kRight - kPadX, rowH, badgeW,
        i == 0);
  }
}

// --- Bildschirme -----------------------------------------------------------

void drawBoard(lv_obj_t *scr, const View &v, const Palette &p) {
  if (v.sections.size() >= 2) {  // geteilt: je Haltestelle Kopf + 2 Karten
    const int32_t half = (kHeight - 2 * kPadY - kGap) / 2;
    for (size_t i = 0; i < v.sections.size() && i < 2; ++i) {
      const SectionView &s = v.sections[i];
      const int32_t top = kPadY + static_cast<int32_t>(i) * (half + kGap);
      View head = v;
      head.header = StopNameStyle::Normal;  // bei zwei Haltestellen immer mit Namen
      if (i > 0) {
        head.clock.clear();
        head.date.clear();
      }
      header(scr, head, p, s.title, top, kHeadSplit, i == 0);
      cards(scr, s, kRows4, p, top + kHeadSplit + 6, top + half, static_cast<int32_t>(abf::kSplitRows));
    }
    return;
  }

  const int32_t headH = headerHeight(v);
  header(scr, v, p, v.title, kPadY, headH, true);
  const RowMetrics &m = v.rows <= 3 ? kRows3 : kRows4;
  int32_t bottom = kHeight - kPadY;
  if (!v.banner.empty()) {
    // Veraltete Daten: ruhiger Hinweis in Koralle statt gelber Leiste
    const int32_t y = bottom - kBannerH;
    box(scr, kPadX, y, kRight - kPadX, kBannerH, p.card, 12);
    lv_obj_t *b = label(scr, v.banner, &ui_font_small, p.coral);
    singleLine(b, &ui_font_small, kRight - 3 * kPadX);
    lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_CENTER, 0);
    placeBaseline(b, &ui_font_small, 2 * kPadX, y + (kBannerH + capHeight(&ui_font_small)) / 2);
    bottom = y - kGap;
  }
  const int32_t slots = v.banner.empty() ? v.rows : v.rows - 1;
  if (!v.sections.empty()) cards(scr, v.sections.front(), m, p, kPadY + headH + kGap, bottom, slots);
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

// Symbol links, Überschrift und Text linksbündig daneben, vertikal zentriert zwischen top und bottom
void messageBlock(lv_obj_t *scr, const Palette &p, Icon icon, const std::string &heading, const std::string &text,
                  int32_t top, int32_t bottom) {
  const std::string glyph = iconGlyph(icon);
  constexpr int32_t kIconCol = 150;  // Spalte für das Symbol
  const int32_t textX = glyph.empty() ? 3 * kPadX : kPadX + kIconCol;
  const int32_t textW = kRight - kPadX - textX;
  const int32_t headH = heading.empty() ? 0 : textHeight(heading, &ui_font_title, textW);
  const int32_t bodyH = text.empty() ? 0 : textHeight(text, &ui_font_small, textW);
  const int32_t total = headH + bodyH + (headH && bodyH ? 10 : 0);
  int32_t y = top + (bottom - top - total) / 2;

  if (!glyph.empty()) {
    const uint32_t color = icon == Icon::Warning ? p.coral : p.muted;
    const int32_t w = textWidth(glyph, &ui_font_icon);
    const int32_t ih = capHeight(&ui_font_icon);
    const int32_t ix = kPadX + (kIconCol - w) / 2;
    const int32_t iy = top + (bottom - top - ih) / 2;
    placeBaseline(label(scr, glyph, &ui_font_icon, color), &ui_font_icon, ix, iy + ih);
    if (icon == Icon::NoWifi) slash(scr, sSlashBig, ix + 4, iy + 2, ih - 4, 7, p.coral);
  }
  if (headH) {
    lv_obj_t *l = label(scr, heading, &ui_font_title, p.text);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, textW);
    lv_obj_set_pos(l, textX, y);
    y += headH + (bodyH ? 10 : 0);
  }
  if (bodyH) {
    lv_obj_t *l = label(scr, text, &ui_font_small, p.muted);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, textW);
    lv_obj_set_pos(l, textX, y);
  }
}

void drawMessage(lv_obj_t *scr, const View &v, const Palette &p) {
  constexpr int32_t kAddressH = 40;
  const int32_t headH = headerHeight(v);
  header(scr, v, p, v.title, kPadY, headH, true);
  const int32_t bottom = v.address.empty() ? kHeight - kPadY : kHeight - kAddressH;
  messageBlock(scr, p, v.icon, v.heading, v.text, kPadY + headH, bottom);
  if (!v.address.empty()) {
    // Adresse der Web-UI, damit man die Tafel auch ohne funktionierendes .local findet
    lv_obj_t *a = label(scr, v.address, &ui_font_small, p.muted);
    singleLine(a, &ui_font_small, kRight - kPadX);
    lv_obj_set_style_text_align(a, LV_TEXT_ALIGN_CENTER, 0);
    placeBaseline(a, &ui_font_small, kPadX, kHeight - (kAddressH - capHeight(&ui_font_small)) / 2);
  }
}

// Update: Symbol, Überschrift, Fortschrittsbalken und Text mittig
void drawUpdate(lv_obj_t *scr, const View &v, const Palette &p) {
  constexpr int32_t kTextW = kWidth - 4 * kPadX;
  constexpr int32_t kBarH = 18;
  std::string text = v.text;
  if (v.progress >= 0 && v.progress < 100) text = std::to_string(v.progress) + " % · " + text;
  const int32_t iconH = capHeight(&ui_font_icon);
  const int32_t headH = v.heading.empty() ? 0 : textHeight(v.heading, &ui_font_title, kTextW);
  const int32_t bodyH = text.empty() ? 0 : textHeight(text, &ui_font_small, kTextW);
  const int32_t barH = v.progress >= 0 ? kBarH + 36 : 0;
  int32_t y = (kHeight - (iconH + 22 + headH + barH + bodyH)) / 2;

  const int32_t w = textWidth(UI_ICON_DOWNLOAD, &ui_font_icon);
  placeBaseline(label(scr, UI_ICON_DOWNLOAD, &ui_font_icon, abf::brand::kTeal), &ui_font_icon, (kWidth - w) / 2,
                y + iconH);
  y += iconH + 22;
  if (headH) {
    lv_obj_t *l = label(scr, v.heading, &ui_font_title, p.text);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, kTextW);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, 2 * kPadX, y);
    y += headH;
  }
  if (v.progress >= 0) {
    y += 18;
    const int32_t barW = kWidth - 120;
    box(scr, 60, y, barW, kBarH, p.border, kBarH / 2);
    if (v.progress > 0) {
      box(scr, 60, y, std::max<int32_t>(kBarH, barW * v.progress / 100), kBarH, abf::brand::kTeal, kBarH / 2);
    }
    y += kBarH + 18;
  }
  if (bodyH) {
    lv_obj_t *l = label(scr, text, &ui_font_small, p.muted);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, kTextW);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, 2 * kPadX, y);
  }
}

// QR links, nummerierte Schritte rechts
void drawSetup(lv_obj_t *scr, const View &v, const Palette &p) {
  constexpr int32_t kBox = 232;
  constexpr int32_t kBoxX = 28;
  constexpr int32_t kBoxY = 22;
  lv_obj_t *qrBox = box(scr, kBoxX, kBoxY, kBox, kBox, 0xFFFFFF, 18);
  lv_obj_t *qr = lv_qrcode_create(qrBox);
  lv_qrcode_set_size(qr, kBox - 28);
  lv_qrcode_set_dark_color(qr, c(abf::brand::kNavy));
  lv_qrcode_set_light_color(qr, lv_color_white());
  lv_qrcode_update(qr, v.qr.data(), static_cast<uint32_t>(v.qr.size()));
  lv_obj_center(qr);
  if (!v.qrCaption.empty()) {
    lv_obj_t *cap = label(scr, v.qrCaption, &ui_font_small, p.muted);
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
    lv_obj_t *num = box(scr, kTextX, y + (lineH - kNum) / 2, kNum, kNum, abf::brand::kTeal, LV_RADIUS_CIRCLE);
    lv_obj_t *nl = label(num, std::to_string(n++), &ui_font_bold, abf::brand::kNavy);
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

  const Palette &p = v.theme == DisplayTheme::Day ? kDay : kNight;
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
