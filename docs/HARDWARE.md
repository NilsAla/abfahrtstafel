# Hardware – Waveshare ESP32-S3-LCD-3.16

Stand: 2026-10-26 · Quelle: Waveshare-Wiki, Maßzeichnung aus der Produktgalerie, Schaltpläne, Community-Configs.
Werte mit (?) am Board/Schaltplan verifizieren.

## Links
- Wiki: https://www.waveshare.com/wiki/ESP32-S3-LCD-3.16
- Produktseite: https://www.waveshare.com/esp32-s3-lcd-3.16.htm
- Schaltplan V1: https://files.waveshare.com/wiki/ESP32-S3-LCD-3.16/ESP32-S3-LCD-3.16-Schematic.pdf
- Schaltplan V2 (Rev1.1): https://files.waveshare.com/wiki/ESP32-S3-LCD-3.16/ESP32-S3-LCD-3.16-Rev1.1.pdf
- Demo-Paket (Arduino + ESP-IDF + Factory-Firmware): https://files.waveshare.com/wiki/ESP32-S3-LCD-3.16/ESP32-S3-LCD-3.16-Demo.zip
  (Spiegel: https://drive.google.com/file/d/1fsIRFZ7HQxAZFpMyKR3LApBGr8H7ttXK/view). Lokal nach `vendor/waveshare/` entpacken, nicht committen
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
- Zwei Hardware-Revisionen (V1, Rev1.1), Pins identisch, Unterschiede siehe unten. Board 1 = Rev1.1

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

## Pins und Display-Ansteuerung (M0.5, aus Waveshare-Demo + Schaltplan)
Quelle: Waveshare-Demopaket `ESP32-S3-LCD-3.16-Demo.zip` (https://files.waveshare.com/wiki/ESP32-S3-LCD-3.16/ESP32-S3-LCD-3.16-Demo.zip,
Google-Drive-Link oft gesperrt), Arduino `08_LVGL_V9_Test` (`user_config.h`, `lvgl_port.c`, `lcd_bl_pwm_bsp.c`),
gegengeprüft mit ESP-IDF `09_FactoryProgram` und beiden Schaltplänen. Umsetzung: `src/board/board.cpp`, `src/board/st7701_init.h`.

| Signal | GPIO | Anmerkung |
|---|---|---|
| R0 R1 R2 R3 R4 | 17 46 3 8 18 | |
| G0 G1 G2 G3 G4 G5 | 14 13 12 11 10 9 | |
| B0 B1 B2 B3 B4 | 21 5 45 48 47 | |
| Reihenfolge esp_lcd `data_gpio_nums[0..15]` | B0–B4, G0–G5, R0–R4 | RGB565, D0 = LSB |
| HSYNC / VSYNC / DE / PCLK | 38 / 39 / 40 / 41 | DISP-Pin keiner (−1) |
| LCD Reset | 16 | low-aktiv, 10 ms low, 120 ms warten |
| 3-Wire-SPI SCK / SDA | 2 / 1 | nur für Init; teilt sich die Leitungen mit dem TF-Slot (SD_CLK/SD_MOSI) |
| 3-Wire-SPI CS | **0** | = BOOT-Taster, siehe unten |
| Backlight BL_PWM | 6 | LEDC 50 kHz, 8 bit, **invertiert** (Duty 255 = aus) |
| BOOT-Taster | 0 | low-aktiv, interner Pull-up |
| I2C SDA / SCL (RTC, IMU) | 15 / 7 | |
| Batterie-ADC | 4 | |

**RGB-Timing:** PCLK 18 MHz, Daten bei steigender Flanke (`pclk_active_neg = 0`), HSYNC Puls/Back/Front 6/30/30,
VSYNC Puls/Back/Front 40/20/20 → ≈ 52 Hz. Ein Framebuffer im PSRAM (512,5 KB), Bounce-Buffer 10 Zeilen im internen RAM.

**ST7701-Init:** 3-Wire-SPI, 9 Bit pro Wort (D/C-Bit zuerst, 0 = Kommando, 1 = Daten), MSB zuerst, SPI-Mode 0
(Takt ruht low, Übernahme bei steigender Flanke), ≤ 500 kHz, CS low-aktiv und pro Wort getoggelt. Sequenz in
`src/board/st7701_init.h` (MADCTL 0x00, COLMOD 0x55 = RGB565). Kein eigener Treiber nötig: das Projekt sendet die
Sequenz per Bit-Bang und nutzt danach nur `esp_lcd_new_rgb_panel()` aus ESP-IDF.

**CS 0 vs. 42 (geklärt):** Netz `LCD_CS` geht über R38 (0 Ω, bestückt) auf GPIO0; Alternativen R37 → GPIO16 und
R39 → GND sind nicht bestückt. Die GPIO-Tabelle im Schaltplan V1 nennt IO42 als „LCD_CS“ – das ist ein Tabellenfehler
(in Rev1.1 korrigiert, IO42 = SD_MISO) und vermutlich die Quelle des LovyanGFX-Werts. Demo und Factory-Firmware nutzen 0.

**Konflikt mit BOOT/GPIO0 (geklärt):** GPIO0 dient nur während der Init als CS-Ausgang. Danach wird er Eingang mit
Pull-up (wie in der Waveshare-Firmware) und ist wieder der BOOT-Taster. Ein Tastendruck zieht dann zwar CS des Panels
auf low, ohne Flanken auf SCK übernimmt der ST7701 aber nichts. Konsequenz: Nach `displayBegin()` keine weiteren
SPI-Kommandos ans Panel (Helligkeit läuft über BL_PWM, nicht per Kommando). Beim Einschalten nicht BOOT halten
(Download-Modus).

**Backlight:** BL_PWM speist über R28/C17 in den Feedback-Pin des Boost-Wandlers AP3032 → je höher der Duty, desto
dunkler. Demo: `LCD_PWM_MODE_x = 0xFF - x`. Vor `ledcAttach` ist GPIO6 hochohmig.

### Unterschiede Schaltplan V1 ↔ Rev1.1
Display-, Backlight- und BOOT-Pins sind in beiden Revisionen identisch. Abweichungen:

| Bauteil | V1 | Rev1.1 | Auswirkung |
|---|---|---|---|
| Flash | Winbond W25Q128JVSI (JEDEC 0xEF) | XMC XM25QH128 (JEDEC 0x20) | Erkennung der Revision per `esptool flash_id` |
| R29 (Strommessung Backlight) | 8,2 Ω | 11 Ω | Rev1.1 maximal etwas dunkler |
| BOOT-Taster | ohne C | C39 100 nF parallel | Entprellung in Hardware |
| I2C-Pull-ups R12/R13 | 10 kΩ | 4,7 kΩ | – |
| Lade-IC ISET R7 | 82 kΩ | 360 kΩ | Ladestrom 2 A → 0,5 A (Akku nicht genutzt) |
| RTC-Batterie | direkt | Dioden D3/D4 (B5819WS), R45 | – |
| Akku-Schaltung | Q1 | zusätzlich Q2 AO3400, D5 | – |
| GPIO-Tabelle IO42 | „LCD_CS“ + SD_MISO | nur SD_MISO | Tabellenfehler V1, siehe oben |

**Board 1 ist Rev1.1** (2026-10-28: `esptool flash_id` → Manufacturer 0x20, Device 0x4018 = XMC 16 MB). Revision ist
nicht aufgedruckt.

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
