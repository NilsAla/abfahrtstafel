"""Bettet die Web-UI gzip-komprimiert in die Firmware ein.

Liest alle Dateien aus web/ (außer README.md) und erzeugt src/generated/web_assets.h.
Läuft als PlatformIO-Pre-Script vor jedem Build, lässt sich aber auch direkt aufrufen:
    python3 scripts/embed_web.py
Die Ausgabe ist deterministisch (gzip mtime=0) und wird nur bei Änderungen neu geschrieben,
damit kein unnötiger Rebuild entsteht.
"""
import gzip
import hashlib
import os
import sys

try:
    Import("env")  # noqa: F821  (von PlatformIO bereitgestellt)
    PROJECT_DIR = env.subst("$PROJECT_DIR")  # noqa: F821
except NameError:
    PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(sys.argv[0])))

WEB_DIR = os.path.join(PROJECT_DIR, "web")
OUT_FILE = os.path.join(PROJECT_DIR, "src", "generated", "web_assets.h")

MIME = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".svg": "image/svg+xml",
    ".ico": "image/x-icon",
    ".png": "image/png",
    ".json": "application/json",
}
SKIP = {"README.md"}


def collect():
    assets = []
    for root, _, files in os.walk(WEB_DIR):
        for name in sorted(files):
            if name in SKIP or name.startswith("."):
                continue
            path = os.path.join(root, name)
            ext = os.path.splitext(name)[1].lower()
            if ext not in MIME:
                raise SystemExit(f"embed_web: unbekannter Dateityp {path}")
            url = "/" + os.path.relpath(path, WEB_DIR).replace(os.sep, "/")
            raw = open(path, "rb").read()
            gz = gzip.compress(raw, compresslevel=9, mtime=0)
            etag = '"' + hashlib.sha256(gz).hexdigest()[:16] + '"'
            assets.append((url, MIME[ext], gz, etag, len(raw)))
    assets.sort(key=lambda a: a[0])
    return assets


def render(assets):
    out = [
        "// Automatisch erzeugt von scripts/embed_web.py – nicht bearbeiten.",
        "#pragma once",
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "struct WebAsset {",
        "  const char *path;",
        "  const char *mime;",
        "  const uint8_t *data;  // gzip",
        "  size_t len;",
        "  const char *etag;",
        "};",
        "",
    ]
    for i, (url, _, gz, _, raw_len) in enumerate(assets):
        out.append(f"// {url}: {raw_len} B → {len(gz)} B gzip")
        out.append(f"static const uint8_t kWebAsset{i}[] = {{")
        for off in range(0, len(gz), 20):
            out.append("  " + ", ".join(f"0x{b:02x}" for b in gz[off:off + 20]) + ",")
        out.append("};")
        out.append("")
    out.append("static const WebAsset kWebAssets[] = {")
    for i, (url, mime, gz, etag, _) in enumerate(assets):
        etag_c = etag.replace('"', '\\"')
        out.append(f'  {{"{url}", "{mime}", kWebAsset{i}, {len(gz)}, "{etag_c}"}},')
    out.append("};")
    out.append(f"static const size_t kWebAssetCount = {len(assets)};")
    out.append("")
    return "\n".join(out)


def main():
    assets = collect()
    if not any(a[0] == "/index.html" for a in assets):
        raise SystemExit("embed_web: web/index.html fehlt")
    content = render(assets)
    os.makedirs(os.path.dirname(OUT_FILE), exist_ok=True)
    old = open(OUT_FILE, encoding="utf-8").read() if os.path.exists(OUT_FILE) else None
    total = sum(len(a[2]) for a in assets)
    if content != old:
        with open(OUT_FILE, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"embed_web: {len(assets)} Dateien, {total} B gzip → {os.path.relpath(OUT_FILE, PROJECT_DIR)}")
    else:
        print(f"embed_web: unverändert ({len(assets)} Dateien, {total} B gzip)")


main()
