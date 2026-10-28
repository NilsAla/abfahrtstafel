# Roadmap

Stand: 2026-10-28 (M0.5 Phase B)

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
- [x] Pins, Timings und ST7701-Init-Sequenz aus Waveshare-Demo-Code und Schaltplan übernehmen, CS-Konflikt (0 vs. 42) auflösen
  (in Phase B erledigt, Demo-Paket lag nicht im Repo)
- [x] HAL in `src/board/`: ST7701-Init per Bit-Bang, esp_lcd-RGB-Panel, Backlight (PWM, invertiert), Taster
- [x] Environment `bringup`: nur Testbild direkt in den Framebuffer (ohne LVGL) + Serial
- [x] [B] **Bring-up zuerst:** Testbild korrekt, keine Farbvertauschung, Backlight dimmbar – erst danach voller Stack, Schicht für Schicht
- Done (B): Testbild stimmt, Orientierung bekannt ✅ (2026-10-28, Board 1 = Rev1.1), Firmware zeigt UI ruhig, Release `v0.5.0`

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
- [x] [A] WiFiManager-Portal auf Deutsch (`include/wm_strings_de.h` via `WM_STRINGS_FILE`), Hinweis zur Bedienung im Portal
- [ ] [B] Portal-Flow, mDNS, Taster am Gerät (siehe „Offen am Board“)
- Done (A): baut, Config-/Migrationstests grün, Release `v0.1.0`
- Done (B): Gerät nach Factory Reset ohne Serial einrichtbar

## M2 – Update per Knopfdruck
- [x] [A] Update-Seite: aktuelle Version, „Nach Update suchen“, Release-Notes, „Update installieren“, Fortschritt,
  Ergebnis des letzten Updates (erfolgreich / zurückgerollt)
- [x] [A] Manifest → Semver → HTTPS-Download über `url` (Redirects manuell, Header-Puffer 4 KB) → SHA-256 streamend prüfen →
  erst dann Boot-Partition setzen → Neustart. URL muss exakt `…/releases/download/v<version>/firmware.bin` dieses Repos sein
- [x] [A] Zustand „Update läuft“ abfragbar über `ota::busy()` (Darstellung in M4)
- [x] [A] Selbsttest nach Boot (WLAN + Web-UI), dann als gültig markieren. Bootloader-Rollback ist in den Arduino-Libs aktiv
  (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`); `verifyRollbackLater()` übernimmt die Entscheidung, Timeout 5 min
- [x] [A] Build-Flag `FW_SELFTEST_FAIL` für absichtlich kaputten Test-Build (Rollback nach 30 s), in `release.yml` als Schalter
- [x] [A] Optional: täglicher Check, nur Hinweis (Einstellung „Täglich nach Updates suchen“)
- [ ] [B] Update per Knopf, Rollback, Bildverhalten während Flash-Write (Bounce-Buffer / statischer Screen)
- Done (A): Manifest-/Semver-/Statemachine-Tests grün, Releases `v0.2.0` und `v0.2.1` für den Update-Test
- Done (B): Update v0.2.0 → v0.2.1 per Knopf; kaputter Build rollt automatisch zurück ✅ (2026-10-26, DevKit)

## M3 – Transitous-Client
- [x] [A] Provider-Interface (`src/provider/`) + Transitous-Implementierung, Fallback-Provider vorgesehen
- [x] [A] Parser als reine Funktion, Unit-Tests auf echten Antwort-Fixtures (`test/fixtures/transitous/`)
- [x] [A] Einstellungen: Haltestellensuche (geocode), bis zu 2 Haltestellen, Filter „alle“ (nach Verkehrsmittel) oder
  „nur bestimmte Richtungen“ (Linie + GTFS-directionId, nach Ziel gruppiert), Fußweg-Offset
- [x] [A] Polling 60 s mit Backoff (30 s … 10 min), Fehlerzustände (kein WLAN, keine Zeit, API down, keine Abfahrten),
  letzte Daten mit Datenalter; Ausfälle zur Planzeit einordnen, Dubletten aus mehreren Feeds entfernen
- [x] [A] User-Agent, Attribution in der Web-UI, `/api/departures`, `/api/stops?q=`, `/api/directions?stop=`
- [ ] [A] Zweites Fixture (anderer Tag/Uhrzeit, Bushaltestelle, Lübeck): `directionId` über die Zeit stabil?
- [ ] [B] 24-h-Dauerlauf ohne Heap-Leck
- Done (A): Parser-/Filtertests grün, Release `v0.3.0`
- Done (B): 24 h stabil

## M4 – Display
- [x] [A] UI-Schicht in LVGL 9.3, board-unabhängig (`src/ui/`); Logik in `lib/dispmodel/` (nativ getestet);
  Headless-Rendering (`env:sim`, `scripts/render_screens.py`) als PNG 820×320 nach `docs/screens/`
- [x] [A] Eigene Fonts aus Barlow Semi Condensed (OFL) + Font-Awesome-Symbole, `scripts/gen_fonts.py`, Größen mit mm-Kommentar
- [x] [A] 3 und 4 Zeilen per Einstellung (Web-UI): Linien-Badge in Linienfarbe, Ziel mit „…“, Minuten bzw. Uhrzeit ab 60 min,
  Verspätung farbig, Ausfälle durchgestrichen mit „fällt aus“, Gleis/Steig
- [x] [A] Zustände: Erststart mit WLAN-QR, QR für `http://abfahrt.local`, WLAN-Fallback mit QR, Fehler im Klartext,
  Update läuft/Neustart, keine Abfahrten, Laden, Uhrzeit fehlt, Nachtabschaltung, 2 Haltestellen (untereinander oder im Wechsel)
- [x] [A] Statuszeile (WLAN, Update verfügbar, Seite 1/2, Uhrzeit), Hinweisleiste mit Datenstand, Helligkeit und Nachtfenster als Logik
- [x] [A] Farbstile Dunkel (Nachtblau) und Nacht (Bernstein), Umschalter in der Web-UI; im Nachtfenster immer Nacht-Stil
- [x] [A] Feedback Endnutzerin: Datum in der Kopfzeile; Haltestellenname Groß/Normal/Aus (Web-UI, NVS `stopName`,
  bei 2 Haltestellen immer sichtbar); Verspätung ab +2 min: Minuten rot, rechts Planzeit durchgestrichen + neue Zeit
  statt „+4“ (wurde als Aufschlag gelesen), Gleis entfällt in dieser Zeile
- [ ] [B] Software-Rotation, Framebuffer im PSRAM, Ruckeln, Leseabstand 1–2 m → endgültige Zeilenzahl/Schriftgröße
- Done (A): Screenshots aller Zustände im Repo (`docs/screens/`), Release `v0.4.0` ✅ (2026-10-27, App 2,1 MB)
- Done (B): Layout am echten Display abgenommen

## M5 – Härtung und Übergabe
- [x] [A] Heap-Monitoring (`lib/heapwatch`: Warnung, Neustart nach 10 min kritisch, `heap.state` in `/api/status`),
  Coredump (ELF) über Web-UI abrufbar (Reiter „Protokoll“, `/api/coredump`), `firmware.elf` als Release-Asset zum Auswerten
- [x] [A] Web-Flash-Seite (ESP Web Tools 10.4.0, GitHub Pages) als Recovery-Pfad, nutzt `firmware-factory.bin`:
  `flash/index.html`, `.github/workflows/pages.yml` (nach jedem Release). Einmalig: Settings → Pages → Source „GitHub Actions“
- [x] [A] Kurzanleitung für die Endnutzerin (1 Seite, ohne Fachbegriffe): `docs/ANLEITUNG.md`
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
- [x] **CS-Pin klären (M0.5):** CS = GPIO0 (R38 0 Ω), „IO42“ ist ein Tabellenfehler im Schaltplan V1. GPIO0 nach der Init
  wieder Eingang → BOOT funktioniert nach `displayBegin()` (am Board geprüft, Log `BOOT gedrückt/losgelassen`).
  Details `docs/HARDWARE.md`.
- [x] **Bring-up (M0.5):** Befund 2026-10-28, Board 1 (Rev1.1, Flash XMC 0x20), vorher `erase_flash`. `pio run -e bringup -t upload`:
  `displayBegin ok in 434 ms`. Testbild: Farben R/G/B/Weiß und Verläufe korrekt, Rahmen an allen vier Kanten, kein
  Spiegeln. Panel-Oberkante (y = 0) = Seite gegenüber USB-C, x = 0 links. Backlight-Rampe 0–100 % sichtbar.
  Nebenbefund: grüne Lade-LED (LED1, ETA6098) flackert ohne Akku dauerhaft → im Gehäuse abdecken.
- [x] **Display-HAL im Firmware-Stack (M0.5):** Log `LVGL bereit, 820x320`, Einrichtungs- und Abfahrtsbildschirm stimmen mit
  `docs/screens/` überein (quer, USB-C links). Befund Bildruhe: Mit WLAN wackelte das Bild seitlich. Ursache 1: LVGL-Vollbild-
  puffer im PSRAM, gedrehter Flush 250 ms mit spaltenweisen PSRAM-Lesezugriffen → LVGL rendert jetzt in 820×20-Streifen
  im internen RAM (`src/display/display_task.cpp`, 32 KB). Ursache 2: Bounce-Buffer 10 Zeilen zu knapp, weil WLAN-/LWIP-
  Puffer im PSRAM liegen (`CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP`) → 41 Zeilen (2 × 26 KB). Danach ruhig. Heap frei nach
  Display-Start ≈ 118 KB. Versuch PCLK 14 MHz machte es deutlich schlimmer (zusammen mit dem alten Vollbild-Flush) → 18 MHz.
  Nach Update auf v0.5.0: Bild dauerhaft nach rechts versetzt (rechter Rand links sichtbar), vermutlich nach TLS-Abruf
  → `board::displayResync()` (`esp_lcd_rgb_panel_restart`) alle 500 ms aus der Display-Task; 3 Update-Prüfungen
  hintereinander danach ohne Versatz. Außerdem meldete jeder Software-Neustart (auch nach OTA) Reset-Grund „Watchdog“:
  laufender RGB-DMA blockierte `esp_restart()` → Shutdown-Handler stoppt das Panel, jetzt „Neustart durch Software“.
  Beides in `v0.5.1`. Offen: Langzeit-Beobachtung und Bild während des OTA-Downloads (siehe „Update-Bild“).
- [ ] **Bildversatz nach Flash-Zugriffen (v0.6.1):** Befund 2026-10-28 an `v0.5.1`: Bild trotz Resync dauerhaft um genau
  einen Bounce-Buffer (41 px quer) zyklisch versetzt („Gleis 1“ → rechts „Glei“, links „s1“; Uhr „21:56“ → „6“ links).
  Ursache: Arduino-Core baut den S3 ohne `CONFIG_LCD_RGB_ISR_IRAM_SAFE`; bei abgeschaltetem Cache (NVS, WLAN-Daten, OTA)
  gehen EOF-Interrupts verloren, die CPU füllt die Bounce-Buffer einen Chunk hinter dem DMA. Die Treiber-Resynchronisierung
  (`lcd_rgb_panel_try_restart_transmission`, IDF 5.5) erkennt nur „CPU voraus“, nicht „CPU hinterher“; `RESTART_IN_VSYNC`
  ist im Core ohnehin an → `displayResync()` entfernt. Fix: `custom_sdkconfig` mit `LCD_RGB_ISR_IRAM_SAFE` und
  `SPIRAM_XIP_FROM_PSRAM` (Hybrid-Compile, `platformio.ini`).
  Test: Log zeigt `RGB-ISR IRAM-sicher, XIP aus PSRAM`. In der Web-UI 10× Einstellungen speichern, Update suchen,
  WLAN-Router kurz trennen → Bild bleibt deckungsgleich. Danach Update auf eine Folgeversion: Bild während Download/Flash
  höchstens kurz gestört, danach ohne Versatz. `/api/status`: Heap frei notieren (IRAM-Bedarf steigt leicht).
  Fehlerbild weiter vorhanden → Bounce-Buffer auf 20 Zeilen (Versatz halbiert sich?) zur Bestätigung der Diagnose.
- [ ] **Update-Seite prüft beim Öffnen neu:** Ergebnis der automatischen Prüfung blieb stehen (am Board `0.4.0` angeboten,
  obwohl `0.5.0` schon draußen war). Jetzt liefert `/api/update` `checked_s` (Alter der letzten Prüfung); ist sie älter als
  5 min oder fehlt sie, prüft der Reiter „Update“ beim Öffnen selbst. Test: Gerät > 5 min laufen lassen, Reiter öffnen →
  kurz „Suche nach Updates …“, dann aktuelles Ergebnis; erneut öffnen innerhalb 5 min → keine neue Prüfung (Log ohne
  zweite `Firmware aktuell`-Zeile). Code: `loadUpdate()` in `web/app.js`, `buildStatus()` in `src/ota/ota.cpp`.
- Befund 2026-10-26 (DevKitC-1 N16R8 statt Waveshare, v0.2.0): Flash/PSRAM ok (8192 KB PSRAM, 229 KB Heap frei),
  Portal-Einrichtung ok, mDNS ok, NTP ok, automatische Update-Prüfung nach 2 min ok. Gefunden und im Branch behoben:
  AP hieß `Abfahrtstafel-0000` (MAC vor STA-Start leer → eFuse-MAC) und Web-UI nach dem Portal nicht erreichbar
  (`AsyncTCP bind error: -8` → Wiederholung alle 2 s). Update 0.2.0 → 0.2.1 per Knopf erfolgreich, Rollback mit
  Test-Build 0.2.2 erfolgreich (zurück auf 0.2.1, Meldung in der Web-UI). Daraus: zurückgerollte Versionen werden
  nie wieder angeboten (NVS `ota/blocked`). Update 0.2.1 → 0.3.0: 1,44 MB in 28,5 s (~50 KB/s), Selbsttest nach 1,2 s
  bestanden. Kosmetik behoben: erste Logzeilen nach Software-Reset in UTC, „per NTP gestellt“ ohne NTP,
  NVS-Fehlerzeile für fehlenden Key.
- [ ] **Erststart/Portal (M1):** Nach Factory Reset bzw. leerem NVS erscheint das WLAN `Abfahrtstafel-XXXX` (offen).
  Handy verbinden → Captive Portal öffnet sich (iOS + Android prüfen), nur Menüpunkt WLAN, kein Update-Menü.
  Portal komplett deutsch: Knopf „WLAN einrichten“, Hinweis „So geht es …“, Felder „WLAN-Name“/„WLAN-Passwort“,
  Knopf „Verbinden“, danach Meldung „Gespeichert.“; falsches Passwort → „Nicht verbunden … Passwort falsch“.
  Code: `include/wm_strings_de.h`.
  Nach Eingabe der Zugangsdaten: Portal schließt, Log zeigt `Zustand: provisioning → online`, `Web-UI gestartet`,
  `mDNS: http://abfahrt.local`. Code: `src/net/portal.cpp`, `src/net/network.cpp`.
- [ ] **Neustart nach Einrichtung (M1):** Befund: Nach der Ersteinrichtung war ein Neustart nötig. Jetzt: sobald die
  Tafel nach dem Portal (Erststart oder neue Zugangsdaten im Fallback-Portal) online ist, Log `WLAN eingerichtet, Neustart
  in 4 s`, Display „WLAN verbunden! Die Tafel startet einmal neu …“, danach Neustart ohne Portal. Dann Bildschirm
  „Fast geschafft!“: QR öffnet `http://<IP>`, Schritt 2 nennt `abfahrt.local` und die IP, unter dem QR steht die IP.
  Router nur kurz weg (ohne neue Zugangsdaten) → kein Neustart. Code: `apply()` in `src/net/network.cpp`,
  `setupRestart`/`ip` in `lib/dispmodel`.
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
- [ ] **Update per Knopf (M2):** Gerät per USB mit `v0.2.0` (`firmware-factory.bin`) flashen, einrichten. Reiter Update →
  „Nach Update suchen“ → Angebot `0.2.1` mit Notes → „Update installieren“ → Fortschritt bis 100 %, Neustart.
  Danach: Version 0.2.1, grüner Hinweis „erfolgreich installiert“, Log `Selbsttest bestanden`. Dauer notieren.
  Fehlerbilder: `Verbindung fehlgeschlagen`/`HTTP …` im Log (TLS, Redirect), `esp_ota_begin`/`esp_ota_write` (Flash).
  Code: `src/ota/ota.cpp`.
- [ ] **Rollback (M2):** Release mit Schalter `selftest_fail` als neueste Version veröffentlichen (z. B. `v0.2.2`),
  Update darauf installieren. Erwartet: neue Version startet, Log `Test-Build: schlägt absichtlich fehl`, nach 30 s
  `Rollback`, Neustart in die Vorversion, Update-Seite zeigt „hat nicht geklappt, bisherige Version läuft weiter“.
  Danach sofort ein reguläres Release (`v0.2.3`) nachschieben, damit `latest` wieder gut ist.
- [ ] **Stromausfall während Selbsttest (M2):** Direkt nach dem Update-Neustart (vor `Selbsttest bestanden`) Strom ziehen →
  beim nächsten Start läuft die Vorversion (Bootloader setzt ungeprüfte Images auf ABORTED). Dokumentieren, ob gewollt.
- [ ] **Abfahrten live (M3):** Haltestelle in der Web-UI suchen, auswählen, speichern → Status zeigt nach < 1 min
  Abfahrten, die mit der DB-App übereinstimmen (Zeit, Verspätung, Gleis). Log `dep` ohne Warnungen.
  Fehlerbilder: `HTTP 4xx`, `Antwort zu groß`, `Abfahrten nicht lesbar`. Code: `src/provider/`.
- [ ] **Richtungsauswahl (M3):** „Nur bestimmte Richtungen“ → Gruppen erscheinen nach wenigen Sekunden, Auswahl
  speichern, Vorschau zeigt nur diese Richtung. „Weiter in die Zukunft suchen“ ergänzt Linien.
- [ ] **API weg / WLAN weg (M3):** Router aus → Vorschau zeigt weiter Abfahrten mit „Stand: vor N min (keine
  WLAN-Verbindung)“, vergangene verschwinden. Nach Wiederkehr aktualisiert sich alles ohne Neustart.
- [ ] **Speicher (M3):** `/api/status` → Heap nach 1 h Betrieb mit 2 Haltestellen stabil (min_free notieren),
  PSRAM wird genutzt. Task-Stack der Abruf-Task reicht (kein Stack-Overflow im Log).
- [ ] **Serial ohne Host (M1):** Am USB-Netzteil (ohne PC) läuft das Gerät flüssig, Web-UI reagiert sofort
  (Serial-Ausgabe darf ohne verbundenen Host nicht blockieren). Code: `src/log/app_log.cpp`.
- [ ] **Web-UI M4 am DevKit (geht ohne Panel):** Update auf `v0.4.0`. Status-Reiter: Umschalter Dunkel/Nacht speichert sofort,
  Vorschau wechselt die Farben, Wert bleibt nach Neustart. Einstellungen → Anzeige: 3/4 Zeilen und „Untereinander/Im Wechsel“
  ändern die Vorschau. Nachtfenster auf die aktuelle Uhrzeit legen → Hinweis „Nachtzeit …“, Vorschau im Nacht-Stil.
  `/api/status` → `"display": false`, Log `Kein Display erkannt`. Update dauert länger (2,1 MB statt 1,4 MB).
- [x] **Display-HAL (M4 → M0.5):** erledigt, siehe oben. `board::displayBegin()` liefert bis M0.5 `false`, die Display-Task startet dann nicht
  (Log `Kein Display erkannt`, `/api/status` → `"display": false`). Mit HAL: `true`, Log `LVGL bereit, 820x320`.
  Vertrag: `displayFlush()` bekommt Querformat-Koordinaten (820 × 320, RGB565 in CPU-Byte-Reihenfolge) und dreht selbst
  ins Hochformat des Panels. Code: `src/board/`, `src/display/display_task.cpp`.
- [x] **Ausrichtung (M4):** Festgelegt: USB-C links im Gehäuse → `kRotateClockwise = false` in `src/board/board.cpp`.
  Am Board geprüft (2026-10-28). Für USB-C rechts die Konstante auf `true`.
- [ ] **Speicher (M4):** `/api/status` → PSRAM frei nach Start mit Display. Seit M0.5: LVGL-Streifen 32 KB intern,
  Bounce-Buffer 52 KB intern, Panel-Framebuffer 512,5 KB PSRAM → > 7 MB PSRAM frei erwartet. Heap (intern) darf durch LVGL kaum sinken (Objekte landen per malloc teils intern – Minimum notieren).
- [ ] **Ruckeln/Neuzeichnen (M4):** Minutenwechsel, Wechselanzeige (alle 10 s) und Umschalten Dunkel/Nacht in der Web-UI
  erscheinen ohne sichtbares Flackern oder Zeilenversatz. Komplett-Neuaufbau je Änderung ist gewollt (`src/ui/ui.cpp`).
- [ ] **Lesbarkeit (M4):** Aus 1 m und 2 m: 4 vs. 3 Zeilen (Einstellungen → Anzeige) und Nacht-Stil bei 5–15 % Helligkeit.
  Ergebnis → Standard für `Config::rows` in `lib/config/src/app_config.h`.
- [ ] **QR-Codes (M4):** Erststart: WLAN-QR mit iOS- und Android-Kamera scannen → Handy verbindet sich mit
  `Abfahrtstafel-XXXX`. Danach QR `http://abfahrt.local` öffnet die Web-UI. Auch im Nacht-Stil scannbar.
- [ ] **Nachtabschaltung (M4):** Nachtfenster mit 0 % → Backlight wirklich aus (nicht nur schwarzes Bild).
  Während eines Updates in der Nacht geht das Display mit minimaler Helligkeit an.
- [ ] **Update-Bild (M4/M2):** Während des Downloads aktualisiert sich der Fortschritt; Bild bleibt während Flash-Schreibzugriffen
  stabil (sonst Bounce-Buffer im RGB-Treiber, siehe ARCHITEKTUR).
- [ ] **Kopfzeile und Verspätung (M4):** Einstellungen → „Haltestellenname“ Groß / Normal / Ausblenden → Display
  wechselt binnen 1 s, Web-Vorschau beim nächsten Abruf. Groß: Name aus 1–2 m lesbar, Abfahrtszeilen nicht zu gedrängt?
  Ausblenden: langes Datum links. Datum stimmt (auch nach Mitternacht). Verspätete Zeile (≥ +2 min): rote Minuten,
  Planzeit durchgestrichen über neuer Zeit, beides aus 1–2 m unterscheidbar. Code: `src/ui/ui.cpp` (`header`, `row`).
- [ ] **Absturzbericht (M5):** Build mit Coredump (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`, ELF). Testweise
  `*(volatile int *)nullptr = 0;` in `loop()` nach 60 s Laufzeit → Absturz, Neustart. Erwartet: Log
  `Absturzbericht vorhanden: Task "loopTask", PC 0x…`, `/api/status` → `"coredump": true`, im Reiter „Protokoll“ Block
  „Absturzbericht“ mit Grund (`LoadStoreError`/`StoreProhibited`) und Backtrace. Download liefert `coredump-<version>.elf`;
  `esp-coredump info_corefile -c coredump-….elf .pio/build/esp32s3/firmware.elf` zeigt `loop()` in `main.cpp`.
  „Bericht löschen“ → Block verschwindet, auch nach Neustart. Passt das Image nicht in 64 KB (viele Tasks), steht beim
  Absturz im Serial-Log ein Coredump-Fehler → dann `CONFIG_ESP_COREDUMP_MAX_TASKS_NUM` senken. Danach Test-Code entfernen.
  Code: `src/system/coredump.cpp`, Routen in `src/web/web_server.cpp`.
- [ ] **Web-Flash-Seite (M5):** https://nilsala.github.io/abfahrtstafel/ in Chrome/Edge öffnen, Board per USB, „Installieren“
  → Port „USB JTAG/serial debug unit“ wählen → Fortschritt bis 100 %, Board startet mit der neuesten Version und öffnet
  das Einrichtungs-WLAN (NVS leer, weil das Factory-Image den Bereich mit 0xFF überschreibt). Auch prüfen: Board im
  Download-Modus (BOOT halten + RESET). Code: `flash/index.html`, `.github/workflows/pages.yml`.
- [ ] **Kurzanleitung (M5):** `docs/ANLEITUNG.md` am fertigen Gerät gegenlesen (Texte, Knopf-Lage im Gehäuse), am besten
  von jemandem ohne Vorwissen durchspielen lassen.
- [ ] **Heap-Überwachung (M5):** Heartbeat alle 10 min zeigt `Heap … (min …, Block …), PSRAM … (min …)`. Im Normalbetrieb
  mit Display und 2 Haltestellen kein `Heap knapp` (Schwellen 40 KB frei / 24 KB Block, kritisch 20 / 10 KB). Falls doch:
  realistische Werte notieren und `HeapWatch::Limits` in `lib/heapwatch/src/heap_watch.h` anpassen. `/api/status` →
  `heap.state` = `ok`. Code: `src/system/heap_monitor.cpp`.
