# Display-Screens

Automatisch gerendert aus `env:sim` (LVGL, RGB565 wie am Gerät, 820 × 320 px ≈ 68 × 26 mm):

    pio run -e sim
    python3 scripts/render_screens.py

Beispieldaten: Emsdetten, Bahnhof, Dienstag 12:20 Uhr. Szenarien in `src/sim/sim_main.cpp`.

## Abfahrten
| Nacht (Standard) | Tag |
|---|---|
| ![4 Zeilen](board-4-zeilen.png) | ![4 Zeilen Tag](board-4-zeilen-tag.png) |
| ![3 Zeilen](board-3-zeilen.png) | ![3 Zeilen Tag](board-3-zeilen-tag.png) |

## Haltestellenname (Einstellung)
| Groß | Ausblenden (nur Datum und Uhr) |
|---|---|
| ![Groß](kopfzeile-gross.png) | ![Aus](kopfzeile-aus.png) |
| ![Groß, 3 Zeilen](kopfzeile-gross-3-zeilen.png) | ![Aus, Tag](kopfzeile-aus-tag.png) |

Verspätung ab +2 min: Minuten rot (enthalten die Verspätung), rechts Planzeit durchgestrichen und neue Zeit.

Lange Namen werden gekürzt, Badges wachsen bis 170 px:

![Lange Namen](lange-namen.png)

## Zwei Haltestellen
| Untereinander | Im Wechsel (alle 10 s, mit Update-Hinweis) |
|---|---|
| ![Geteilt](zwei-haltestellen-geteilt.png) | ![Wechsel 1](zwei-haltestellen-wechsel-1.png) |
| ![Geteilt, eine veraltet](zwei-haltestellen-geteilt-veraltet.png) | ![Wechsel 2](zwei-haltestellen-wechsel-2.png) |

## Veraltete Daten
| WLAN weg | Fahrplandaten weg (Tag) |
|---|---|
| ![Kein WLAN](veraltet-kein-wlan.png) | ![API Tag](veraltet-api-tag.png) |

## Einrichtung
| Erststart | Nach dem Portal (Neustart) | Keine Haltestelle (QR auf die IP) | WLAN nicht erreichbar |
|---|---|---|---|
| ![WLAN einrichten](einrichtung-wlan.png) | ![Neustart](einrichtung-neustart.png) | ![Haltestelle einrichten](einrichtung-haltestelle.png) | ![Fallback](wlan-nicht-erreichbar.png) |

## Meldungen
| | |
|---|---|
| ![Uhrzeit](meldung-uhrzeit.png) | ![Laden](meldung-laden.png) |
| ![API-Fehler](meldung-api-fehler.png) | ![Kein WLAN](meldung-kein-wlan.png) |
| ![Keine Abfahrten](meldung-keine-abfahrten.png) | ![Keine Abfahrten Tag](meldung-keine-abfahrten-tag.png) |

## Update
| Läuft | Neustart (Tag) |
|---|---|
| ![Update](update-laeuft.png) | ![Neustart](update-neustart-tag.png) |
