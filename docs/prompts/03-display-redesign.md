# Prompt 3 – Display-Redesign

```
Repo: NilsAla/abfahrtstafel. Hänge es mit Push-Rechten an die Session. Lies CLAUDE.md, docs/ARCHITEKTUR.md,
docs/branding/BRANDING.md (Abschnitte 2, 3, 5), docs/branding/mockups/Display_*.png, lib/dispmodel, src/ui.
Arbeite nur diesen Block ab: Display-UI im wannbus-Design. Voraussetzung: Prompts 1 und 2 sind gemergt.
Neuer Branch, kleine deutsche Commits, PR gegen main.

Ziel: Das Display (820×320) sieht aus wie die Mockups, in Tag und Nacht, in allen Zuständen. Verhalten und Texte bleiben,
sofern nicht unten anders steht. Was angezeigt wird, entscheidet lib/dispmodel (testbar, ohne LVGL); src/ui zeichnet nur.

Aufgaben:
1. Theme-Tokens aus lib/branding verwenden (Prompt 1), Theme Day/Night aus Prompt 2. Keine Farbliterale mehr in src/ui.
   Bernstein-Theme entfernen.
2. Hauptbildschirm laut BRANDING.md Abschnitt 5: Kopf, Zeilen (Radius 18, Zeile 1 `card_hot`), Badge-Stil, Maße, Schriftgrößen.
   3-Zeilen-Variante skaliert (größere Schrift), 2-Haltestellen-Layouts (geteilt/Wechsel) im selben Stil.
   Fonts neu erzeugen, falls Größen fehlen (scripts/gen_fonts.py, Umlaute/ß, Flash-Budget im PR nennen).
3. Unterzeile je Abfahrt: „pünktlich“ (Türkis) / „+N min verspätet“ (Koralle) / „fällt aus“ (Koralle, Planzeit durchgestrichen).
   Als Felder im Display-Modell, mit Tests (Grenzfälle: 0 min, ≥ 60 min, Ausfall, fehlender Echtzeitwert).
4. Minuten-Farbzustände (Jetzt los / Bald / Entspannt) als Funktion im Modell mit Parameter Gehzeit. Gehzeit ist noch nicht
   konfigurierbar: Parameter fest 0 → alles neutral. Kein neues Konfig-Feld in diesem Block.
5. Marke bei Leerlauf und Restfläche (BRANDING.md Abschnitt 5, „Marke auf dem Display“):
   a) Keine Abfahrten, Daten aktuell (Betriebsruhe, Filter leer): zentrierte Marke (Icon + Wortmarke), darunter Statuszeile
      „Nächste Abfahrt um HH:MM“ mit Badge, wenn bekannt.
   b) Weniger Abfahrten als Zeilen: Zeilen oben, im freien Rest Marke nach Höhentabelle (≥ 150 px groß, 78–149 klein, sonst keine).
   Das Modell liefert dafür ein Element `Brand{size: None|Small|Large, y, h}`; Berechnung rein arithmetisch, Tests für 0/1/2/3/4 Zeilen
   bei Zeilenzahl 3 und 4, im Wechsel-Layout, im geteilten Layout (keine Marke). Icon als LVGL-Zeichnung aus Primitiven
   (Rechtecke, Kreise) oder kleinem Bitmap – kein Rendern aus SVG zur Laufzeit; Wortmarke als Label in Barlow Semi Condensed,
   „bus“ als eigener Farb-Span.
   Die Marke ist gedämpft (siehe Spec) und darf keine Abfahrtszeile verdrängen.
6. Fehler- und Sonderbildschirme (Kein WLAN, Update, Setup mit QR, Betriebsruhe) im neuen Stil, Klartext bleibt. Betriebsruhe
   entspricht Fall 5a.
7. Screens neu rendern: `pio run -e sim && python3 scripts/render_screens.py`, `docs/screens/` mitcommitten, jeweils Tag und Nacht,
   inkl. 0, 1, 2, 3 und 4 Abfahrten. docs/mockups/README.md auf den neuen Stand verweisen (alte Bernstein-/Hell-Entwürfe kennzeichnen).
8. Vergleich: Rendering gegen docs/branding/mockups prüfen (Abstände, Farben); Abweichungen im PR nennen.

Randbedingungen:
- Speicher: LVGL rendert in 820×20-Streifen im internen RAM (siehe ROADMAP „Display-HAL“); Heap frei nach Display-Start ≈ 118 KB nicht
  verschlechtern, Wert im PR nennen. Keine Vollbild-Puffer, keine großen Bitmaps.
- Keine Änderung an Display-HAL, Timing, PCLK.
- `pio test -e native`, `pio run -e esp32s3`, `pio run -e sim` grün.

Abschluss: Einträge in „Offen am Board“: Lesbarkeit aus 1–2 m (Tag und Nacht, Umgebungslicht), Farbtreue Türkis/Koralle am echten Panel,
Marke bei 1 Abfahrt und bei Betriebsruhe, Bildruhe (kein Wackeln), Heap-Wert nach Start. Code-Stellen angeben.
```
