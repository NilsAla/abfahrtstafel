"""Injiziert FW_VERSION in den Build.

Reihenfolge:
  1. Umgebungsvariable FW_VERSION (setzt release.yml)
  2. Git-Tag exakt auf HEAD (vX.Y.Z)
  3. Fallback 0.0.0-dev
"""
import os
import re
import subprocess

Import("env")  # noqa: F821  (von PlatformIO bereitgestellt)

SEMVER = re.compile(r"^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$")


def from_git():
    try:
        tag = subprocess.check_output(
            ["git", "describe", "--tags", "--exact-match"],
            stderr=subprocess.DEVNULL,
            cwd=env.subst("$PROJECT_DIR"),  # noqa: F821
        ).decode().strip()
    except Exception:
        return None
    return tag[1:] if tag.startswith("v") else tag


version = os.environ.get("FW_VERSION") or from_git() or "0.0.0-dev"
if not SEMVER.match(version):
    raise SystemExit(f"FW_VERSION '{version}' ist kein gültiges SemVer")

print(f"FW_VERSION = {version}")
env.Append(CPPDEFINES=[("FW_VERSION", env.StringifyMacro(version))])  # noqa: F821
