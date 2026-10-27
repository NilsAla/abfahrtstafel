# Display-Screens

Automatisch gerendert aus `env:sim` (LVGL, RGB565 wie am Gerät, 820 × 320 px ≈ 68 × 26 mm):

    pio run -e sim
    python3 scripts/render_screens.py

Beispieldaten: Emsdetten, Bahnhof, Dienstag 12:20 Uhr. Szenarien in `src/sim/sim_main.cpp`.

## Abfahrten
| Dunkel | Nacht |
|---|---|
| ![4 Zeilen](board-4-zeilen.png) | ![4 Zeilen Nacht](board-4-zeilen-nacht.png) |
| ![3 Zeilen](board-3-zeilen.png) | ![3 Zeilen Nacht](board-3-zeilen-nacht.png) |

Lange Namen werden gekürzt, Badges wachsen bis 170 px:

![Lange Namen](lange-namen.png)

## Zwei Haltestellen
| Untereinander | Im Wechsel (alle 10 s, mit Update-Hinweis) |
|---|---|
| ![Geteilt](zwei-haltestellen-geteilt.png) | ![Wechsel 1](zwei-haltestellen-wechsel-1.png) |
| ![Geteilt, eine veraltet](zwei-haltestellen-geteilt-veraltet.png) | ![Wechsel 2](zwei-haltestellen-wechsel-2.png) |

## Veraltete Daten
| WLAN weg | Fahrplandaten weg (Nacht) |
|---|---|
| ![Kein WLAN](veraltet-kein-wlan.png) | ![API Nacht](veraltet-api-nacht.png) |

## Einrichtung
| Erststart | Keine Haltestelle | WLAN nicht erreichbar |
|---|---|---|
| ![WLAN einrichten](einrichtung-wlan.png) | ![Haltestelle einrichten](einrichtung-haltestelle.png) | ![Fallback](wlan-nicht-erreichbar.png) |

## Meldungen
| | |
|---|---|
| ![Uhrzeit](meldung-uhrzeit.png) | ![Laden](meldung-laden.png) |
| ![API-Fehler](meldung-api-fehler.png) | ![Kein WLAN](meldung-kein-wlan.png) |
| ![Keine Abfahrten](meldung-keine-abfahrten.png) | ![Keine Abfahrten Nacht](meldung-keine-abfahrten-nacht.png) |

## Update
| Läuft | Neustart (Nacht) |
|---|---|
| ![Update](update-laeuft.png) | ![Neustart](update-neustart-nacht.png) |
