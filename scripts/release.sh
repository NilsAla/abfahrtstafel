#!/usr/bin/env bash
# Lokales Release: testet und baut auf diesem Rechner (~2 min) und veröffentlicht per gh auf GitHub.
#
#   scripts/release.sh 0.9.0-beta.1 "Was neu ist"   → Beta (Vorabversion, nur Tafeln im Beta-Kanal)
#   scripts/release.sh 0.9.0 "Was neu ist"          → reguläres Release (alle Tafeln, Flash-Seite)
#   SELFTEST_FAIL=1 scripts/release.sh …            → Test-Build, der absichtlich zurückrollt
#
# Gegenstück ohne Toolchain (Cloud-Session): release.yml per workflow_dispatch.
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION="${1:-}"
VERSION="${VERSION#v}"
NOTES="${2:-}"
REPO="NilsAla/wannbus"
PIO="$(command -v pio || echo "$HOME/.local/bin/pio")"

fail() { echo "FEHLER: $*" >&2; exit 1; }

[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$ ]] || fail "Version fehlt oder ungültig (z. B. 0.9.0-beta.1)"
[[ "$VERSION" == *-* ]] && PRE=1 || PRE=0
[ "$(git rev-parse --abbrev-ref HEAD)" = main ] || fail "nur von main aus releasen"
git diff --quiet && git diff --cached --quiet || fail "uncommittete Änderungen"
git fetch -q origin main --tags
[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] || fail "main ist nicht auf dem Stand von origin/main"
git rev-parse -q --verify "refs/tags/v$VERSION" >/dev/null && fail "Tag v$VERSION existiert schon"

echo "== Unit-Tests"
NATIVE_FLAGS=""
if [ "$(uname)" = Darwin ]; then
  # Apple-CLT liefert ein fast leeres usr/include/c++ mit, das die SDK-Header verdeckt
  NATIVE_FLAGS="-nostdinc++ -isystem $(xcrun --show-sdk-path)/usr/include/c++/v1"
fi
PLATFORMIO_BUILD_FLAGS="$NATIVE_FLAGS" "$PIO" test -e native

echo "== Firmware $VERSION bauen"
FW_FLAGS=""
[ "${SELFTEST_FAIL:-0}" = 1 ] && FW_FLAGS="-DFW_SELFTEST_FAIL" && echo "ACHTUNG: Test-Build mit absichtlichem Rollback"
FW_VERSION="$VERSION" PLATFORMIO_BUILD_FLAGS="$FW_FLAGS" "$PIO" run -e esp32s3
rm -rf dist
python3 scripts/release_files.py "$VERSION" "$REPO" "$NOTES"

echo "== Veröffentlichen"
git tag -a "v$VERSION" -m "${NOTES:-v$VERSION}"
git push -q origin "v$VERSION"
if [ "$PRE" = 1 ]; then
  gh release create "v$VERSION" dist/* --repo "$REPO" --verify-tag --title "v$VERSION" --notes "$NOTES" --prerelease --latest=false
else
  gh release create "v$VERSION" dist/* --repo "$REPO" --verify-tag --title "v$VERSION" --notes "$NOTES" --latest
fi

echo "== Beta-Kanal"
if ! gh release view beta --repo "$REPO" >/dev/null 2>&1; then
  gh release create beta --repo "$REPO" --prerelease --latest=false --target main --title "Beta-Kanal" \
    --notes "Technisches Sammel-Release für Geräte im Beta-Kanal. Enthält nur manifest.json; die Firmware liegt im jeweiligen Versions-Release."
fi
rm -f dist/beta-current.json
gh release download beta --repo "$REPO" --pattern manifest.json --output dist/beta-current.json 2>/dev/null || true
if python3 scripts/beta_newer.py "$VERSION" dist/beta-current.json; then
  gh release upload beta dist/manifest.json --repo "$REPO" --clobber
  echo "Beta-Kanal zeigt jetzt auf $VERSION"
else
  echo "Beta-Kanal unverändert (enthält bereits eine neuere Version)"
fi

if [ "$PRE" = 0 ]; then
  echo "== Flash-Seite aktualisieren"
  gh workflow run pages.yml --repo "$REPO" --ref main
fi
echo "Fertig: https://github.com/$REPO/releases/tag/v$VERSION"
