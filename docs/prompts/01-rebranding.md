# Prompt 1 – Rebranding zu „wannbus“

```
Repo: NilsAla/abfahrtstafel. Hänge es mit Push-Rechten an die Session. Lies CLAUDE.md, docs/ARCHITEKTUR.md,
docs/branding/BRANDING.md (Abschnitte 1–4) und docs/ROADMAP.md. Arbeite nur diesen Block ab: Rebranding zu „wannbus“.
Neuer Branch, Commits klein und auf Deutsch im Imperativ, am Ende PR gegen main. Kein Push auf main.

Ziel: Das Produkt heißt wannbus. Alles, was Endnutzer sehen, trägt den neuen Namen. Repo-Name, OTA-Pfade und
gespeicherte Daten bleiben unangetastet (Liste in BRANDING.md Abschnitt 1, „Bewusst NICHT umbenennen“).

Aufgaben:
1. Bestandsaufnahme: alle Vorkommen von „Abfahrtstafel“/„abfahrt“ per grep, jedes einordnen (sichtbar / technisch / bleibt).
   Ergebnis als Tabelle in die PR-Beschreibung.
2. Sichtbare Texte umstellen: Log-Banner in src/main.cpp, WiFiManager-Titel (src/net/portal.cpp), Web-UI-Titel und -Header
   (web/index.html, web/app.js), flash/index.html, README.md, docs/ANLEITUNG.md, docs/screens-Texte in src/ui und lib/dispmodel.
   Tests in test/ anpassen.
3. AP-Name: `abf::apName(mac)` liefert `wannbus-XXXX` (gleiche Suffix-Logik). Test anpassen. Das WLAN-Portal erkennt der Nutzer
   am Namen; ANLEITUNG und Display-Setup-Screen (QR/Text) müssen den neuen Namen zeigen.
4. Hostname: Default in `Config::hostname` von `abfahrt` auf `wannbus`. Migration: Geräte, deren gespeicherter Hostname noch
   exakt dem alten Default `abfahrt` entspricht, bekommen `wannbus`; eigene Namen bleiben. Das ist eine Schema-Migration
   (kConfigSchema 1 → 2, Migrationspfad in app_config.cpp, Unit-Tests in test/test_config inkl. „Schema 1 mit Default-Hostname“,
   „Schema 1 mit eigenem Hostname“, „frisches Gerät“). Neue Keys statt Umdeuten, damit Rollback auf die Vorversion liest, was sie
   kennt (siehe Kommentar an kConfigSchema). Wenn diese Migration und die aus Prompt 2 im selben Release landen sollen,
   gemeinsam auf Schema 2 – vorher in der PR-Beschreibung klären.
5. Design-Tokens als Code: neue Datei `lib/branding/src/tokens.h` (board-unabhängig, ohne LVGL) mit den Farben aus BRANDING.md
   Abschnitt 2 als `constexpr uint32_t` je Theme (Nacht, Tag). Dazu `web/tokens.css` mit CSS-Variablen und ein Native-Test, der
   Hex-Werte gegen BRANDING.md-Tabelle (als Testdaten) prüft. Noch nicht verwenden; Prompts 3 und 4 nutzen sie.
6. Logo: `docs/branding/logo/wannbus-icon.svg` als Favicon (inline Data-URI im HTML) und in den Web-UI-Header, Wortmarke
   „wann“ + „bus“ (Koralle). Mehr Design macht Prompt 4.
7. Release-Notiz vorbereiten (Text für `inputs[notes]`): „Neuer Name wannbus“, Hinweis auf geänderte Adresse
   http://wannbus.local und neuen WLAN-Namen.

Randbedingungen:
- Update-Mechanismus nicht anfassen (URLs, Manifest, Asset-Namen, User-Agent bleiben).
- partitions.csv nicht ändern.
- `pio test -e native` und `pio run -e esp32s3` grün (GitHub Actions ist Referenz).

Abschluss: Eintrag in „Offen am Board“: (a) Gerät mit alter Firmware (Hostname `abfahrt`) per Web-UI-Update auf diese Version:
erwartet `http://wannbus.local` erreichbar; (b) Gerät mit eigenem Hostnamen behält ihn; (c) Werksreset → AP heißt `wannbus-XXXX`;
(d) Rollback auf die Vorversion startet fehlerfrei. Code-Stellen angeben.
```
