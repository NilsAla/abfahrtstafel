"""Erzeugt test/test_transitous/fixtures.h aus test/fixtures/transitous/*.json.

Native Tests laufen ohne Dateisystemzugriff auf das Projekt, daher werden die
Fixtures als Raw-String-Literale eingebettet. Nach neuen Fixtures erneut ausführen:
    python3 scripts/fixtures_header.py
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "test", "fixtures", "transitous")
OUT = os.path.join(ROOT, "test", "test_transitous", "fixtures.h")

parts = ["// Automatisch erzeugt von scripts/fixtures_header.py – nicht bearbeiten.", "#pragma once", ""]
for name in sorted(os.listdir(SRC)):
    if not name.endswith(".json"):
        continue
    data = open(os.path.join(SRC, name), encoding="utf-8").read().strip()
    if ')FIX"' in data:
        raise SystemExit(f"{name}: enthält den Raw-String-Begrenzer")
    ident = "kFixture_" + re.sub(r"[^0-9A-Za-z]", "_", name[:-5])
    parts.append(f"static const char {ident}[] = R\"FIX({data})FIX\";")
    parts.append("")
open(OUT, "w", encoding="utf-8").write("\n".join(parts))
print(f"{OUT}: {len(parts) // 2 - 1} Fixtures")
