# Prompts für das wannbus-Redesign

Fertige Prompts für Claude-Sessions (Phase A, Cloud, ohne Board). Jede Session bekommt genau einen Prompt,
arbeitet nur diesen Block ab und endet mit Push, PR und aktualisierter Liste „Offen am Board“ in `docs/ROADMAP.md`.
Spezifikation: `docs/branding/BRANDING.md`. Mockups: `docs/branding/mockups/`.

Verwendung: Neue Session starten, Repo `NilsAla/abfahrtstafel` mit Push-Rechten anhängen, Prompt aus der Datei einfügen.
Nach jedem Block mergen (PR), Release per `release.yml`, am Board prüfen (Phase B), dann der nächste Block.

## Reihenfolge

| # | Datei | Inhalt | Braucht |
|---|---|---|---|
| 1 | `01-rebranding.md` | Name wannbus in allen sichtbaren Texten, AP-Name, Hostname-Migration, Design-Tokens als Code | – |
| 2 | `02-theme-modus.md` | Tag/Nacht-Modell (`auto`/`day`/`night`), Konfig-Migration, Schalter in der Web-UI | 1 |
| 3 | `03-display-redesign.md` | Display-UI im neuen Farb- und Layoutkonzept, Tag und Nacht, alle Zustände | 1, 2 |
| 4 | `04-webui-redesign.md` | Web-UI und WLAN-Portal im neuen Design, Tag und Nacht | 1, 2 |
| 5 | `05-bootanimation.md` | Boot-Animation (Bus, Haltestelle, Wortmarke), Tag und Nacht | 1, 2, 3 |

Gründe für die Reihenfolge: Umbenennung und Tokens zuerst, damit keine Doppelarbeit entsteht. Das Theme-Modell kommt vor den
beiden Redesigns, weil beide es brauchen und es eine Konfig-Migration enthält (kritisch, Rollback muss gehen).
Die Animation zuletzt, weil sie die Display-Farben und den Speicherbedarf des UI kennen muss.

## Nicht Teil dieser Prompts
- **Gehzeit** („Weg zur Haltestelle“, Minuten-Farbe Jetzt los/Bald/Entspannt): neues Feature mit eigener Logik in `lib/dispmodel`.
  Das Display-Redesign (Prompt 3) baut die Farbzustände schon ein, aber nur lauffähig mit Gehzeit 0 = neutral. Eigener Block, wenn gewünscht.
- **Zweite Datenquelle** (transport.rest): siehe Diskussion; falls gewünscht eigener Block hinter `src/provider/`.
