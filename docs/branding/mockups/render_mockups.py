"""Rendert die .dc.html-Artboards des Canvas als PNG (2x). Aufruf: render_mockups.py <canvas-dir> <fonts-dir> <out-dir>"""
import re, sys, json, pathlib
from playwright.sync_api import sync_playwright
src, fonts, out = map(pathlib.Path, sys.argv[1:4])
cfg = json.loads((src / "canvas.json").read_text())
face = "".join(
    f"@font-face{{font-family:'Barlow Semi Condensed';font-weight:{w};src:url('file://{(fonts/f'BarlowSemiCondensed-{n}.ttf').resolve()}')}}"
    for n, w in (("Regular", 400), ("Medium", 500), ("SemiBold", 600))
    if (fonts / f"BarlowSemiCondensed-{n}.ttf").exists())
out.mkdir(parents=True, exist_ok=True)
with sync_playwright() as p:
    b = p.chromium.launch(executable_path="/usr/bin/google-chrome", args=["--no-sandbox"])
    for name, bd in cfg["boards"].items():
        html = (src / name).read_text()
        m = re.search(r"<x-dc>(.*)</x-dc>", html, re.S)
        body = re.sub(r"<helmet>.*?</helmet>", "", m.group(1), flags=re.S)
        pg = b.new_page(viewport={"width": bd["w"], "height": bd["h"]}, device_scale_factor=2)
        pg.set_content(f"<style>{face}body{{margin:0}}</style>{body}")
        pg.wait_for_timeout(300)
        h = pg.evaluate("document.documentElement.scrollHeight")
        pg.set_viewport_size({"width": bd["w"], "height": max(bd["h"], h)})
        pg.screenshot(path=str(out / (name.replace(".dc.html", "") + ".png")), full_page=True)
        pg.close()
    b.close()
