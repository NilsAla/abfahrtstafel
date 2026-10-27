#!/usr/bin/env python3
"""Rendert alle Display-Zustände als PNG nach docs/screens/.

    pio run -e sim
    python3 scripts/render_screens.py            # nutzt .pio/build/sim/program
    python3 scripts/render_screens.py <programm> # anderer Pfad

Der Simulator schreibt PPM-Rohbilder; hier werden sie ohne Zusatzpakete (nur zlib) zu PNG.
"""
import pathlib
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "screens"


def read_ppm(path):
    data = path.read_bytes()
    parts = data.split(maxsplit=4)  # P6, Breite, Höhe, Maxwert, Pixel
    if parts[0] != b"P6" or parts[3] != b"255":
        raise ValueError(f"{path}: kein 8-Bit-PPM")
    return int(parts[1]), int(parts[2]), parts[4]


def write_png(path, width, height, rgb):
    def chunk(kind, payload):
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    stride = width * 3
    raw = b"".join(b"\x00" + rgb[y * stride:(y + 1) * stride] for y in range(height))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def main():
    program = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / ".pio" / "build" / "sim" / "program"
    if not program.exists():
        sys.exit(f"{program} fehlt – zuerst `pio run -e sim` ausführen")
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob("*.png"):  # README.md bleibt
        old.unlink()
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run([str(program), tmp], check=True, stdout=subprocess.DEVNULL)
        for ppm in sorted(pathlib.Path(tmp).glob("*.ppm")):
            w, h, rgb = read_ppm(ppm)
            target = OUT / (ppm.stem + ".png")
            write_png(target, w, h, rgb)
            print(f"{target.relative_to(ROOT)}  {target.stat().st_size // 1024} KB")


if __name__ == "__main__":
    main()
