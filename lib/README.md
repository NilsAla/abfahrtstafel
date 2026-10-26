# lib/

Board-unabhängige Logik (kein `Arduino.h`), je Modul ein Unterordner.
Alles hier muss in `env:native` kompilieren und ist dort per Unit-Test abgesichert
(`pio test -e native`). Geplant: Config/Migration (M1), OTA-Manifest/SemVer (M2),
Transitous-Parser/Filter (M3), Helligkeit/Nachtmodus (M4).
