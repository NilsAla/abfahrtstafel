# Roadmap

Stand: 2026-10-26 (M1 Phase A)

Reihenfolge ist Absicht: Update-Mechanismus kommt vor allen Features. Sobald M2 auf echter Hardware steht,
kann alles Weitere remote ausgeliefert werden – auch nachdem das Gerät bei der Endnutzerin hängt.

Zielhardware: Waveshare ESP32-S3-LCD-3.16 (ESP32-S3R8, 16 MB Flash, 8 MB PSRAM,
320×820 ST7701 RGB565, kein Touch). Details in `ARCHITEKTUR.md` und `HARDWARE.md`.

## Arbeitsteilung
- **Phase A – Cloud (ohne Hardware):** Claude in der claude.ai-Cloud-Session. Alles, was sich ohne Board bauen
  und prüfen lässt: Repo, CI, Logik, Web-UI, Provider, Layout per Headless-Rendering, Unit-Tests (`env:native`).
- **Phase B – Claude Code lokal (mit Hardware):** Board per USB am Rechner. Bring-up, Display-HAL, alles Timing-,
  Flash- und Strom-Abhängige, End-to-End-Tests. Arbeitet die Liste „Offen am Board“ ab: kurze Sessions,
  ein Punkt pro Session, dazwischen `/clear`.
- Übergabe ausschließlich über dieses Repo. Jede Phase-A-Session endet mit Push und aktualisierter Liste „Offen am Board“.
- Größere Umbauten, die sich am Board zeigen, gehen zurück in Phase A. Phase B fixt nur, was am Board hängt.

Markierung: **[A]** = ohne Hardware machbar, **[B]** = braucht Hardware.
Jeder Meilenstein endet mit einem lauffähigen Build und getaggtem Release. „Done (B)“ erst mit Hardware.

## M0 – Repo-Skelett und CI
- [x] [A] PlatformIO auf pioarduino 55.03.312-1 (arduino-esp32 3.3.12, gepinnt), ESP32-S3, 16 MB Flash, `qio_opi`
- [x] [A] Environments: `esp32s3`, `native`; `sim` (M4) und `bringup` (M0.5) als Platzhalter
- [x] [A] Partitionstabelle 16 MB: `nvs`, `otadata`, `app0`/`app1` je 6 MB, `coredump` 64 KB, `storage` reserviert.
  Wird mit dem ersten Flash eingefroren – spätere Änderungen nur per USB.
- [x] [A] `FW_VERSION` aus Env/Git-Tag (`scripts/version.py`), lokal `0.0.0-dev`
- [x] [A] `build.yml` (jeder Push/PR, inkl. `pio test -e native`), `release.yml` (Tag `v*` oder manuell mit Versionsfeld):
  `firmware.bin`, `manifest.json` (version, sha256, size, url, notes), `firmware-factory.bin` (gemergt, Offset 0x0)
- [x] [A] Lizenz MIT, README, Projektdoku in `docs/`
- [ ] [B] Hardware-Check: Waveshare-Demo flashen, Revision (V1/Rev1.1) notieren, BOOT-Taster, Strom C-zu-C, Maße nachmessen
- Done (A): Release `v0.0.1` mit allen drei Assets, sha256 im Manifest stimmt ✅ (2026-10-26, App 309 KB)
- Done (B): `v0.0.1` per USB geflasht, bootet, Serial-Log sichtbar

## M0.5 – Board-HAL-Entwurf
- [ ] [A] Pins, Timings und ST7701-Init-Sequenz aus Waveshare-Demo-Code und Schaltplan übernehmen, CS-Konflikt (0 vs. 42) auflösen
- [ ] [A] HAL-Interface in `src/board/`: Display-Init/Flush, Backlight (PWM), Taster. Implementierung als UNGETESTET markiert
- [ ] [A] Environment `bringup`: nur Testbild direkt in den Framebuffer (ohne LVGL) + Serial
- [ ] [B] **Bring-up zuerst:** Testbild korrekt, keine Farbvertauschung, Backlight dimmbar – erst danach voller Stack, Schicht für Schicht
- Done (B): Testbild stimmt, Orientierung bekannt

## M1 – WLAN-Setup und Web-UI
- [x] [A] Captive Portal beim Erststart (WiFiManager, nicht blockierend), AP `Abfahrtstafel-XXXX`; danach ESPAsyncWebServer
  (ESP32Async-Fork). Ablauf als Statemachine `lib/wifisup` inkl. Fallback-Portal nach 3 min ohne Verbindung
- [x] [A] mDNS `abfahrt.local` (Hostname einstellbar), NTP mit Europe/Berlin
- [x] [A] Web-UI ohne Framework, beim Build gzip-komprimiert eingebettet (`scripts/embed_web.py`, ETag);
  Seiten: Status, Einstellungen, Update (Platzhalter bis M2), Protokoll
- [x] [A] `/api/status` (Version, Uptime, RSSI, Heap/PSRAM, Reset-Grund, letzter API-Abruf), `/api/config` (GET/POST)
- [x] [A] `/api/log?since=N`: Ringpuffer (32 KB PSRAM) für Remote-Debugging ohne Serial
- [x] [A] Konfiguration in NVS mit Schema-Version und Migration (`lib/config`, rollback-sicher)
- [x] [A] Factory Reset per BOOT (GPIO0, 10 s), Task-Watchdog (30 s)
- [ ] [A] WiFiManager-Portal auf Deutsch (eigene Strings via `WM_STRINGS_FILE`), Hinweis zur Bedienung im Portal
- [ ] [B] Portal-Flow, mDNS, Taster am Gerät (siehe „Offen am Board“)
- Done (A): baut, Config-/Migrationstests grün, Release `v0.1.0`
- Done (B): Gerät nach Factory Reset ohne Serial einrichtbar

## M2 – Update per Knopfdruck
- [ ] [A] Update-Seite: aktuelle Version, „Nach Update suchen“, Release-Notes, „Update installieren“, Fortschritt
- [ ] [A] Manifest → Semver → HTTPS-Download über `url` (Redirects folgen) → SHA-256 streamend prüfen → erst dann Boot-Partition setzen → Neustart
- [ ] [A] Zustand „Update läuft“ als Event an die UI-Schicht (Darstellung in M4)
- [ ] [A] Selbsttest nach Boot (WLAN + Web-UI), dann als gültig markieren. Rollback-Support des Bootloaders prüfen,
  sonst Software-Rollback (Boot-Zähler)
- [ ] [A] Build-Flag `FW_SELFTEST_FAIL` für absichtlich kaputten Test-Build
- [ ] [A] Optional: täglicher Check, nur Hinweis
- [ ] [B] Update per Knopf, Rollback, Bildverhalten während Flash-Write (Bounce-Buffer / statischer Screen)
- Done (A): Manifest-/Semver-/Statemachine-Tests grün, Releases `v0.2.0` und `v0.2.1` für den Update-Test
- Done (B): Update v0.2.0 → v0.2.1 per Knopf; kaputter Build rollt automatisch zurück

## M3 – Transitous-Client
- [ ] [A] Provider-Interface (`src/provider/`) + Transitous-Implementierung, Fallback-Provider vorgesehen
- [ ] [A] Parser als reine Funktion, Unit-Tests auf echten Antwort-Fixtures (`test/fixtures/`)
- [ ] [A] Einstellungen: Haltestellensuche (geocode), bis zu 2 Haltestellen, Filter Linie/Richtung/Verkehrsmittel, Fußweg-Offset
- [ ] [A] Polling ≥ 30 s mit Backoff, Fehlerzustände (kein WLAN, keine Zeit, API down, keine Abfahrten, nur Soll-Daten),
  letzte Daten mit Datenalter
- [ ] [A] User-Agent, Attribution in der Web-UI, `/api/departures`, `/api/stops?q=`
- [ ] [B] 24-h-Dauerlauf ohne Heap-Leck
- Done (A): Parser-/Filtertests grün, Release `v0.3.0`
- Done (B): 24 h stabil

## M4 – Display
- [ ] [A] UI-Schicht in LVGL 9.3, board-unabhängig; Headless-Rendering (`env:sim`) als PNG 820×320 nach `docs/screens/`
- [ ] [A] Eigene Fonts per lv_font_conv inkl. Umlaute/ß, Größen als Konstanten in mm gedacht (~0,0825 mm/px)
- [ ] [A] 3 und 4 Zeilen per Konstante (Versalhöhe ≥ ~36 px): Linien-Badge, Ziel, Minuten/Uhrzeit, Verspätung farbig,
  Ausfälle durchgestrichen/grau, Gleis falls Platz
- [ ] [A] Zustände: Erststart mit WLAN-QR, QR für `http://abfahrt.local`, Fehler im Klartext, Update läuft, keine Abfahrten,
  Nachtmodus, 2 Haltestellen (Timer-Wechsel oder geteilte Ansicht)
- [ ] [A] Statuszeile (WLAN, Datenalter, Update verfügbar, Uhrzeit), Helligkeit und Nachtabschaltung als Logik
- [ ] [B] Software-Rotation, Framebuffer im PSRAM, Ruckeln, Leseabstand 1–2 m → endgültige Zeilenzahl/Schriftgröße
- Done (A): Screenshots aller Zustände im Repo, Release `v0.4.0`
- Done (B): Layout am echten Display abgenommen

## M5 – Härtung und Übergabe
- [ ] [A] Heap-Monitoring, Coredump über Web-UI abrufbar
- [ ] [A] Web-Flash-Seite (ESP Web Tools, GitHub Pages) als Recovery-Pfad, nutzt `firmware-factory.bin`
- [ ] [A] Kurzanleitung für die Endnutzerin (1 Seite, ohne Fachbegriffe)
- [ ] [B] 48-h-Dauertest, Übergabe-Test mit fremdem WLAN
- [ ] [B] Gehäuse (3D-Druck, Schild-Look) nach Messung: Ausschnitt ≈ 27 × 68 mm, 4× M2, Zugang USB-C und BOOT,
  Ein/Aus-Schalter verdeckt bzw. auf ON fixiert

## Offen am Board
Wird von Phase-A-Sessions gepflegt. Pro Eintrag: was testen, erwartetes Ergebnis, betroffene Code-Stelle.
Phase B hakt ab oder notiert den Befund.
- [ ] **Hardware-Check (M0):** Waveshare-Factory-Demo flashen bzw. vorinstallierte Demo starten. Revision (V1/Rev1.1) auf der
  Platine ablesen und in `docs/HARDWARE.md` eintragen. BOOT-Taster reagiert in der Demo. Strom: läuft am USB-C-Netzteil
  mit C-zu-C-Kabel? Maße mit Messschieber gegen `docs/HARDWARE.md` prüfen.
- [ ] **Erstflash v0.0.1 (M0):** `pio run -e esp32s3 -t upload` oder `firmware-factory.bin` aus dem Release ab 0x0 flashen.
  Erwartet im Serial-Log: `=== Abfahrtstafel 0.0.1 ===`, Reset-Grund, Partition `app0 @ 0x010000`, Flash 16 MB,
  PSRAM ≈ 8 MB gesamt, danach alle 10 s eine Heap-Zeile. Fehlerbilder: PSRAM 0 → `memory_type`/`qio_opi` prüfen;
  kein Serial → `ARDUINO_USB_CDC_ON_BOOT` in `platformio.ini`. Code: `src/main.cpp`.
- [ ] **CS-Pin klären (M0.5):** CS 0 vs. 42 und möglicher Konflikt mit BOOT/GPIO0. Code: `src/board/`.
- [ ] **Erststart/Portal (M1):** Nach Factory Reset bzw. leerem NVS erscheint das WLAN `Abfahrtstafel-XXXX` (offen).
  Handy verbinden → Captive Portal öffnet sich (iOS + Android prüfen), nur Menüpunkt WLAN, kein Update-Menü.
  Nach Eingabe der Zugangsdaten: Portal schließt, Log zeigt `Zustand: provisioning → online`, `Web-UI gestartet`,
  `mDNS: http://abfahrt.local`. Code: `src/net/portal.cpp`, `src/net/network.cpp`.
- [ ] **Port 80 nach Portal (M1):** Direkt nach der Einrichtung ist `http://abfahrt.local` erreichbar. Bekanntes
  WiFiManager-Problem: Port wird beim Schließen evtl. nicht freigegeben → dann bindet ESPAsyncWebServer nicht.
  Fehlerbild: Web-UI erst nach Neustart erreichbar. Code: `web::begin()` in `src/web/web_server.cpp`.
- [ ] **Web-UI und API (M1):** Alle vier Reiter laden, Einstellungen speichern (Wert bleibt nach Neustart), ungültiger
  Gerätename zeigt Feldfehler, Namensänderung → Hinweis + Neustart → neuer Name per mDNS erreichbar.
  Zweiter Seitenaufruf liefert 304 (ETag). `curl -s abfahrt.local/api/status | jq` plausibel (PSRAM ≈ 8 MB).
- [ ] **Router weg / Fallback (M1):** Router ausschalten → Log `online → connecting`, nach 3 min `fallback`, Portal-WLAN
  sichtbar; nach 5 min ohne Client zurück zu `connecting`. Router wieder an → `online` ohne Neustart.
  Code: `lib/wifisup`, Timing in `WifiSupervisor::Timing`.
- [ ] **Factory Reset (M1):** BOOT 10 s halten → Log `Werksreset`, Neustart, Portal-WLAN erscheint, Einstellungen auf
  Standard. Kurz drücken (< 10 s) löst nichts aus. Zur Laufzeit (nicht beim Einschalten) drücken, sonst Download-Modus.
  Code: `src/system/system.cpp`, Pin in `src/board/board.h`.
- [ ] **Watchdog (M1):** Testweise `delay(40000)` in `loop()` → Neustart, Reset-Grund „Task-Watchdog“ in `/api/status`.
  Danach wieder entfernen. Code: `sys::watchdogBegin()`.
- [ ] **Serial ohne Host (M1):** Am USB-Netzteil (ohne PC) läuft das Gerät flüssig, Web-UI reagiert sofort
  (Serial-Ausgabe darf ohne verbundenen Host nicht blockieren). Code: `src/log/app_log.cpp`.
