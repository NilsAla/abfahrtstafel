# lib/

Board-unabhängige Logik (kein `Arduino.h`), je Modul ein Unterordner mit `src/`.
Alles hier muss in `env:native` kompilieren und ist dort per Unit-Test abgesichert
(`pio test -e native`, Tests in `test/test_<modul>/`).

| Modul | Inhalt | Meilenstein |
|---|---|---|
| `config` | Konfigurationsmodell, NVS-Schema/Migration (über `KvStore`), Validierung, JSON | M1 |
| `logring` | Zeilen-Ringpuffer für `/api/log` | M1 |
| `wifisup` | WLAN-Statemachine (Einrichtung, Reconnect, Fallback-Portal), AP-Name | M1 |
| `holdbutton` | Entprellter Taster mit Halte-Erkennung (Factory Reset) | M1 |
| `timeutil` | Zeitzone Europe/Berlin, Plausibilität der Systemzeit | M1 |

Geplant: OTA-Manifest/SemVer (M2), Transitous-Parser/Filter (M3), Helligkeit/Nachtmodus (M4).
