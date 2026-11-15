"""Erzeugt die Release-Dateien in dist/ aus dem fertigen esp32s3-Build.

Aufruf: python3 scripts/release_files.py <version> <repo> [notes]
Genutzt von scripts/release.sh (lokal) und .github/workflows/release.yml.
"""
import hashlib
import json
import os
import shutil
import sys

MAX_APP = 6 * 1024 * 1024  # OTA-Partition


def main():
    version, repo = sys.argv[1], sys.argv[2]
    notes = sys.argv[3] if len(sys.argv) > 3 else ""
    build = os.path.join(".pio", "build", "esp32s3")
    os.makedirs("dist", exist_ok=True)
    shutil.copy(os.path.join(build, "firmware.bin"), "dist/firmware.bin")
    shutil.copy(os.path.join(build, "firmware.factory.bin"), "dist/firmware-factory.bin")
    # Nur zum Auswerten von Absturzberichten (esp-coredump), das Gerät lädt es nie
    shutil.copy(os.path.join(build, "firmware.elf"), "dist/firmware.elf")

    data = open("dist/firmware.bin", "rb").read()
    if len(data) >= MAX_APP:
        raise SystemExit(f"firmware.bin ({len(data)} Bytes) passt nicht in die OTA-Partition")
    manifest = {
        "version": version,
        "sha256": hashlib.sha256(data).hexdigest(),
        "size": len(data),
        "url": f"https://github.com/{repo}/releases/download/v{version}/firmware.bin",
        "notes": notes.strip(),
    }
    with open("dist/manifest.json", "w") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
    print(json.dumps(manifest, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
