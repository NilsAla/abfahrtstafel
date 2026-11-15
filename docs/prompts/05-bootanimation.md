# Prompt 5 – Bootanimation

```
Repo: NilsAla/wannbus. Hänge es mit Push-Rechten an die Session. Lies CLAUDE.md, docs/ARCHITEKTUR.md, docs/HARDWARE.md,
docs/branding/BRANDING.md (Abschnitte 4, 5, 7), docs/branding/animation/ (GIFs und Generator-Skripte als Vorlage) und src/ui, src/display.
Arbeite nur diesen Block ab: Bootanimation. Voraussetzung: Prompts 1–3 sind gemergt.
Neuer Branch, kleine deutsche Commits, PR gegen main.

Ziel: Beim Start spielt die Szene aus den Referenz-GIFs einmal („boot-once“: Schnitt bei 4,0 s, danach 1,5 s Halten auf dem Logo),
in Tag oder Nacht passend zum aufgelösten Theme, danach Wechsel auf den normalen Bildschirm. Der Endzustand der Animation entspricht
dem Marken-Block aus BRANDING.md Abschnitt 5, damit der Übergang ohne Sprung wirkt.

Entscheidung zuerst (kurz im PR begründen, dann umsetzen): Variante A – in LVGL aus Primitiven und lv_anim nachbauen (bevorzugt:
kein Flash-Bedarf für Frames, sauber skalierbar, Tag/Nacht über Tokens). Variante B – vorgerenderte RGB565-Frames: nur wenn A
am Board nicht flüssig läuft; Flash- und Heap-Budget dann zahlenmäßig belegen. Keine GIF-Dekodierung auf dem Gerät.

Aufgaben:
1. Szene laut Referenz: Straße, Haltestelle mit Dach und Bank, abstrakte alte Dame, Fahnen-Haltestellenschild links neben der Haltestelle
   (klein, ohne Text, Display türkis angedeutet), Bus von links. Formen aus Rechtecken, Kreisen, abgerundeten Rechtecken; kein Detail,
   das auf 820×320 nicht lesbar ist. Farben aus lib/branding (Tag/Nacht, ggf. zusätzliche Szenenfarben als Tokens ergänzen).
2. Zeitachse als Konstanten in lib/ (board-unabhängig, testbar): Einfahrt 0,4–2,7 s ease-out cubic, Tür 2,9–3,3, Dame 3,7, Gehen 3,9–5,2,
   Tür zu 5,3–5,7, Abfahrt 6,1–7,8 ease-in cubic, Ende 8,3. Im Boot-once-Modus bei 4,0 s abbrechen und 1,5 s auf dem Logo halten.
   Funktionen `busX(t)`, `revealEdge(t)` (Kante = Busfront − 12 px bei Einfahrt) mit Unit-Tests (Monotonie, Randwerte).
3. Wortmarke „wannbus“ wird mit der Busfront aufgedeckt (Clip-Bereich), „bus“ in Koralle. Reiner Clip, keine Textneuberechnung pro Frame.
4. Tag/Nacht: Theme vor dem Start auflösen (Prompt 2); ist die Uhrzeit noch ungültig, Nacht. Animation darf den Start nicht blockieren:
   WLAN-Verbindung und Web-UI starten parallel (eigener Task/Timer), Watchdog füttern, Animation per Konfig abschaltbar
   (`boot_animation`, Default an, Schalter in der Web-UI Einstellungen) und überspringbar durch Abbruch, sobald die erste
   Abfahrtsseite bereit ist, aber frühestens nach der Einfahrt (2,7 s), damit sie nie abrupt wirkt.
5. Sim: `env:sim` rendert Schlüsselbilder (t = 0,0 / 1,5 / 2,7 / 3,3 / 4,0 / Endbild, Tag und Nacht) als PNG nach docs/screens/boot/,
   plus Kontaktbogen. Gegen docs/branding/animation vergleichen.
6. Fehlerfälle: Bei Factory-Reset-Start, Rollback-Start oder OTA-Neustart keine Animation (schnell zur Statusanzeige); Setup-Screen
   hat Vorrang, wenn kein WLAN konfiguriert ist.

Randbedingungen:
- Speicher: LVGL-Streifenpuffer 820×20 im internen RAM nicht vergrößern; freier Heap nach Display-Start (≈ 118 KB) nicht verschlechtern,
  Wert im PR nennen. Keine Vollbild-Puffer.
- Bildruhe: kein Wackeln mit WLAN (siehe ROADMAP „Display-HAL“). Keine Änderung an PCLK/Bounce-Buffer.
- Konfig-Key `boot_animation` neu mit Default, kein Umdeuten alter Keys, Rollback bleibt lesbar (CLAUDE.md).
- `pio test -e native`, `pio run -e esp32s3`, `pio run -e sim` grün.

Abschluss: Einträge in „Offen am Board“: Flüssigkeit (sichtbares Ruckeln? Logzeile mit Frame-Zeiten), Bildruhe mit WLAN während der Animation,
Startzeit bis erste Abfahrten (darf durch die Animation nicht länger werden als 1,5 s Halten), Tag/Nacht-Wahl beim Start, Rollback-Start ohne Animation.
Code-Stellen angeben.
```
