"""Erzeugt Mockups 'Marke auf dem Display' (0/1/2 Abfahrten, Tag/Nacht). Aufruf: render_marke.py <canvas-dir> <fonts-dir> <out-dir>"""
import re, sys, pathlib
from playwright.sync_api import sync_playwright
src, fonts, out = map(pathlib.Path, sys.argv[1:4])
face = "".join(f"@font-face{{font-family:'Barlow Semi Condensed';font-weight:{w};src:url('file://{(fonts/f'BarlowSemiCondensed-{n}.ttf').resolve()}')}}"
               for n, w in (("Medium", 500), ("SemiBold", 600)))
ICON = lambda s: (f'<svg width="{s}" height="{s}" viewBox="0 0 120 120"><rect width="120" height="120" rx="28" fill="#1B1F3B"/>'
  '<rect x="22" y="20" width="76" height="78" rx="14" fill="#FFC72C"/><rect x="30" y="30" width="60" height="36" rx="6" fill="#BFEFE9"/>'
  '<rect x="22" y="76" width="76" height="6" fill="#FF4F5E"/><circle cx="37" cy="90" r="4" fill="#FFF6E5"/><circle cx="83" cy="90" r="4" fill="#FFF6E5"/></svg>')
T = {"Nacht": dict(muted="#A9ABC9", coral="#FF6B78", text="#FFF6E5", bg="#0E1024"),
     "Tag": dict(muted="#4A5075", coral="#C81F33", text="#1B1F3B", bg="#EAF4FA")}
def brand(t, icon, size, gap):
    return (f'<div style="flex:1;display:flex;align-items:center;justify-content:center;gap:{gap}px">{ICON(icon)}'
            f'<span style="font-size:{size}px;line-height:1;letter-spacing:-0.5px;color:{t["muted"]}">wann<span style="color:{t["coral"]}">bus</span></span></div>')
with sync_playwright() as p:
    b = p.chromium.launch(executable_path="/usr/bin/google-chrome", args=["--no-sandbox"])
    for mode in T:
        t = T[mode]
        html = (src / f"Display_{mode}.dc.html").read_text()
        root = re.search(r"<x-dc>.*?(<div style=\"width:820px.*)</x-dc>", html, re.S).group(1)
        mark = '<div style="display:flex;align-items:center;gap:18px;height:78px'
        parts = root.split(mark)
        opening = parts[0]
        rows = [mark + x for x in parts[1:]]
        rows[-1] = rows[-1][:rows[-1].rindex("</div>")]   # schließendes </div> der Wurzel entfernen
        for n, (ic, sz, gp) in {0: (96, 64, 22), 1: (72, 56, 18), 2: (40, 40, 12)}.items():
            body = opening + "".join(rows[:n]) + brand(t, ic, sz, gp) + "</div>"
            if n == 0: body = opening.replace('gap:8px', 'gap:8px') + brand(t, 96, 64, 22) + "</div>"
            pg = b.new_page(viewport={"width": 820, "height": 320}, device_scale_factor=2)
            pg.set_content(f"<style>{face}body{{margin:0}}</style>{body}")
            pg.wait_for_timeout(200)
            out.mkdir(parents=True, exist_ok=True)
            pg.screenshot(path=str(out / f"Display_Marke_{n}Zeilen_{mode}.png")); pg.close()
    b.close()
