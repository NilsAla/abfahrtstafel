# Prompt 4 – Web-UI und WLAN-Portal im wannbus-Design

```
Repo: NilsAla/wannbus. Hänge es mit Push-Rechten an die Session. Lies CLAUDE.md, docs/branding/BRANDING.md (Abschnitte 2, 3, 4, 6),
docs/branding/mockups/WebUI_*.png, Main.png, WifiSetup_Tag.png sowie web/ und src/net/portal.cpp.
Arbeite nur diesen Block ab: Web-UI und WLAN-Portal im neuen Design. Voraussetzung: Prompts 1 und 2 sind gemergt.
Neuer Branch, kleine deutsche Commits, PR gegen main.

Ziel: Die Web-UI sieht aus wie die Mockups (Tag und Nacht, mobile first). Funktion bleibt, nichts geht verloren.

Aufgaben:
1. web/tokens.css (Prompt 1) als Grundlage; `prefers-color-scheme` wählt Tag/Nacht. Keine Farbliterale in app.css.
   Türkis ist die einzige Aktionsfarbe, Gelb nur im Logo.
2. Layout: Kopf mit Icon, Wortmarke und Status-Pille; Karten (Vorschau, Haltestelle, Anzeige, Update und die bestehenden Reiter-Inhalte
   Protokoll/Einstellungen) im Kartenstil. Bestehende Reiter dürfen bleiben, wenn sie in der Navigation sauber wirken; Ziel ist
   Wiedererkennbarkeit, kein Funktionsumbau.
3. Vorschau-Karte: zeigt die aktuelle Tafel (Daten aus /api/status bzw. vorhandener Quelle), nur Darstellung, inkl. Marke bei Leerlauf
   (siehe BRANDING.md Abschnitt 5, vereinfacht: ein Hinweistext mit Wortmarke reicht).
4. Bedienelemente: Primärbutton (Türkis, Text Navy, Radius 14, Höhe ≥ 56), Segment-Schalter, Slider, Chips für Linien. Alles per
   Tastatur bedienbar, Fokus sichtbar, Zielgröße ≥ 44 px, Kontrast ≥ 4,5:1 (Tag-Tokens sind dafür abgedunkelt).
5. Schrift: keine Fremd-Requests. Entweder Systemschrift-Stack oder Barlow als eingebettetes WOFF2-Subset; Größenänderung der
   Firmware im PR nennen (ein Update = genau eine firmware.bin, siehe CLAUDE.md). Bei mehr als +40 KB Systemschrift nehmen.
6. Attribution (Transitous, OpenStreetMap) bleibt sichtbar im Fuß, dazu `wannbus.local · Version x.y.z`.
7. WLAN-Portal (WiFiManager): eigenes CSS über `setCustomHeadElement`/Style-Einschub in src/net/portal.cpp, Titel „wannbus“, Logo oben,
   Texte in einfachen Worten, großer türkiser „Verbinden“-Button, Tag/Nacht per prefers-color-scheme. Größe des Einschubs klein halten
   (RAM/Flash), kein JavaScript-Framework.
8. Fehlermeldungen der Web-UI weiter im Klartext, ohne Fehlercodes.
9. Mockups-Vergleich: Headless-Screenshots (z. B. Playwright, nur lokal im Sandbox-Tooling, nicht als Projektabhängigkeit) von
   web/index.html mit Beispieldaten für Tag/Nacht im PR zeigen (nicht ins Repo einchecken); Abweichungen nennen.

Randbedingungen:
- Keine neuen Libraries ohne Begründung im Commit. Web-UI bleibt HTML/CSS/JS ohne Framework, wird beim Build gzip-komprimiert und eingebettet.
- API unverändert (nur Felder aus Prompt 2); alte Clients dürfen nicht brechen.
- `pio run -e esp32s3` und `pio test -e native` grün.

Abschluss: Einträge in „Offen am Board“: Web-UI auf echtem Handy (iOS Safari, Android Chrome) in Tag und Nacht, Portal im Captive-Portal-Fenster
(iOS/Android), Ladezeit der Seite über WLAN, freier Heap beim Seitenabruf. Code-Stellen angeben.
```
