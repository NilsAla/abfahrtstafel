# Prompt 2 – Tag/Nacht-Modell und Schalter in der Web-UI

```
Repo: NilsAla/abfahrtstafel. Hänge es mit Push-Rechten an die Session. Lies CLAUDE.md, docs/ARCHITEKTUR.md,
docs/branding/BRANDING.md (Abschnitte 2 und 8) und lib/config/src/app_config.h. Arbeite nur diesen Block ab:
Tag/Nacht-Modus mit Schalter in der Web-UI. Voraussetzung: Prompt 1 ist gemergt. Neuer Branch, kleine deutsche Commits, PR gegen main.

Ausgangslage: `DisplayTheme { Dark, Night }` (Nachtblau / Bernstein), Key `theme`, Web-UI-Schnellschalter `theme-toggle`
im Status-Reiter plus Radios `theme` in den Einstellungen, POST /api/config {theme}. Das neue Konzept kennt Tag und Nacht,
beide im wannbus-Farbschema; Bernstein entfällt.

Neues Modell:
- Konfig `theme_mode` mit den Werten `auto`, `day`, `night`. Default für neue Geräte `auto`.
- `auto`: Tag von `day_start` bis `day_end` (Default 06:30–20:00, Ortszeit Europe/Berlin per NTP, in der Web-UI einstellbar).
  Ohne gültige Zeit (NTP noch nicht da): `night`, weil dunkel nachts am wenigsten stört.
- Die Entscheidung „welches Theme gerade aktiv ist“ ist reine Logik in `lib/dispmodel` bzw. `lib/config` (ohne Arduino):
  `resolveTheme(mode, minutesOfDay, timeValid) -> Day|Night`, getestet in `env:native` inkl. Mitternachtsfenster,
  Grenzen, ungültiger Zeit.
- Das Display wechselt zur Laufzeit ohne Neustart und ohne Flackern (Umschalten nur beim Wechsel des aufgelösten Themes,
  Prüfung höchstens einmal pro Minute und bei Konfigänderung). Die UI kennt dafür eine Funktion „Theme anwenden“; wenn
  Prompt 3 noch nicht gemergt ist, schaltet sie die vorhandenen Farben um, ohne Layout zu ändern.
- Das bestehende Nachtfenster für Helligkeit (`night_enabled`, `night_brightness`) bleibt eigenständig und unverändert.

Konfig-Migration (kritisch, siehe CLAUDE.md „Update-Mechanismus“):
- Neue Keys `theme_mode`, `day_start`, `day_end`; alter Key `theme` bleibt bestehen und wird weiter geschrieben
  (`dark` bei `day`/`night`), damit ein Rollback auf die Vorversion ein gültiges Theme liest. Keys nicht umdeuten.
- Schema-Bump (gemeinsam mit Prompt 1, falls dort schon auf 2 gegangen; sonst 1 → 2): Bestandsgeräte mit `theme=dark` oder
  `night` bekommen `theme_mode=night` (sie sollen optisch dunkel bleiben), nur frische Geräte starten mit `auto`.
- Unit-Tests: Migration aus Schema 1 für beide alten Werte, frisches Gerät, Rollback-Lesbarkeit (alter Key vorhanden),
  Validierung in `config_json.cpp` mit Klartext-Fehlermeldungen („Bitte Auto, Tag oder Nacht wählen.“).

Web-UI:
- Schalter „Anzeige“ als Segment `Auto | Tag | Nacht` (Referenz: docs/branding/mockups/WebUI_Tag.png, Karte „Anzeige“),
  im Status-Reiter statt der alten Radios, wirkt sofort (POST /api/config {theme_mode}), Hinweiszeile zeigt, was gerade aktiv ist
  („Auto: gerade Nacht, Tag ab 06:30“). Tastaturbedienbar, `role="radiogroup"`, Fokus sichtbar (Türkis).
- In den Einstellungen: `day_start`/`day_end` nur sichtbar bei `auto`.
- /api/status liefert `theme_mode` und `theme_active` (`day`/`night`).
- Die Farben der Web-UI selbst folgen `prefers-color-scheme` des Handys; ein Schalter „Web-UI gleich wie Display“ ist nicht nötig.
  (Entscheidung: Display-Theme und Handy-Theme sind getrennt; sonst wird die Seite beim Umschalten nachts hell.)
- Alte Radios und `renderThemeToggle` in web/app.js aufräumen, keine toten Pfade lassen. gzip-Einbettung bleibt.

Abschluss: Eintrag in „Offen am Board“: Schalter auf Tag/Nacht/Auto drücken, Display folgt innerhalb 2 s ohne Neustart;
`auto` über die Zeitgrenzen (Uhrzeit testweise per Einstellung knapp vor die Grenze legen); Update von Vorversion behält
dunkles Aussehen; Rollback startet fehlerfrei. Code-Stellen angeben.
```
