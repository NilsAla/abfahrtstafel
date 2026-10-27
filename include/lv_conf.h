// LVGL-Konfiguration (LVGL 9.3) für Firmware und Simulator.
// Nur Abweichungen von den Defaults; alles andere setzt lv_conf_internal.h.
// Eingebunden über -DLV_CONF_INCLUDE_SIMPLE und -I include (platformio.ini).
#ifndef LV_CONF_H
#define LV_CONF_H

// Panel ist RGB565
#define LV_COLOR_DEPTH 16

// Speicher über malloc der C-Bibliothek: auf dem ESP32 landen große Blöcke im PSRAM,
// statt eines festen LVGL-Pools im knappen internen RAM
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

// LVGL läuft ausschließlich in der Display-Task
#define LV_USE_OS LV_OS_NONE

// Inhalt ändert sich höchstens sekündlich
#define LV_DEF_REFR_PERIOD 50

// Eigene Theme-Farben je Objekt; das Default-Theme bliebe sonst bei Labels/Bars sichtbar
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 0

// Fallback-Schrift; die Oberfläche nutzt nur die eigenen Fonts aus src/ui/fonts
#define LV_FONT_MONTSERRAT_14 1

// QR-Codes für WLAN-Einrichtung und Web-UI
#define LV_USE_QRCODE 1

// Nicht benötigt
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LED 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_MSGBOX 0
#define LV_USE_ROLLER 0
#define LV_USE_SCALE 0
#define LV_USE_SLIDER 0
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 0
#define LV_USE_TEXTAREA 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0
#define LV_USE_DROPDOWN 0
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_ARCLABEL 0
#define LV_USE_LOTTIE 0

#endif  // LV_CONF_H
