# Transitous-Fixtures

Echte Antworten von `api.transitous.org`, abgerufen mit dem Projekt-User-Agent. Nicht von Hand bearbeiten.

| Datei | Abfrage | Abgerufen | Besonderheiten |
|---|---|---|---|
| `geocode_emsdetten_bahnhof.json` | `/api/v1/geocode?text=Emsdetten%20Bahnhof&type=STOP&numResults=5` | 2026-10-26 ~12:20 MEZ | nur Treffer 0 in DE, Rest Schweiz („…, Bahnhof“); Gemeinde in `areas[adminLevel=8]` |
| `stoptimes_emsdetten_bf_2026-10-26.json` | `/api/v6/stoptimes?stopId=de-DELFI_de:05566:13729_G&n=20` | 2026-10-26 ~12:20 MEZ | Bus + Bahn gemischt, Steige A/B und Gleis 1/2, Verspätungen bis +87 min, 5× `cancelled` am Halt (`tripCancelled` false), Halt aus zweitem Feed (`de-VBN_…`) ohne Gleis, `displayName` mit Zugnummer („RB65 (52322)“), ~45 KB für 20 Abfahrten |
