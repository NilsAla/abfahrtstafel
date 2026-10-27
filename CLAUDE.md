# Abfahrtstafel – Projektregeln für Claude

Standalone-Abfahrtsanzeige (Bus/Bahn) auf ESP32-S3. Läuft ohne Server, ohne Home Assistant.
Endnutzerin ist technisch unerfahren: Alles muss ohne Hilfe funktionieren, jede Fehlersituation
wird im Klartext auf dem Display angezeigt. Wartung erfolgt remote über Firmware-Updates.

Meilensteine und Arbeitsteilung: `docs/ROADMAP.md`. Arbeite immer genau einen Meilenstein bzw. Block ab.
Hardware-Details: `docs/HARDWARE.md`, Entscheidungen: `docs/ARCHITEKTUR.md`.

## Arbeitsteilung
- **Cloud-Session (Phase A):** kein Hardware-Zugriff. Board-spezifischen Code nur als UNGETESTET markierten Entwurf
  schreiben, board-unabhängige Logik in `lib/` mit Tests in `env:native` absichern. Jeden neuen Hardware-Prüfpunkt
  in „Offen am Board“ (`docs/ROADMAP.md`) eintragen: was testen, erwartetes Ergebnis, Code-Stelle.
- **Claude Code lokal (Phase B):** Board per USB angeschlossen. Arbeitet „Offen am Board“ ab, ein Punkt pro Session.
  Fixt nur, was am Board hängt; größere Umbauten als Eintrag zurück nach Phase A.

## Stack
- PlatformIO auf pioarduino (arduino-esp32 3.3.x, Plattform-Version in `platformio.ini` gepinnt), ESP32-S3, 16 MB Flash, 8 MB OPI-PSRAM
- Environments: `esp32s3` (Firmware), `native` (Unit-Tests), `sim` (Headless-Rendering der UI als PNG: `pio run -e sim && python3 scripts/render_screens.py`), `bringup` (Display-Testbild, ab M0.5)
- LVGL 9.3 für das Display. Board-Spezifisches nur in `src/board/` (HAL), Rest board-unabhängig.
  Was angezeigt wird, entscheidet `lib/dispmodel` (testbar, ohne LVGL); `src/ui` zeichnet nur.
  Nach UI-Änderungen Screens neu rendern und `docs/screens/` mitcommitten.
- ESPAsyncWebServer (ESP32Async-Fork), ArduinoJson (immer mit Filter-Dokument parsen), WiFiManager
- Konfiguration in NVS (`Preferences`) mit Schema-Version, NICHT in LittleFS → überlebt Updates
- Web-UI (HTML/CSS/JS, ohne Framework) wird beim Build gzip-komprimiert und in die Firmware
  eingebettet. Ein Update = genau eine `firmware.bin`, kein separates Filesystem-Image.
- Fonts per lv_font_conv inkl. Umlaute/ß; Schriftgrößen in mm denken (~0,0825 mm/px)

## Update-Mechanismus (kritisch – nie brechen)
- Updates kommen aus GitHub Releases dieses (öffentlichen) Repos `NilsAla/abfahrtstafel`
- Release enthält `firmware.bin`, `manifest.json` (`version`, `sha256`, `size`, `url`, `notes`) und `firmware-factory.bin` (Erstflash, Offset 0x0)
- Gerät lädt `https://github.com/NilsAla/abfahrtstafel/releases/latest/download/manifest.json`
  und danach die Firmware über das versionierte `url`-Feld (nicht über `latest`, sonst Race bei neuem Release)
- Installation nur nach Klick auf „Update installieren“ in der Web-UI; optional täglicher Check,
  der nur einen Hinweis anzeigt
- SHA-256 der Firmware vor dem Umschalten prüfen
- Zwei OTA-Partitionen; neue Firmware erst nach erfolgreichem Selbsttest als gültig markieren
  (WLAN verbunden + Web-UI läuft), sonst Rollback auf die vorherige Version
- TLS ausschließlich über das eingebaute CA-Bundle (`esp_crt_bundle`), keine gepinnten Zertifikate
- Version kommt aus dem Git-Tag (`vX.Y.Z`) und wird beim Build als `FW_VERSION` injiziert (`scripts/version.py`)
- `partitions.csv` ist ab dem ersten Flash eingefroren. Jede Änderung an NVS-Schema oder OTA-Code
  braucht einen Migrationspfad von der letzten veröffentlichten Version

## Datenquelle: Transitous (MOTIS 2)
- Base `https://api.transitous.org`
- Suche: `GET /api/v1/geocode?text=…&type=STOP&numResults=5`
- Abfahrten: `GET /api/v6/stoptimes?stopId=…&n=…`
- Felder: `place.scheduledDeparture`, `place.departure`, `place.track`, `realTime`,
  `cancelled`, `tripCancelled`, `displayName`/`routeShortName`, `headsign`, `mode`
- User-Agent Pflicht: `abfahrtstafel/<version> (+https://github.com/NilsAla/abfahrtstafel)`
- Polling nicht schneller als alle 30 s, Backoff bei Fehlern
- Provider hinter Interface `src/provider/` kapseln, damit er austauschbar bleibt
- Attribution (Link auf https://transitous.org/sources/ und OpenStreetMap) sichtbar in der Web-UI

## Robustheit
- Task-Watchdog, automatischer WLAN-Reconnect, Heap/PSRAM-Werte auf `/api/status`
- Logging über ein Makro, das Serial und den Ringpuffer für `/api/log` bedient
- Bei API-Fehlern: letzte Daten weiter anzeigen, mit Datenalter
- Zeit über NTP, Zeitzone `Europe/Berlin` inkl. Sommerzeit
- Factory Reset: BOOT-Taster 10 s halten → NVS löschen, WLAN-Setup neu

## Arbeitsweise
- Commits klein, Message auf Deutsch, imperativ
- `pio run -e esp32s3` und `pio test -e native` müssen grün sein; GitHub Actions ist der Referenz-Build
- Cloud-Sandbox: PlatformIO-Registry, Toolchain-Downloads und api.transitous.org sind dort nicht erreichbar.
  Verifikation über GitHub Actions (Status per `env -u GH_TOKEN gh api …`; Job-Logs sind nicht abrufbar, nur Step-Status),
  reine Logik lokal mit g++ + Unity testen. Transitous-Fixtures liefert der Nutzer per curl.
- Releases aus der Cloud-Session: Tag-Pushes sind dort gesperrt → `release.yml` per `workflow_dispatch`
  starten (`gh api -X POST …/actions/workflows/release.yml/dispatches -f ref=main -f 'inputs[version]=X.Y.Z' -f 'inputs[notes]=…'`).
  Lokal geht auch `git tag -a vX.Y.Z -m "Notes" && git push origin vX.Y.Z`.
- Keine neuen Libraries ohne Begründung im Commit
- Nutzer kennt sich mit Git kaum aus: Git-Schritte selbst erledigen und kurz erklären, was passiert ist
- Hardware-Tests nur in Phase B: `pio run -e esp32s3 -t upload`, Serial-Log in Datei
  (`pio device monitor | tee log.txt`) und nur die relevanten Zeilen lesen, nicht den Live-Output
