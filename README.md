# Abfahrtstafel

Kleine, eigenständige Abfahrtsanzeige für Bus und Bahn im Haltestellen-Look.
Läuft komplett auf dem Gerät – kein Server, kein Home Assistant, keine App. Eingerichtet wird
über eine Weboberfläche im Heimnetz, Updates gibt es per Knopfdruck.

> Status: in Entwicklung (Meilenstein M3). Fortschritt: [`docs/ROADMAP.md`](docs/ROADMAP.md)

## Vorschau

![Entwurf der Anzeige: Abfahrten am Bahnhof Emsdetten](docs/mockups/Main.png)

Entwurf, noch nicht umgesetzt – weitere Mockups (Display-Zustände, Web-UI) in [`docs/mockups/`](docs/mockups/README.md).

## Hardware
- [Waveshare ESP32-S3-LCD-3.16](https://www.waveshare.com/wiki/ESP32-S3-LCD-3.16)
  (ESP32-S3, 16 MB Flash, 8 MB PSRAM, 3,16" IPS 320×820)
- USB-Netzteil 5 V und USB-C-Kabel
- Gehäuse aus dem 3D-Drucker (folgt)

Details: [`docs/HARDWARE.md`](docs/HARDWARE.md), Entscheidungen: [`docs/ARCHITEKTUR.md`](docs/ARCHITEKTUR.md)

## Firmware flashen
Die fertige Firmware liegt unter [Releases](https://github.com/NilsAla/abfahrtstafel/releases/latest):

- **Am einfachsten:** [Flash-Seite](https://nilsala.github.io/abfahrtstafel/) in Chrome/Edge öffnen, Tafel per USB
  anschließen, „Installieren“ – spielt immer das neueste Release ein (auch als Notfall-Weg, wenn nichts mehr startet)
- `firmware-factory.bin` – Erstinstallation per USB, ab Adresse `0x0` flashen
- `firmware.bin` – wird vom Gerät selbst für Updates genutzt
- `manifest.json` – Versionsinfo und Prüfsumme für den Update-Mechanismus
- `firmware.elf` – nur zum Auswerten von Absturzberichten (`esp-coredump info_corefile -c coredump.elf firmware.elf`)

Bedienung für Endnutzer: [`docs/ANLEITUNG.md`](docs/ANLEITUNG.md)

## Einrichtung
1. Gerät mit Strom verbinden. Beim ersten Start erscheint das WLAN **Abfahrtstafel-XXXX**.
2. Mit dem Handy verbinden, die Einrichtungsseite öffnet sich automatisch. Heim-WLAN auswählen, Passwort eingeben.
3. Danach ist die Tafel im Heimnetz unter **http://abfahrt.local** erreichbar.

Zurücksetzen auf Werkseinstellungen: BOOT-Taster im laufenden Betrieb 10 Sekunden gedrückt halten.

## Selbst bauen
Voraussetzung: [PlatformIO](https://platformio.org/)

```sh
pio run -e esp32s3              # Firmware bauen
pio run -e esp32s3 -t upload    # per USB flashen
pio device monitor              # Serial-Log
pio test -e native              # Unit-Tests auf dem PC
```

Ein Release entsteht durch einen Git-Tag `vX.Y.Z` (oder manuell unter Actions → Release).

## Datenquelle und Attribution
Abfahrtsdaten von [Transitous](https://transitous.org/) – [Datenquellen](https://transitous.org/sources/).
Kartendaten © [OpenStreetMap](https://www.openstreetmap.org/copyright)-Mitwirkende.
Nicht-kommerzielles Projekt.

## Lizenz
[MIT](LICENSE)
