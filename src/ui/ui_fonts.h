// Display-Schriften (erzeugt mit scripts/gen_fonts.py, Barlow Semi Condensed + Font Awesome 5).
// Versalhöhen bei ~0,0825 mm/px siehe scripts/gen_fonts.py.
#pragma once

#include <lvgl.h>

extern "C" {
extern const lv_font_t ui_font_row4;    // Ziel, 4 Zeilen (52 px)
extern const lv_font_t ui_font_row3;    // Ziel, 3 Zeilen (64 px)
extern const lv_font_t ui_font_time4;   // Minuten, 4 Zeilen (56 px, nur Ziffern/:/jetzt)
extern const lv_font_t ui_font_time3;   // Minuten, 3 Zeilen (68 px)
extern const lv_font_t ui_font_badge4;  // Linie, 4 Zeilen (36 px)
extern const lv_font_t ui_font_badge3;  // Linie, 3 Zeilen (44 px)
extern const lv_font_t ui_font_small;   // „min“, Gleis, Kopfzeile (28 px, mit WLAN-/Download-Symbol)
extern const lv_font_t ui_font_body;    // Fließtext (32 px)
extern const lv_font_t ui_font_bold;    // Verspätung, Hinweisleiste (30 px)
extern const lv_font_t ui_font_clock;   // Uhrzeit (36 px, nur Ziffern)
extern const lv_font_t ui_font_title;   // Überschriften (48 px)
extern const lv_font_t ui_font_icon;    // große Symbole (64 px)
}

// Font-Awesome-Codepunkte als UTF-8
#define UI_ICON_WIFI "\xEF\x87\xAB"      // U+F1EB
#define UI_ICON_DOWNLOAD "\xEF\x80\x99"  // U+F019
#define UI_ICON_WARNING "\xEF\x81\xB1"   // U+F071
#define UI_ICON_INFO "\xEF\x81\x9A"      // U+F05A
#define UI_ICON_CLOCK "\xEF\x80\x97"     // U+F017
#define UI_ICON_SYNC "\xEF\x80\xA1"      // U+F021
