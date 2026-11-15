#!/usr/bin/env python3
"""Erzeugt die LVGL-Schriften für das Display (src/ui/fonts/*.c).

Nur nötig, wenn sich Schriftgrößen oder Zeichenumfang ändern. Die erzeugten Dateien liegen im Repo,
der normale Build braucht kein Node.

    npm install -g lv_font_conv@1.5.3
    python3 scripts/gen_fonts.py

Größen sind Pixel bei ~0,0825 mm/px. Barlow Semi Condensed hat eine Versalhöhe von 0,7 em,
also Versalhöhe in mm ≈ Größe × 0,7 × 0,0825.
"""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "assets" / "fonts"
OUT = ROOT / "src" / "ui" / "fonts"

MEDIUM = SRC / "BarlowSemiCondensed-Medium.ttf"
SEMIBOLD = SRC / "BarlowSemiCondensed-SemiBold.ttf"
BOLD = SRC / "BarlowSemiCondensed-Bold.ttf"
ICONS = SRC / "FontAwesome5.woff"

# ASCII, Latin-1 (Umlaute, ß, ·), typografische Anführungszeichen, Gedankenstrich, Auslassungspunkte
LATIN = "0x20-0x7E,0xA0-0xFF,0x2013,0x2018-0x201E,0x2026,0x2212"
TIME = "0123456789:+-jetz"

# Font Awesome 5: wifi, download, warnung, info, uhr, aktualisieren
ICONS_SMALL = "0xF1EB,0xF019"
ICONS_BIG = "0xF1EB,0xF019,0xF071,0xF05A,0xF017,0xF021"

# name: (Schrift, Größe px, Zeichen, Icons) – Kommentar: Versalhöhe
FONTS = {
    # wannbus-Design (docs/branding/BRANDING.md, Abschnitt 5) mit am Gerät lesbaren Größen: keine kleine Unterzeile,
    # Verspätung über die Farbe der Minuten. Versalhöhe in mm ≈ Größe × 0,7 × 0,0825.
    "ui_font_dest3": (SEMIBOLD, 56, LATIN, None),     # Ziel, 3 Zeilen: 3,2 mm
    "ui_font_dest4": (SEMIBOLD, 44, LATIN, None),     # Ziel, 4 Zeilen und geteilt: 2,5 mm
    "ui_font_min3": (BOLD, 64, TIME, None),           # Minuten, 3 Zeilen: 3,7 mm
    "ui_font_min4": (BOLD, 50, TIME, None),           # Minuten, 4 Zeilen: 2,9 mm
    "ui_font_badge": (BOLD, 40, LATIN, None),         # Linie, 3 Zeilen: 2,3 mm
    "ui_font_badge_s": (BOLD, 32, LATIN, None),       # Linie, 4 Zeilen und geteilt: 1,8 mm
    "ui_font_ring": (BOLD, 18, "H", None),            # „H“ im Haltestellen-Ring
    "ui_font_small": (MEDIUM, 28, LATIN, ICONS_SMALL),  # „min“, Gleis, Kopfzeile, Adresse: 1,6 mm
    "ui_font_body": (MEDIUM, 32, LATIN, None),        # Texte, „min“ bei 3 Zeilen: 1,8 mm
    "ui_font_bold": (BOLD, 30, LATIN, None),          # „fällt aus“, Nummern der Schritte: 1,7 mm
    "ui_font_clock": (BOLD, 36, TIME, None),          # Uhr: 2,1 mm
    "ui_font_title": (BOLD, 48, LATIN, None),         # Überschriften: 2,8 mm
    "ui_font_icon": (None, 64, None, ICONS_BIG),      # große Symbole
    "ui_font_brand": (MEDIUM, 76, "wanbus", None),    # Wortmarke der Bootanimation: 4,4 mm
}


def build(name, font, size, chars, icons):
    rel = lambda p: str(p.relative_to(ROOT))  # relative Pfade → reproduzierbarer Dateikopf
    cmd = ["lv_font_conv", "--bpp", "4", "--size", str(size), "--format", "lvgl", "--no-compress",
           "--lv-include", "lvgl.h", "--lv-font-name", name, "-o", rel(OUT / f"{name}.c")]
    if font is not None:
        cmd += ["--font", rel(font)]
        cmd += ["--symbols", chars] if not chars.startswith("0x") else ["-r", chars]
    if icons:
        cmd += ["--font", rel(ICONS), "-r", icons]
    subprocess.run(cmd, check=True, cwd=ROOT)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, (font, size, chars, icons) in FONTS.items():
        build(name, font, size, chars, icons)
        print(f"{name}: {(OUT / f'{name}.c').stat().st_size // 1024} KB Quelltext")


if __name__ == "__main__":
    main()
