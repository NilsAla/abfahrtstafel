# wannbus – Marken- und Designspezifikation

Stand: 2026-11-15. Quelle der Wahrheit für Namen, Farben, Logo, Display-, Web-UI- und Animationsdesign.
Die Prompts in `docs/prompts/` verweisen hierauf. Bei Widerspruch gilt diese Datei.

Bearbeitbares Original der Mockups (Canvas, privat): https://claude.ai/artifact/4b7cd31c-7e80-4c0d-81f3-2b4d15acd017
Gerenderte Mockups: `docs/branding/mockups/` (2×, per `mockups/render_mockups.py` aus dem Canvas erzeugt).

## 1. Name

- Produktname: **wannbus** (immer kleingeschrieben, „wann“ in Text-/Navyfarbe, „bus“ in Koralle).
- Klartext-Name in Fließtext: „wannbus“. „Abfahrtstafel“ bleibt nur als technischer Repo-Name.
- Hinweis: Es gibt eine App namens „Wann“. Kein Markenrecht-Check gemacht; vor einer öffentlichen Verbreitung prüfen.

**Bewusst NICHT umbenennen** (OTA und Bestandsgeräte hängen daran):
- GitHub-Repo `NilsAla/abfahrtstafel`, Manifest-URL, Release-Asset-Namen (`firmware.bin`, `manifest.json`, `firmware-factory.bin`)
- User-Agent bleibt `abfahrtstafel/<version> (+https://github.com/NilsAla/abfahrtstafel)` (Transitous-Vorgabe, verweist aufs Repo)
- NVS-Namespace und -Keys, `partitions.csv`

Umbenennen (sichtbar für Nutzer): Display-Texte, Web-UI-Titel/Header, WLAN-Portal-Titel, AP-Name
(`wannbus-XXXX` statt `Abfahrtstafel-XXXX`, siehe `abf::apName`), Default-Hostname (`Config::hostname`, heute `abfahrt` →
`wannbus`, also `http://wannbus.local`), README, ANLEITUNG, Flash-Seite, Log-Banner.
Folgen: Der Hostname liegt in NVS. Geräte mit gespeichertem Default `abfahrt` müssen per Migration auf `wannbus` kommen,
ein vom Nutzer gesetzter eigener Name bleibt unangetastet. Die ANLEITUNG nennt den neuen AP-Namen und die neue Adresse.

## 2. Farben

### Marke (Logo, Akzente)
| Name | Hex | Verwendung |
|---|---|---|
| Navy | `#1B1F3B` | Logo-Kachel, Text im Tagmodus, Karten im Nachtmodus |
| Gelb | `#FFC72C` | **nur im Logo** (Bus-Körper), sonst nirgends |
| Koralle | `#FF4F5E` | „bus“ im Wortmarke, „Jetzt los“ |
| Türkis | `#1FC8B5` | **einzige Aktionsfarbe** (Buttons, Schalter, Fokus, „Bald“) |
| Creme | `#FFF6E5` | Text im Nachtmodus, Linien-Badge |

Regel: ruhig halten. Türkis ist die einzige Farbe für Interaktion. Gelb nie als Fläche in UI-Screens.
Koralle nur für Dringlichkeit (Jetzt los), Fehler und die Wortmarke.

### Themes (Tokens)
| Token | Nacht | Tag |
|---|---|---|
| `bg` (Web) | `#14162E` | `#EEF7FC` |
| `bg_display` | `#0E1024` | `#EAF4FA` |
| `bg_rest` (Betriebsruhe) | `#0A0B1A` | `#E1ECF4` |
| `card` | `#1B1F3B` | `#FFFFFF` |
| `card_hot` (Zeile 1 Display) | `#163A44` | `#DDF5F1` |
| `border` | `#2E3466` | `#CFE3EE` |
| `text` | `#FFF6E5` | `#1B1F3B` |
| `muted` | `#A9ABC9` | `#4A5075` |
| `teal_text` | `#3DDCC9` | `#0A7D71` |
| `coral_text` | `#FF6B78` | `#C81F33` |
| `teal_big` (Minuten) | `#1FC8B5` | `#0A8F80` |
| `coral_big` (Minuten) | `#FF4F5E` | `#E02D3C` |
| `badge_bg` / `badge_text` | `#FFF6E5` / `#1B1F3B` | `#1B1F3B` / `#FFF6E5` |

Kontrast: Text auf Fläche mindestens 4,5:1 (Tag: `teal_text` und `coral_text` sind dafür abgedunkelt). Große Zahlen ab 56 px dürfen heller sein.

### Zustandsfarben der Minuten (Gehzeit)
- **Jetzt los** (Restzeit ≤ Gehzeit): Koralle
- **Bald** (Restzeit ≤ Gehzeit + 5 min): Türkis
- **Entspannt**: neutral (Textfarbe)
- Verspätung steht als Unterzeile („+2 min verspätet“, Koralle), nicht als eigener Farbblock. Pünktlich: „pünktlich“ in Türkis.

**Gehzeit ist ein neues Feature** (in der Firmware heute nicht vorhanden): Einstellung in der Web-UI (Vorschlag Default 6 min, 0–30),
Minuten-Farbe nach den drei Zuständen. Der Schwellwert „Bald“ (+5 min) ist ein Vorschlag. Ohne Gehzeit (0) bleibt alles neutral.
Eigener Block, nicht Teil des Redesigns (siehe `docs/prompts/README.md`).

## 3. Schrift

- Display: **Barlow Semi Condensed** (OFL), Medium/SemiBold, Umlaute und ß per `scripts/gen_fonts.py`
- Web-UI: Barlow (OFL) bzw. Systemschrift als Fallback. Keine Webfonts von Google laden (kein Fremd-Request, Gerät läuft offline im LAN):
  Fonts werden eingebettet (subset, WOFF2) oder es bleibt bei der Systemschrift. Entscheidung im Prompt „Web-UI“.
- Wortmarke: Barlow Semi Condensed Medium, `letter-spacing: -0.5px`, „wann“ + „bus“ in Koralle

## 4. Logo

- App-Icon: Navy-Kachel (Radius 28/120), gelber Bus von vorn, Scheibe `#BFEFE9`, koralle Zierlinie, zwei creme Lichter.
  Quelle: `docs/branding/logo/wannbus-icon.svg`.
- Wortmarke daneben, Höhe ≈ 0,5 × Kachelhöhe. Mindestgröße Icon: 24 px.
- Favicon: Icon als SVG (`<link rel="icon" type="image/svg+xml">`), inline als Data-URI, damit kein extra Asset in der Firmware nötig ist.

## 5. Display (820 × 320, quer, kein Touch)

Gerenderte Referenzen: `mockups/Display_{Nacht,Tag}.png`, `Display_Fehler_{Nacht,Tag}.png`, `Display_Ruhe_{Nacht,Tag}.png`.

Hauptbildschirm (4 Zeilen, 3 Zeilen skaliert):
- Wurzel 820×320, Innenabstand 12 / 16 px, Abstand zwischen Elementen 8 px
- Kopf, Höhe 34: Haltestellen-Ring (28 px, Rahmen 2,5 px türkis, „H“ 15 px), Haltestellenname 23 px `muted`, Uhrzeit 30 px `text`
- Zeile, Höhe 78, Radius 18, Innenabstand 0 / 20, Abstand 18. Zeile 1 mit `card_hot`, übrige `card`
- Linien-Badge: Mindestbreite 60, Schrift 28 px, Radius 11, Farben `badge_bg`/`badge_text`
- Ziel 34 px (Ellipsis), Unterzeile 17 px („pünktlich“ / „+N min verspätet“ / „fällt aus“)
- Minuten 56 px + „min“ 22 px `muted`; ab 60 min Uhrzeit statt Minuten; Ausfall: Planzeit durchgestrichen
- Steig/Gleis: nur wenn vorhanden, rechts neben den Minuten klein (≈ 20 px, `muted`)

Fehlerbildschirm („Kein WLAN“): durchgestrichenes WLAN-Symbol (Strich Koralle), Titel 50 px, Hinweis 27 px in einfachen Worten,
Zeile „Letzte Abfahrten von 15:38“ 20 px Koralle, wenn noch Daten da sind. Ein Fehlerbild pro Ursache, Klartext, keine Codes.

Betriebsruhe: Mond-Symbol, „Nächste Abfahrt um 04:52“ mit Linien-Badge, Hintergrund `bg_rest`, Helligkeit reduziert.

### Marke auf dem Display (Leerlauf und Restfläche)
Logo-Icon und Wortmarke erscheinen überall dort, wo sonst Leere wäre. Zwei Fälle:

1. **Keine Abfahrten anzuzeigen** (Betriebsruhe, Linienfilter ergibt nichts, Daten ok): Marke zentriert (Icon 96 px, Wortmarke 64 px
   daneben oder darunter), darunter die Statuszeile („Nächste Abfahrt um 04:52“ mit Badge, falls bekannt). Der Kopf mit Haltestelle und Uhrzeit bleibt.
   Fehlerzustände (Kein WLAN, Update, Setup) behalten ihre eigenen Bildschirme, dort keine Marke.
2. **Anzeige nicht gefüllt** (weniger Abfahrten als Zeilen, z. B. 1 von 4): Die Zeilen bleiben oben ausgerichtet, nichts springt.
   Im freien Rest erscheint die Marke, zentriert im Freiraum:

| Freie Höhe (px) | Darstellung |
|---|---|
| ≥ 150 (0–1 Zeile belegt bei 4 Zeilen) | Icon 72 px + Wortmarke 56 px, nebeneinander, zentriert |
| 78 – 149 (2 Zeilen belegt) | Icon 40 px + Wortmarke 40 px, nebeneinander |
| < 78 | keine Marke |

Marke gedämpft: Wortmarke in `muted`, „bus“ in `coral_text`, Icon voll; sie darf nie stärker wirken als eine Abfahrtszeile.
Bei Zwei-Haltestellen-Layout gilt die Regel je Hälfte nur im Vollbild-Wechsel, nicht im geteilten Layout (zu wenig Platz).
Die Marke ist reine Darstellung; ob und wie groß sie erscheint, entscheidet `lib/dispmodel` (testbar), `src/ui` zeichnet nur.
Mockups: `mockups/Display_Marke_{0,1,2}Zeilen_{Nacht,Tag}.png` (erzeugt mit `mockups/render_marke.py`). Echte Renderings folgen im Display-Block (`docs/screens/`).

Alle Zustände (Setup mit QR, Update, Zwei-Haltestellen-Layout, Offline) werden analog umgestellt. Verhalten und Texte bleiben,
nur Farben, Maße und Badge-Stil ändern sich. Was angezeigt wird, entscheidet weiter `lib/dispmodel`.

## 6. Web-UI und WLAN-Portal (Handy)

Referenzen: `mockups/WebUI_{Nacht,Tag}.png`, `Main.png` (WLAN-Setup Nacht), `WifiSetup_Tag.png`.

- Mobile first, eine Spalte, Karten mit Radius 20, Rahmen 1 px `border`, Innenabstand 22 px
- Kopf: Icon + Wortmarke links, Status-Pille („Online“) rechts
- Karten: Vorschau („So sieht die Tafel gerade aus“), Haltestelle (Name, Linien als Wahl-Chips), Gehzeit (Slider + Legende der drei Zustände;
  erst mit dem Gehzeit-Block), **Anzeige** (Segment Auto / Tag / Nacht + Helligkeit), Update. Bestehende Einstellungen
  (Zeilenzahl, zwei Haltestellen, Haltestellenname, Nachtfenster, Update-Check) bleiben erhalten und werden nur neu gestaltet.
- Primärbutton: Türkis `#1FC8B5`, Text Navy, Radius 14, Höhe ≥ 56
- Slider: Türkis gefüllt, Rest `muted`-Spur; Chips ausgewählt: türkiser Rand
- Fuß: Attribution (Transitous, OpenStreetMap) bleibt Pflicht, dazu `wannbus.local · Version x.y.z`
- Tag/Nacht der Web-UI folgt `prefers-color-scheme`, überschreibbar über den Schalter (siehe Prompt „Tag/Nacht-Schalter“)
- WLAN-Portal (WiFiManager): gleiche Tokens per eingeschleustem `<style>`, Titel „wannbus“, Hinweistexte in einfachen Worten

## 7. Bootanimation

Referenz-GIFs (820 × 320 skaliert, 25 fps): `animation/wannbus_{nacht,tag}_boot_{once,loop}.gif`.
Generator: `animation/src/make_gif_{nacht,tag}.py` (Python + Pillow, Schrift DejaVu; im Gerät Barlow). Nur Vorlage für Timing und Geometrie.

Szene: Straße von der Seite, Haltestelle mit Dach und Bank, alte Dame (abstrakt), Fahnen-Haltestellenschild links neben der Haltestelle
(klein, kein Text, Display türkis angedeutet), Bus fährt von links ein, hält, Dame steigt ein, Bus fährt nach rechts weg.
Wortmarke „wannbus“ wird mit der Busfront aufgedeckt (Kante = Busfront − 12 px) und beim Wegfahren wieder verdeckt.

Zeitachse (Sekunden): Einfahrt 0,4–2,7 (ease-out cubic, 0 → 726 px) · Tür öffnet 2,9–3,3 · Dame steht auf 3,7 · Gehen 3,9–5,2 ·
Tür schließt 5,3–5,7 · Abfahrt 6,1–7,8 (ease-in cubic, bis 1080 px) · Ende 8,3.

Zwei Varianten für den Gerätebetrieb:
- **Boot-once** (Standard): Schnitt bei 4,0 s, danach 1,5 s Halten auf dem Logo. Spielt einmal beim Start, dann Wechsel auf die Abfahrten.
- **Loop**: nur für Web-Vorschau oder Demo, nicht beim Booten.
Nacht/Tag richtet sich nach dem eingestellten Modus (Abschnitt 8).

Technisch: keine GIFs auf dem Gerät dekodieren. Vorrendern zu RGB565-Frames oder (besser) in LVGL per Zeichenprimitiven/Animation
nachbauen; Budget siehe Prompt „Bootanimation“.

## 8. Tag/Nacht-Modus

- Drei Werte in der Konfiguration: `auto`, `day`, `night`. Heute gibt es `dark` (Nachtblau) und `night` (Bernstein); das alte
  `night` (Bernstein) entfällt, siehe Migration im Prompt „Rebranding“.
- `auto` schaltet nach der Uhrzeit, **nicht** nach Umgebungslicht (das Board hat keinen Lichtsensor). Vorschlag: fester Zeitplan
  (Tag 06:30–20:00, einstellbar), Zeitzone Europe/Berlin. Sonnenstand-basiert wäre schöner, braucht aber Koordinaten – später.
- Der Schalter in der Web-UI gilt für Display **und** Web-UI-Farben des Geräts; Display wechselt ohne Neustart
- Die bestehende Helligkeitsabsenkung im Nachtfenster (`night_enabled`, `night_brightness`) bleibt eigenständig; Betriebsruhe unverändert
