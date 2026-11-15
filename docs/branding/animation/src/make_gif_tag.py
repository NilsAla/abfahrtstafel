import math, random, sys, os
from PIL import Image, ImageDraw, ImageFont

S = 2
W, H = 820, 320
FPS = 25
DT = 1.0 / FPS
OUT = sys.argv[1] if len(sys.argv) > 1 else "/home/claude/wb_out"

SKY = (186, 228, 248)
NAVY = (27, 31, 59)
NAVY2 = (42, 47, 90)
ROAD = (11, 13, 28)
SW = (218, 211, 198)
CURB = (240, 234, 222)
YEL = (255, 199, 44)
YEL_D = (235, 175, 25)
CORAL = (255, 79, 94)
TEAL = (31, 200, 181)
TEAL_D = (20, 150, 135)
VIO = (122, 92, 250)
VIO_D = (90, 66, 200)
CREAM = (255, 246, 229)
GLASS = (191, 239, 233)
DARK = (14, 16, 36)
SKIN = (255, 214, 180)
GRAY = (225, 225, 235)

FP = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"
if not os.path.exists(FP):
    FP = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"


def F(size):
    return ImageFont.truetype(FP, int(size * S))


def rr(d, x0, y0, x1, y1, fill, r=0, outline=None, w=0):
    d.rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], radius=r * S, fill=fill,
                        outline=outline, width=int(w * S))


def el(d, x0, y0, x1, y1, fill):
    d.ellipse([x0 * S, y0 * S, x1 * S, y1 * S], fill=fill)


def ci(d, cx, cy, r, fill):
    el(d, cx - r, cy - r, cx + r, cy + r, fill)


def poly(d, pts, fill):
    d.polygon([(x * S, y * S) for x, y in pts], fill=fill)


def ln(d, x0, y0, x1, y1, fill, w):
    d.line([x0 * S, y0 * S, x1 * S, y1 * S], fill=fill, width=max(1, int(w * S)))


def tx(d, x, y, text, font, fill, anchor="mm"):
    d.text((x * S, y * S), text, font=font, fill=fill, anchor=anchor)


# ---------- static background ----------
def build_bg():
    bg = Image.new("RGB", (W * S, H * S), SKY)
    d = ImageDraw.Draw(bg)
    ci(d, 72, 58, 22, YEL)
    ci(d, 72, 58, 30, (255, 236, 170))
    ci(d, 72, 58, 22, YEL)
    rr(d, 690, 40, 770, 58, (255, 255, 255), r=9)
    rr(d, 710, 28, 756, 48, (255, 255, 255), r=10)
    for x0, x1, top in ((30, 190, 118), (300, 440, 104), (580, 760, 126)):
        rr(d, x0, top, x1, 172, (150, 198, 228), r=10)
    rr(d, 0, 172, W, 256, (96, 104, 128))
    rr(d, 0, 256, W, H, SW)
    rr(d, 0, 256, W, 262, CURB)
    return bg


STARS = []
_r = random.Random(3)
for _ in range(34):
    STARS.append((_r.randint(8, W - 8), _r.randint(6, 118), _r.random() * 6.28))


def draw_stars(d, t):
    return


# ---------- text / logo layer ----------
def build_text_layer():
    lay = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(lay)
    f_word = F(76)
    w_wann = f_word.getlength("wann") / S
    w_bus = f_word.getlength("bus") / S
    icon = 76
    gap = 18
    total = icon + gap + w_wann + w_bus
    L = (W - total) / 2
    ix, iy = L, 30
    s = icon / 120.0
    rr(d, ix, iy, ix + icon, iy + icon, NAVY, r=28 * s, outline=NAVY2, w=1.5)
    rr(d, ix + 22 * s, iy + 20 * s, ix + 98 * s, iy + 98 * s, YEL, r=14 * s)
    rr(d, ix + 30 * s, iy + 30 * s, ix + 90 * s, iy + 66 * s, GLASS, r=6 * s)
    rr(d, ix + 22 * s, iy + 76 * s, ix + 98 * s, iy + 82 * s, CORAL)
    tx_x = L + icon + gap
    cy = iy + icon / 2 + 3
    tx(d, tx_x, cy, "wann", f_word, NAVY, anchor="lm")
    tx(d, tx_x + w_wann, cy, "bus", f_word, CORAL, anchor="lm")
    return lay, L - 2, L + total + 2


# ---------- bus ----------
def draw_bus(d, xf, door_k, ang):
    xr = xf - 250
    rr(d, xr, 164, xf, 240, YEL, r=18)
    rr(d, xr + 6, 180, xf - 62, 208, GLASS, r=8)
    rr(d, xf - 52, 180, xf - 10, 214, GLASS, r=8)
    gap = 34 * door_k
    if gap > 1:
        rr(d, xf - 58 - gap / 2, 180, xf - 58 + gap / 2, 236, (30, 34, 70), r=3)
    rr(d, xr + 2, 218, xf - 2, 225, CORAL)
    for wx in (xf - 196, xf - 48):
        ci(d, wx, 240, 16, DARK)
        ci(d, wx, 240, 6, CREAM)


# ---------- shelter, bench, sign ----------
def draw_shelter_back(d):
    rr(d, 548, 150, 556, 316, (70, 76, 110), r=2)
    rr(d, 732, 150, 740, 316, (70, 76, 110), r=2)


def draw_shelter_front(d):
    rr(d, 588, 296, 700, 306, CORAL, r=3)
    rr(d, 530, 140, 758, 156, TEAL, r=6)


def draw_sign(d, state):
    rr(d, 500, 150, 512, 316, (46, 50, 68), r=2)
    rr(d, 500, 150, 512, 196, YEL_D, r=2)
    rr(d, 492, 310, 520, 316, (46, 50, 68), r=3)
    rr(d, 422, 112, 516, 152, YEL, r=7)
    ci(d, 444, 132, 13, TEAL_D)
    ci(d, 444, 132, 10.5, YEL)
    rr(d, 439, 125, 442, 139, TEAL_D, r=1)
    rr(d, 446, 125, 449, 139, TEAL_D, r=1)
    rr(d, 439, 131, 449, 134, TEAL_D)
    rr(d, 461, 118, 509, 146, (30, 120, 125), r=6)
    rr(d, 464, 121, 506, 143, TEAL, r=4)

# ---------- lady (frontal sprite, hip-origin) ----------
def lady(d, x, y_hip, s, leg_phase=None, wave=None, bag_hand=False):
    K = 0.9 * s

    def P(px, py):
        return (x + px * K, y_hip + py * K)

    sit = leg_phase is None
    sw_l = sw_r = 0
    if not sit:
        sw_l = max(0, math.sin(leg_phase)) * 5
        sw_r = max(0, -math.sin(leg_phase)) * 5
    for sx, sw in ((-6, sw_l), (6, sw_r)):
        ln(d, *P(sx, 0), *P(sx, 20 - sw), VIO_D, 6 * K)
    poly(d, [P(-12, -36), P(12, -36), P(16, 2), P(-16, 2)], VIO)
    if wave is not None:
        ln(d, *P(12, -32), *P(26, -50 + wave), VIO, 5 * K)
    ci(d, *P(0, -48), 10 * K, SKIN)
    rr(d, *P(-15, -60), *P(15, -55), CORAL, r=2 * K)
    rr(d, *P(-8, -70), *P(8, -56), CORAL, r=5 * K)


# ---------- timeline ----------
T_ENTER0, T_ENTER1 = 0.4, 2.7
T_DOOR_OPEN = (2.9, 3.3)
T_STAND = 3.7
T_WALK = (3.9, 5.2)
T_DOOR_CLOSE = (5.3, 5.7)
T_DEPART = (6.1, 7.8)
T_END = 8.3
XF_START, XF_STOP = 0.0, 726.0
XF_OUT = 1080.0
BOOT_END = 4.0


def out_cubic(u):
    u = min(max(u, 0), 1)
    return 1 - (1 - u) ** 3


def in_cubic(u):
    u = min(max(u, 0), 1)
    return u ** 3


def lerp(a, b, u):
    return a + (b - a) * u


def lin(t, a, b):
    return min(max((t - a) / (b - a), 0), 1)


def state(t):
    st = {}
    if t < T_ENTER0:
        xf = XF_START
    elif t < T_ENTER1:
        xf = lerp(XF_START, XF_STOP, out_cubic(lin(t, T_ENTER0, T_ENTER1)))
    elif t < T_DEPART[0]:
        xf = XF_STOP
    else:
        xf = lerp(XF_STOP, XF_OUT, in_cubic(lin(t, *T_DEPART)))
    st["xf"] = xf
    k = 0.0
    if T_DOOR_OPEN[0] <= t < T_DOOR_CLOSE[0]:
        k = out_cubic(lin(t, *T_DOOR_OPEN))
    elif t >= T_DOOR_CLOSE[0]:
        k = 1 - out_cubic(lin(t, *T_DOOR_CLOSE))
    st["door"] = k
    st["dfi"] = 0 if xf < 300 else (1 if xf < XF_STOP - 2 and t < T_ENTER1 else 2)
    if t < T_ENTER0 + 0.6:
        st["dfi"] = 0
    elif t < T_ENTER1 - 0.3:
        st["dfi"] = 1
    else:
        st["dfi"] = 2
    if t >= T_DEPART[0]:
        st["dfi"] = 0
    # lady
    if t < T_STAND:
        wave = None
        if xf > 450 and t < T_STAND - 0.1:
            wave = math.sin(t * 14) * 5
        st["lady"] = ("sit", 660.0, 296.0, 1.0, None, wave)
    elif t < T_WALK[0]:
        hop = math.sin(lin(t, T_STAND, T_WALK[0]) * math.pi) * 3
        feet = 313.0
        st["lady"] = ("stand", 660.0, feet - 18 - hop, 1.0, 0.0, None)
    elif t < T_WALK[1] + 0.05:
        u = lin(t, *T_WALK)
        e = u * u * (3 - 2 * u)
        x = lerp(660, 646, e)
        feet = lerp(313, 264, e)
        s = lerp(1.0, 0.74, e)
        phase = t * 11
        bob = abs(math.sin(phase)) * 1.5
        st["lady"] = ("walk", x, feet - 18 * s - bob, s, phase, None)
    else:
        st["lady"] = None
    return st


def render(t, bg, textlayer, tl, tr):
    st = state(t)
    img = bg.copy()
    d = ImageDraw.Draw(img)
    draw_stars(d, t)
    xf = st["xf"]
    ang = xf / 15.0
    if xf > -250 and xf < W + 260:
        draw_bus(d, xf, st["door"], ang)
    # text coupled to bus
    if t < T_DEPART[0] and t >= T_ENTER0:
        edge = min(xf - 12, tr + 1)
        if edge > 0:
            ex = int(edge * S)
            crop = textlayer.crop((0, 0, min(ex, W * S), H * S))
            img.paste(crop, (0, 0), crop)
    elif t >= T_DEPART[0]:
        edge = tl + (xf - XF_STOP) / (XF_OUT - XF_STOP) * (tr - tl + 24)
        if edge < tr + 2:
            ex = max(0, int(edge * S))
            crop = textlayer.crop((ex, 0, W * S, H * S))
            img.paste(crop, (ex, 0), crop)
    d = ImageDraw.Draw(img)
    draw_shelter_back(d)
    ld = st["lady"]
    if ld is not None:
        mode, x, yh, s, ph, wave = ld
        lady(d, x, yh, s, leg_phase=ph, wave=wave)
    draw_shelter_front(d)
    draw_sign(d, st["dfi"])
    return img.resize((W, H), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    bg = build_bg()
    tl_img, tl, tr = build_text_layer()
    n = int(T_END / DT)
    frames = [render(i * DT, bg, tl_img, tl, tr) for i in range(n)]
    # global palette
    probe_ts = [0.2, 1.5, 2.8, 3.8, 4.6, 6.6]
    probes = [frames[int(t / DT)] for t in probe_ts]
    montage = Image.new("RGB", (W, H * len(probes)))
    for i, p in enumerate(probes):
        montage.paste(p, (0, i * H))
    pal = montage.quantize(256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    q = [f.quantize(palette=pal, dither=Image.Dither.NONE) for f in frames]
    dur = int(1000 / FPS)
    q[0].save(os.path.join(OUT, "wannbus_boot_loop.gif"), save_all=True, append_images=q[1:],
              duration=dur, loop=0, optimize=False, disposal=1)
    nb = int(BOOT_END / DT)
    bq = q[:nb]
    durs = [dur] * len(bq)
    durs[-1] = 1500
    bq[0].save(os.path.join(OUT, "wannbus_boot_once.gif"), save_all=True, append_images=bq[1:],
               duration=durs, optimize=False, disposal=1)
    sheet_ts = [0.3, 1.2, 2.2, 3.1, 4.4, 5.9, 6.7, 7.5]
    cols, rows = 2, 4
    sw_, sh_ = W // 2, H // 2
    sheet = Image.new("RGB", (sw_ * cols, sh_ * rows))
    for i, t in enumerate(sheet_ts):
        fr = frames[min(int(t / DT), n - 1)].resize((sw_, sh_), Image.LANCZOS)
        sheet.paste(fr, ((i % cols) * sw_, (i // cols) * sh_))
    sheet.save(os.path.join(OUT, "contact.png"))
    print(n, "frames")


main()
