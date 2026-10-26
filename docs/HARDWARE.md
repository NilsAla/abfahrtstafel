# Hardware – Waveshare ESP32-S3-LCD-3.16

Stand: 2026-10-26 · Quelle: Waveshare-Wiki, Maßzeichnung aus der Produktgalerie, Schaltpläne, Community-Configs.
Werte mit (?) am Board/Schaltplan verifizieren.

## Links
- Wiki: https://www.waveshare.com/wiki/ESP32-S3-LCD-3.16
- Produktseite: https://www.waveshare.com/esp32-s3-lcd-3.16.htm
- Schaltplan V1: https://files.waveshare.com/wiki/ESP32-S3-LCD-3.16/ESP32-S3-LCD-3.16-Schematic.pdf
- Schaltplan V2 (Rev1.1): https://files.waveshare.com/wiki/ESP32-S3-LCD-3.16/ESP32-S3-LCD-3.16-Rev1.1.pdf
- Demo-Paket (Arduino + ESP-IDF + Factory-Firmware): https://drive.google.com/file/d/1fsIRFZ7HQxAZFpMyKR3LApBGr8H7ttXK/view
- Datenblätter: ST7701S, PCF85063, QMI8658 (im Wiki unter Resources)
- ESPHome-Beispielconfig (Plan B): https://community.home-assistant.io/t/waveshare-esp32-s3-lcd-3-16-sample-config/945239
- LovyanGFX-Thread: https://forum.arduino.cc/t/help-waveshare-esp32-s3-lcd-3-16-st7701-rgb565-using-lovyangfx/1427901

## Eckdaten
- ESP32-S3R8 (8 MB Octal-PSRAM im Chip), 16 MB NOR-Flash
- LCD 320×820 nativ hochkant, RGB565, ST7701, 550 cd/m², 1500:1
  - Querformat nur per Software-Rotation (Demo: `EXAMPLE_Rotate_90` in `user_config.h`)
- Platinenantenne (Keramik, oben am Board), IPEX-Anschluss per Umlöten möglich
- QMI8658 (IMU), PCF85063 (RTC) mit eigenem SH1.0-Batteriesockel (1,5–3,3 V), TF-Slot
- USB-C (natives USB des S3, CDC), MX1.25-4P USB-Header, SH1.0-4P I2C, SH1.0-4P UART
- MX1.25-2P LiPo-Anschluss mit Lade-/Entladeschaltung (nicht genutzt)
- **Ein/Aus-Schiebeschalter** (Systemstrom)
- **RST- und BOOT-Taster seitlich**, BOOT frei als Benutzertaster nutzbar (Demo: Klick/Doppelklick/Lang)
- Befestigung: 4× SMD-Kupfermuttern M2 (M2×3,5×3,5+2,0×1,2)
- Zwei Hardware-Revisionen (V1, Rev1.1) → Revision am gelieferten Board prüfen

## Flash-Layout (`partitions.csv`, eingefroren ab erstem Flash)
| Partition | Typ | Offset | Größe | Zweck |
|---|---|---|---|---|
| (Bootloader) | – | 0x0 | – | im Factory-Image enthalten |
| (Partitionstabelle) | – | 0x8000 | 0x1000 | im Factory-Image enthalten |
| nvs | data/nvs | 0x9000 | 20 KB | Konfiguration, WLAN-Daten |
| otadata | data/ota | 0xE000 | 8 KB | aktive App-Partition |
| app0 | app/ota_0 | 0x10000 | 6 MB | Firmware A |
| app1 | app/ota_1 | 0x610000 | 6 MB | Firmware B |
| coredump | data/coredump | 0xC10000 | 64 KB | Absturzanalyse (M5) |
| storage | data/spiffs | 0xC20000 | 3,875 MB | reserviert (LittleFS möglich) |

`firmware-factory.bin` wird ab 0x0 geflasht und enthält Bootloader, Partitionstabelle, boot_app0 und App.
OTA-Updates schreiben nur `firmware.bin` in die jeweils inaktive App-Partition.

## Abmessungen (Maßzeichnung Waveshare, mm)
Board hochkant betrachtet, USB-C unten, Display vorne.

| Maß | Wert |
|---|---|
| Platine B × H | 33,35 × 78,85 |
| Gesamtdicke (Display + Platine + Bauteile) | 9,30 |
| Displaymodul (Glas) B × H | 31,20 ± 0,15 × 76,60 ± 0,15 |
| Aktive Fläche B × H | 26,40 × 67,65 (≈ 2,86" Diagonale, ~0,0825 mm/px ≈ 308 ppi) |
| Rand aktive Fläche seitlich | 2,40 je Seite |
| Rand aktive Fläche unten | 3,40 (?) – Zeichnung abgeschnitten, oben folgt ≈ 5,55 |
| Displaydicke | 2,40 (Seitenansicht), 3,90 bis Platine (?) |
| Montagelöcher M2, Abstand B × H | 27,35 × 72,75 |
| Abstand Lochmitte zum Rand | 3,00 seitlich, ≈ 3,05 oben/unten |
| Linke Kante: Abstände I2C / USB-Header / UART | 16,97 / 16,97 / 19,45 (Bezugslinie am Board prüfen) |
| Rechte Kante: Lage RST / BOOT / Schalter | 10,77 / 22,72 (Bezugslinie am Board prüfen) |

Anordnung Rückseite: RTC-Sockel, I2C, USB-Header, UART links; RST, BOOT, Schiebeschalter rechts; USB-C mittig unten;
TF-Slot mittig oben; Display-FPC mittig.

Nach Lieferung mit Messschieber gegenprüfen: Bezugspunkte der Seitenmaße, untere Displaykante, Höhe der USB-C-Buchse über Platine.

## Pins (Display, vorläufig – wird in M0.5 aus Demo-Code und Schaltplan vervollständigt)
| Signal | GPIO | Quelle |
|---|---|---|
| SPI-Init SCK | 2 | ESPHome-Config |
| SPI-Init MOSI/SDA | 1 | ESPHome-Config |
| SPI-Init CS | 0 oder 42 (?) | widersprüchlich: ESPHome 0, LovyanGFX 42 |
| LCD Reset | 16 | beide |
| DE | 40 | ESPHome |
| HSYNC | 38 | ESPHome |
| VSYNC | 39 | ESPHome |
| PCLK | 41 | ESPHome, 18 MHz, pclk idle high |
| RGB-Datenpins | – | aus Schaltplan übernehmen |
| Backlight (PWM?) | – | aus Schaltplan übernehmen |
| BOOT-Taster | 0 | Standard S3 – Konflikt mit CS=0 klären |
| Batterie-ADC | ADC1 Kanal 3 | Wiki-Demo |

## Software-Vorgaben laut Waveshare
- arduino-esp32 ≥ 3.1.0 → Projekt nutzt pioarduino 55.03.312-1 (arduino-esp32 3.3.12)
- LVGL 8.3.11 oder 9.3.0 (Demos für beide), SensorLib 0.3.1 (nur für RTC/IMU)
- PSRAM: `board_build.arduino.memory_type = qio_opi`, Flash 16 MB
- Download-Modus bei Problemen: BOOT halten, RST drücken, RST loslassen, BOOT loslassen
- FAQ-Tipp: LVGL-Displaybuffer auf Vollbild setzen gegen Ruckeln

## Konsequenzen fürs Projekt
- Factory Reset über BOOT (GPIO0) zur Laufzeit – Taster vorhanden
- Ein/Aus-Schalter: im Gehäuse verdecken oder auf ON fixieren (sonst „kaputt“-Anrufe)
- Optional: Knopfzelle an RTC-Sockel → Uhrzeit sofort nach Stromausfall, auch ohne WLAN
- Lesbarkeit: aktive Fläche quer nur 67,65 × 26,40 mm bei ~308 ppi → Schriftgrößen in mm planen, nicht in px.
  Für Lesen aus 1–2 m eher 3–4 Zeilen statt 5–6; Mindest-Versalhöhe ~3 mm (≈ 36 px)
- Gehäuse: Schild-Ausschnitt ≈ 27 × 68 mm, Platine 33,35 × 78,85 × 9,3 mm mit 4× M2, Zugang zu USB-C unten und BOOT rechts
