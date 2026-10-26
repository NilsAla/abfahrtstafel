# Abfahrtstafel – Architekturentscheidungen

Stand: 2026-10-26 (M1)

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

## Konfiguration in NVS
- Namespace `cfg`, je Feld ein Key (≤ 15 Zeichen), Key `schema` = Schema-Version (`kConfigSchema` in `lib/config`)
- Fehlende oder ungültige Werte → Default (wird geloggt), nie Absturz
- Schema-Regeln, damit Rollbacks die Konfiguration nicht zerstören:
  - Neue Felder = neue Keys mit Default, kein Schema-Bump
  - Bump nur bei geänderter Bedeutung bestehender Keys; Migrationsschritt in `kSteps` ergänzen und testen
  - Bestehende Keys nie umdeuten, sondern neue anlegen; ältere Firmware liest ein neueres Schema (Outcome `Newer`)
    und setzt die Version nie zurück
- WLAN-Zugangsdaten verwaltet der WLAN-Stack selbst (ebenfalls NVS); Factory Reset löscht die ganze NVS-Partition

## WLAN-Ablauf (`lib/wifisup`)
| Zustand | Bedingung | Web-UI | Einrichtungs-WLAN |
|---|---|---|---|
| provisioning | keine Zugangsdaten | aus | offen, ohne Zeitlimit |
| connecting | Zugangsdaten, keine Verbindung | an | aus; Reconnect alle 30 s |
| fallback | 3 min erfolglos | aus | offen für 5 min (länger, solange ein Client verbunden ist) |
| online | verbunden | an | aus |

Portal (WiFiManager, synchroner WebServer) und ESPAsyncWebServer teilen Port 80 und laufen nie gleichzeitig;
beide Libraries stehen in getrennten Übersetzungseinheiten (`src/net/portal.cpp`, `src/web/web_server.cpp`).
Im Portal nur der Menüpunkt WLAN – das WiFiManager-eigene OTA-Update ist bewusst deaktiviert.

## Web-API (M1)
| Methode | Pfad | Inhalt |
|---|---|---|
| GET | `/api/status` | Version, Uptime, Reset-Grund, Partition, WLAN (Zustand, SSID, RSSI, IP), Zeit, Heap, PSRAM, `last_fetch` |
| GET | `/api/config` | Einstellungen als JSON |
| POST | `/api/config` | Teil-Update; 400 mit `errors{feld: Klartext}`, alles-oder-nichts; `restart_required` bei neuem Hostnamen |
| POST | `/api/restart` | Neustart nach 1 s |
| GET | `/api/log?since=N` | Logzeilen mit Sequenznummer > N als Text; Header `X-Log-Last`, `X-Log-Gap` |

Keine Authentifizierung: Gerät steht im Heimnetz. Bei Bedarf später ein optionales Passwort für schreibende Endpunkte.

## Firmware-Update (M2)
1. `GET …/releases/latest/download/manifest.json` (Redirects auf github.com/objects.githubusercontent.com)
2. Manifest prüfen (`lib/ota`): SemVer, SHA-256 als Hex, Größe ≤ OTA-Partition, URL exakt versioniertes `firmware.bin`
   dieses Repos. Angeboten wird nur eine echt neuere Version
3. Installation nur per Knopf: Download in eigener Task (12 KB Stack) direkt in die inaktive Partition,
   SHA-256 über mbedTLS beim Streamen, `esp_ota_end` prüft das Image, erst dann `esp_ota_set_boot_partition`
4. Neustart → Bootloader setzt das Image auf PENDING_VERIFY → Selbsttest: WLAN verbunden + Web-UI läuft →
   `esp_ota_mark_app_valid_cancel_rollback`. Kein Erfolg in 5 min oder Absturz/Stromausfall vorher → Rollback
5. Zielversion steht in NVS `ota/pendingTo`; startet danach eine andere Version, wird „zurückgerollt“ gemeldet

| Methode | Pfad | Inhalt |
|---|---|---|
| GET | `/api/update` | `current`, `state`, `busy`, `percent`, `error`, `offer{version,notes,size}`, `last_update{result,version}`, `selftest_running` |
| POST | `/api/update/check` | Prüfung starten (202, 409 wenn beschäftigt) |
| POST | `/api/update/install` | Angebot installieren (202, 409) |

Für Rollback-Tests baut `release.yml` mit dem Schalter `selftest_fail` eine Firmware mit `-DFW_SELFTEST_FAIL`.
Solche Releases nur veröffentlichen, solange keine Geräte im Einsatz sind, und sofort ein reguläres nachschieben.

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
