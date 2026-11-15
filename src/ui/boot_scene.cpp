// Bootanimation: Straße, Haltestelle, Schild, Bus und Dame wie in docs/branding/animation (Referenz-GIFs).
// Alles aus Rechtecken, Kreisen und Linien; pro Bild werden nur Positionen geändert, LVGL zeichnet die
// betroffenen Bereiche im Streifenpuffer neu. Die Wortmarke liegt in einem Container, dessen Breite der
// Busfront folgt (Clip statt Neuberechnung).
#include "boot_scene.h"

#include <tokens.h>

#include <math.h>

#include <algorithm>

#include "ui.h"
#include "ui_fonts.h"

namespace ui {

namespace {

using abf::boot::Frame;
using abf::boot::LadyMode;

// Szenenfarben (Referenz make_gif_{nacht,tag}.py); Markenfarben aus lib/branding
struct Scene {
  uint32_t sky, building, road, sidewalk, curb, post, word;
  bool sun;
};
constexpr Scene kSceneNight = {0x14162E, 0x1F2346, 0x0B0D1C, 0x343964, 0x525886, 0xC8CDE6, abf::brand::kCream, false};
constexpr Scene kSceneDay = {0xBAE4F8, 0x96C6E4, 0x606880, 0xDAD3C6, 0xF0EADE, 0x464C6E, abf::brand::kNavy, true};

constexpr uint32_t kYellowDark = 0xEBAF19;
constexpr uint32_t kTealDark = 0x149687;
constexpr uint32_t kViolet = 0x7A5CFA;
constexpr uint32_t kVioletDark = 0x5A42C8;
constexpr uint32_t kGlass = 0xBFEFE9;
constexpr uint32_t kWheel = 0x0E1024;
constexpr uint32_t kDoor = 0x1E2246;
constexpr uint32_t kSkin = 0xFFD6B4;
constexpr uint32_t kSignPole = 0x2E3244;
constexpr uint32_t kSignScreen = 0x1E787D;

constexpr int32_t kBusTop = 164;
constexpr int32_t kBusLen = static_cast<int32_t>(abf::boot::kBusLength);

lv_obj_t *rect(lv_obj_t *parent, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color, int32_t r = 0) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  lv_obj_set_pos(o, x0, y0);
  lv_obj_set_size(o, x1 - x0, y1 - y0);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, r, 0);
  return o;
}

lv_obj_t *circle(lv_obj_t *parent, int32_t cx, int32_t cy, int32_t r, uint32_t color) {
  return rect(parent, cx - r, cy - r, cx + r, cy + r, color, LV_RADIUS_CIRCLE);
}

lv_obj_t *group(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  return o;
}

struct Line {
  lv_obj_t *obj = nullptr;
  lv_point_precise_t pts[2];
};

void lineInit(lv_obj_t *parent, Line &l, uint32_t color) {
  l.obj = lv_line_create(parent);
  lv_obj_remove_style_all(l.obj);
  lv_obj_set_style_line_color(l.obj, lv_color_hex(color), 0);
  lv_obj_set_style_line_rounded(l.obj, true, 0);
  l.pts[0] = l.pts[1] = {0, 0};
  lv_line_set_points(l.obj, l.pts, 2);
}

void lineSet(Line &l, float x0, float y0, float x1, float y1, float width) {
  l.pts[0] = {static_cast<lv_value_precise_t>(x0), static_cast<lv_value_precise_t>(y0)};
  l.pts[1] = {static_cast<lv_value_precise_t>(x1), static_cast<lv_value_precise_t>(y1)};
  lv_obj_set_style_line_width(l.obj, std::max<int32_t>(1, static_cast<int32_t>(width + 0.5f)), 0);
  lv_line_set_points(l.obj, l.pts, 2);  // meldet den neuen Bereich zum Neuzeichnen an
}

void place(lv_obj_t *o, float x0, float y0, float x1, float y1, float r = -1) {
  lv_obj_set_pos(o, static_cast<int32_t>(x0), static_cast<int32_t>(y0));
  lv_obj_set_size(o, std::max<int32_t>(1, static_cast<int32_t>(x1 - x0 + 0.5f)),
                  std::max<int32_t>(1, static_cast<int32_t>(y1 - y0 + 0.5f)));
  if (r >= 0) lv_obj_set_style_radius(o, static_cast<int32_t>(r), 0);
}

struct State {
  bool active = false;
  lv_obj_t *bus = nullptr;
  lv_obj_t *door = nullptr;
  lv_obj_t *reveal = nullptr;  // Clip-Container der Wortmarke
  int32_t textRight = 0;
  // Dame
  Line legL, legR, arm;
  lv_obj_t *body = nullptr, *head = nullptr, *brim = nullptr, *hat = nullptr;
} s;

void buildBackground(lv_obj_t *scr, const Scene &sc) {
  rect(scr, 0, 0, kWidth, kHeight, sc.sky);
  if (sc.sun) {
    circle(scr, 72, 58, 30, 0xFFECAA);
    circle(scr, 72, 58, 22, abf::brand::kYellow);
    rect(scr, 690, 40, 770, 58, 0xFFFFFF, 9);
    rect(scr, 710, 28, 756, 48, 0xFFFFFF, 10);
  }
  rect(scr, 30, 118, 190, 172, sc.building, 10);
  rect(scr, 300, 104, 440, 172, sc.building, 10);
  rect(scr, 580, 126, 760, 172, sc.building, 10);
  rect(scr, 0, 172, kWidth, 256, sc.road);
  rect(scr, 0, 256, kWidth, kHeight, sc.sidewalk);
  rect(scr, 0, 256, kWidth, 262, sc.curb);
}

void buildBus(lv_obj_t *scr) {
  s.bus = group(scr, -kBusLen, kBusTop, kBusLen, 92);  // Räder ragen unter die Karosserie
  lv_obj_add_flag(s.bus, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  rect(s.bus, 0, 0, kBusLen, 76, abf::brand::kYellow, 18);
  rect(s.bus, 6, 16, kBusLen - 62, 44, kGlass, 8);
  rect(s.bus, kBusLen - 52, 16, kBusLen - 10, 50, kGlass, 8);
  s.door = rect(s.bus, kBusLen - 58, 16, kBusLen - 57, 72, kDoor, 3);
  lv_obj_add_flag(s.door, LV_OBJ_FLAG_HIDDEN);
  rect(s.bus, 2, 54, kBusLen - 2, 61, abf::brand::kCoral);
  for (int32_t wx : {kBusLen - 196, kBusLen - 48}) {
    circle(s.bus, wx, 76, 16, kWheel);
    circle(s.bus, wx, 76, 6, abf::brand::kCream);
  }
}

// Logo (76 px) + „wann“„bus“ zentriert; liegt in einem Container, dessen Breite die Busfront vorgibt
void buildWordmark(lv_obj_t *scr, const Scene &sc) {
  const lv_font_t *font = &ui_font_brand;
  lv_point_t wann, bus;
  lv_text_get_size(&wann, "wann", font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  lv_text_get_size(&bus, "bus", font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  constexpr int32_t kIcon = 76, kGap = 18, kTop = 30;
  const int32_t total = kIcon + kGap + wann.x + bus.x;
  const int32_t left = (kWidth - total) / 2;
  s.textRight = left + total + 2;

  s.reveal = group(scr, 0, 0, 0, kHeight);
  lv_obj_t *tile = rect(s.reveal, left, kTop, left + kIcon, kTop + kIcon, abf::brand::kNavy, 28 * kIcon / 120);
  lv_obj_set_style_border_color(tile, lv_color_hex(0x2A2F5A), 0);
  lv_obj_set_style_border_width(tile, 1, 0);
  auto u = [](int32_t v) { return v * kIcon / 120; };
  rect(s.reveal, left + u(22), kTop + u(20), left + u(98), kTop + u(98), abf::brand::kYellow, u(14));
  rect(s.reveal, left + u(30), kTop + u(30), left + u(90), kTop + u(66), kGlass, u(6));
  rect(s.reveal, left + u(22), kTop + u(76), left + u(98), kTop + u(82), abf::brand::kCoral);

  // Grundlinie so, dass die Schrift mittig zum Icon steht
  const int32_t base = kTop + kIcon / 2 + 3 + (font->line_height - font->base_line) / 2;
  const int32_t x = left + kIcon + kGap;
  lv_obj_t *a = lv_label_create(s.reveal);
  lv_obj_remove_style_all(a);
  lv_label_set_text(a, "wann");
  lv_obj_set_style_text_font(a, font, 0);
  lv_obj_set_style_text_color(a, lv_color_hex(sc.word), 0);
  lv_obj_set_pos(a, x, base - (font->line_height - font->base_line));
  lv_obj_t *b = lv_label_create(s.reveal);
  lv_obj_remove_style_all(b);
  lv_label_set_text(b, "bus");
  lv_obj_set_style_text_font(b, font, 0);
  lv_obj_set_style_text_color(b, lv_color_hex(abf::brand::kCoral), 0);
  lv_obj_set_pos(b, x + wann.x, base - (font->line_height - font->base_line));
}

void buildLady(lv_obj_t *scr) {
  lineInit(scr, s.legL, kVioletDark);
  lineInit(scr, s.legR, kVioletDark);
  s.body = rect(scr, 0, 0, 1, 1, kViolet, 4);
  lineInit(scr, s.arm, kViolet);
  s.head = rect(scr, 0, 0, 1, 1, kSkin, LV_RADIUS_CIRCLE);
  s.brim = rect(scr, 0, 0, 1, 1, abf::brand::kCoral, 2);
  s.hat = rect(scr, 0, 0, 1, 1, abf::brand::kCoral, 5);
}

void buildSign(lv_obj_t *scr) {
  rect(scr, 500, 150, 512, 316, kSignPole, 2);
  rect(scr, 500, 150, 512, 196, kYellowDark, 2);
  rect(scr, 492, 310, 520, 316, kSignPole, 3);
  rect(scr, 422, 112, 516, 152, abf::brand::kYellow, 7);
  circle(scr, 444, 132, 13, kTealDark);
  circle(scr, 444, 132, 10, abf::brand::kYellow);
  rect(scr, 439, 125, 442, 139, kTealDark, 1);
  rect(scr, 446, 125, 449, 139, kTealDark, 1);
  rect(scr, 439, 131, 449, 134, kTealDark);
  rect(scr, 461, 118, 509, 146, kSignScreen, 6);
  rect(scr, 464, 121, 506, 143, abf::brand::kTeal, 4);
}

void updateLady(const abf::boot::Lady &l) {
  const bool visible = l.mode != LadyMode::Hidden;
  for (lv_obj_t *o : {s.legL.obj, s.legR.obj, s.body, s.arm.obj, s.head, s.brim, s.hat}) {
    if (visible) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
  if (!visible) return;
  const float k = 0.9f * l.scale;
  auto X = [&](float px) { return l.x + px * k; };
  auto Y = [&](float py) { return l.yHip + py * k; };
  float swL = 0, swR = 0;
  if (l.mode == LadyMode::Walk) {
    const float sn = sinf(l.legPhase);
    swL = std::max(0.0f, sn) * 5.0f;
    swR = std::max(0.0f, -sn) * 5.0f;
  }
  lineSet(s.legL, X(-6), Y(0), X(-6), Y(20 - swL), 6 * k);
  lineSet(s.legR, X(6), Y(0), X(6), Y(20 - swR), 6 * k);
  place(s.body, X(-14), Y(-36), X(14), Y(2), 4 * k);
  if (l.waving) {
    lv_obj_remove_flag(s.arm.obj, LV_OBJ_FLAG_HIDDEN);
    lineSet(s.arm, X(12), Y(-32), X(26), Y(-50 + l.wave), 5 * k);
  } else {
    lv_obj_add_flag(s.arm.obj, LV_OBJ_FLAG_HIDDEN);
  }
  place(s.head, X(-10), Y(-58), X(10), Y(-38), LV_RADIUS_CIRCLE);
  place(s.brim, X(-15), Y(-60), X(15), Y(-55), 2 * k);
  place(s.hat, X(-8), Y(-70), X(8), Y(-56), 5 * k);
}

}  // namespace

void bootBegin(abf::DisplayTheme theme) {
  const Scene &sc = theme == abf::DisplayTheme::Day ? kSceneDay : kSceneNight;
  lv_obj_t *scr = lv_screen_active();
  lv_obj_clean(scr);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  s = State{};
  s.active = true;
  // Reihenfolge = Tiefe: Hintergrund, Bus, Wortmarke, Haltestelle hinten, Dame, Haltestelle vorn, Schild
  buildBackground(scr, sc);
  buildBus(scr);
  buildWordmark(scr, sc);
  rect(scr, 548, 150, 556, 316, sc.post, 2);
  rect(scr, 732, 150, 740, 316, sc.post, 2);
  buildLady(scr);
  rect(scr, 588, 296, 700, 306, abf::brand::kCoral, 3);
  rect(scr, 530, 140, 758, 156, abf::brand::kTeal, 6);
  buildSign(scr);
  bootFrame(abf::boot::frameAt(0.0f));
}

void bootFrame(const Frame &f) {
  if (!s.active) return;
  lv_obj_set_x(s.bus, static_cast<int32_t>(f.busFront) - kBusLen);
  const float gap = 34.0f * f.door;
  if (gap > 1.0f) {
    lv_obj_remove_flag(s.door, LV_OBJ_FLAG_HIDDEN);
    place(s.door, kBusLen - 58 - gap / 2, 16, kBusLen - 58 + gap / 2, 72);
  } else {
    lv_obj_add_flag(s.door, LV_OBJ_FLAG_HIDDEN);
  }
  const int32_t edge = std::min(static_cast<int32_t>(f.reveal), s.textRight + 1);
  lv_obj_set_width(s.reveal, std::max<int32_t>(0, edge));
  updateLady(f.lady);
}

void bootEnd() {
  if (!s.active) return;
  s.active = false;
  lv_obj_clean(lv_screen_active());
  invalidate();
}

}  // namespace ui
