# Abfahrtstafel – Architekturentscheidungen

Stand: 2026-10-26

## Zielbild
- Geschenk/Gerät für eine Freundin (Raum Lübeck), Testgerät bei Nils (Münster/Emsdetten)
- Komplett on-device, keine Server-Abhängigkeit, idiotensicher
- Formfaktor: kleines, schmales Schild (Haltestellen-Look), z. B. am gedruckten Mast mit Fuß

## Hardware (entschieden, 2 Stück zum Testen bestellt)
- Board: **Waveshare ESP32-S3-LCD-3.16** (BerryBase WS-31786, ca. 23 €)
  - ESP32-S3R8, 16 MB Flash, 8 MB PSRAM, WLAN/BT mit Platinenantenne
  - 3,16" IPS, 320×820, 550 cd/m², ST7701 über RGB565-Parallel-Interface
  - Kein Touch (Konfiguration ausschließlich über Web-UI)
  - Onboard RTC (PCF85063, ohne Pufferbatterie → Zeitquelle bleibt NTP), IMU, microSD – ungenutzt
  - USB-C für Strom, Flashen, Serial
  - BOOT- und RST-Taster vorhanden (siehe HARDWARE.md); Factory Reset über BOOT/GPIO0
- Stromversorgung: USB-Netzteil 5 V, keine Batterie (Dauerbetrieb ~1–1,5 W, LiPo = Laufzeit zu kurz, Sicherheitsrisiko)
  - Beim ersten Board C-zu-C testen; falls ohne PD kein Strom: USB-A-Netzteil + A→C-Kabel
- Gehäuse: 3D-Druck, Zugang zu USB und BOOT-Taster
- Kosten pro Tafel: ca. 35 € (Board, Netzteil, Kabel, Filament)

### Verworfen
- E-Ink (Ghosting/Flackern bei Minuten-Countdown), OLED (Einbrennen bei festem Layout)
- Android-Rahmen (Wartung), Raspberry Pi (SD-Karte, OS-Pflege)
- 4,3"-Boards 800×480 (Sunton 8048S043, Waveshare 4.3, CrowPanel Advance) – zu groß für den Look
- Bar-Displays mit AXS15231B/QSPI (Waveshare 3.49, LilyGO T-Display-S3-Long) – teurer, Treiber eigenwillig
- RP2350 / Pico 2 W – kein WLAN im Chip, wenig RAM für TLS, ESP-spezifischer Stack (OTA, CA-Bundle) fehlt

## Firmware
- PlatformIO (pioarduino), LVGL (`LV_COLOR_DEPTH 16`, Framebuffer im PSRAM, ~525 KB pro Buffer), WiFiManager (Captive Portal),
  ESPAsyncWebServer, ArduinoJson mit Filter
- Konfiguration in NVS
- TLS über esp_crt_bundle, NTP mit Europe/Berlin
- Statuszeile (WLAN/API/Datenalter), Watchdog, Factory Reset per Taster, Firmware-Update per Knopf in der Web-UI
- RGB-Panel-Eigenheit: Während Flash-Schreibzugriffen (OTA) kann das Bild verrutschen/flackern → Bounce-Buffer im esp_lcd-RGB-Treiber
  und/oder statischer Screen „Update läuft“

## Release-Kette
- Tag `vX.Y.Z` (oder manueller Start von `release.yml` mit Versionsfeld) → GitHub Actions baut mit `FW_VERSION`
- Assets: `firmware.bin` (OTA), `firmware-factory.bin` (Erstflash ab 0x0), `manifest.json`
- `manifest.json`: `version`, `sha256`, `size`, `url` (versionierter Download-Link der `firmware.bin`), `notes`
- Versionen mit Bindestrich (z. B. `0.2.0-rc.1`) werden als Pre-Release veröffentlicht und nicht von `latest` ausgeliefert

## Datenquelle: Transitous (MOTIS 2)
- Base: https://api.transitous.org
- Haltestellensuche: GET /api/v1/geocode?text=…&type=STOP&numResults=…
- Abfahrten: GET /api/v6/stoptimes?stopId=…&n=…
- Relevante Felder: place.scheduledDeparture / place.departure, place.track, realTime, cancelled/tripCancelled,
  displayName/routeShortName, headsign, mode
- Usage Policy: User-Agent mit App, Version und Kontakt; nicht-kommerziell; Quellcode unter Open-Source-Lizenz veröffentlichen;
  sichtbarer Link auf https://transitous.org/sources/ + OSM-Attribution
- Fallback-Provider (db.transport.rest) in der UI wählbar halten

## Plan B
- Falls Standalone-Betrieb nicht zuverlässig klappt: Boards als einfache Anzeige mit Home-Assistant-Anbindung (z. B. ESPHome) betreiben
