"""Exit 0, wenn <version> neuer ist als die Version in <manifest.json> (fehlt/ungültig → auch 0).

Aufruf: python3 scripts/beta_newer.py <version> <manifest.json>
Entscheidet, ob das Beta-Manifest ersetzt wird: ein älterer Hotfix soll keine neuere Beta verdrängen.
"""
import json
import re
import sys


def key(v):
    m = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.-]+))?", v or "")
    if not m:
        return None
    pre = m.group(4)
    # SemVer-Vorrang: Release > Vorabversion; Kennungen numerisch < alphanumerisch
    ids = [(0, int(p), "") if p.isdigit() else (1, 0, p) for p in pre.split(".")] if pre else []
    return (int(m.group(1)), int(m.group(2)), int(m.group(3)), 0 if pre else 1, ids)


def main():
    new = key(sys.argv[1])
    if new is None:
        raise SystemExit(f"Ungültige Version '{sys.argv[1]}'")
    try:
        cur = key(json.load(open(sys.argv[2])).get("version"))
    except (OSError, ValueError, AttributeError):
        cur = None
    sys.exit(0 if cur is None or new > cur else 1)


if __name__ == "__main__":
    main()
