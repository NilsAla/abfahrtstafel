# Web-UI

HTML/CSS/JS ohne Framework. `scripts/embed_web.py` komprimiert beim Build jede Datei hier per gzip
und erzeugt `src/generated/web_assets.h` (nicht im Repo). Ein Update ist damit immer genau eine `firmware.bin`.

- `index.html` – eine Seite mit Reitern Status, Einstellungen, Update, Protokoll
- `app.js` – spricht nur mit `/api/*` (siehe `src/web/web_server.cpp`)
- `app.css` – hell/dunkel nach Systemeinstellung

Keine externen Ressourcen (CDN, Fonts): Die Seite muss auch ohne Internet funktionieren.
Attribution für Transitous und OpenStreetMap steht im Footer und muss sichtbar bleiben.
