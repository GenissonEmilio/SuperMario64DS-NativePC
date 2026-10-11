"""Prepare the new pause menu's host-resolution art (run menu1, lane PAUSE3).

Writes port/res/menu/pause3/*.png from the launcher's art folder (res/art of the launcher repo) and the DS lettering font, at the
sizes the port draws them at (the originals are far larger and are NOT committed):

    python port/tools/prepare_pause3_art.py --art <launcher res/art> --font <sm64ds_original.otf> [--arrow <res/menu/arrow_down.png>] [--out port/res/menu/pause3]

pk_*          the lettered wooden planks (tan / purple / orange / green launcher planks, white lettering with a dark outline)
title_blank   the blank title plank the pause draws its own lettering on (course name, SAVE, QUIT ...)
tab_l, tab_r  the 4:3 edge tabs (LB / RB) that slide the planks and the star tracker
star_got, star_off, coin_gold, coin_red   the tracker's icons (drawn here, supersampled; a stand-in until the artist makes the art)
Then run tools/embed_pause3_art.py.
"""
import argparse, math, os
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

Image.MAX_IMAGE_PIXELS = None
PLANK_W = 700            # a plank's width in pixels (606 x 129 art ratio)
DKBROWN = (92, 44, 10)
BROWN = (122, 58, 12)
CREAM = (255, 243, 205)
FADED = (166, 124, 74)

PLANKS = [   # file, base art, label, outline colour
    ("pk_resume", "play_base.png", "Resume", (110, 70, 10)),
    ("pk_settings", "settings_base.png", "Settings", (70, 30, 100)),
    ("pk_exit", "multiplayer_base.png", "Exit Course", (130, 50, 10)),
    ("pk_savequit", "changelog_base.png", "Save & Quit", (50, 80, 10)),
    ("pk_quitnosave", "multiplayer_base.png", "Quit without saving", (130, 50, 10)),
    ("pk_yesquit", "multiplayer_base.png", "Yes, quit", (130, 50, 10)),
    ("pk_nokeep", "play_base.png", "No, keep playing", (110, 70, 10)),
]


def scaled(path, w=None, h=None):
    im = Image.open(path).convert("RGBA")
    if w is None:
        w = round(im.width * h / im.height)
    if h is None:
        h = round(im.height * w / im.width)
    return im.resize((int(w), int(h)), Image.LANCZOS)


def lettered_plank(art, font_path, base, label, outline, out):
    b = scaled(os.path.join(art, base), w=PLANK_W)
    b = b.crop(b.getbbox())                    # the launcher planks have uneven margins: crop to the visible bounds
    size = int(b.height * 0.5)
    maxw = b.width - 70
    f = ImageFont.truetype(font_path, size)
    while size > 14 and f.getlength(label) > maxw:
        size -= 1
        f = ImageFont.truetype(font_path, size)
    d = ImageDraw.Draw(b)
    d.text((b.width / 2, b.height / 2 + b.height * 0.02), label, font=f, fill=(255, 255, 255), anchor="mm",
           stroke_width=max(2, round(size / 11)), stroke_fill=outline)
    b.save(out, optimize=True)
    return b.size


def star_icon(sz, got):
    S = 8
    n = sz * S
    im = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx = cy = n / 2
    R, r = n * 0.47, n * 0.20
    pts = []
    for i in range(10):
        a = -math.pi / 2 + i * math.pi / 5
        rr = R if i % 2 == 0 else r
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a) + n * 0.03))
    if got:
        d.polygon(pts, fill=(255, 214, 38), outline=DKBROWN, width=int(S * 1.6))
        d.line(pts + [pts[0]], fill=DKBROWN, width=int(S * 1.6), joint="curve")
        d.ellipse((cx - n * .15, cy - n * .08, cx - n * .07, cy + n * .08), fill=DKBROWN)
        d.ellipse((cx + n * .07, cy - n * .08, cx + n * .15, cy + n * .08), fill=DKBROWN)
    else:
        d.polygon(pts, fill=(214, 190, 120), outline=FADED, width=int(S * 1.4))
        d.line(pts + [pts[0]], fill=FADED, width=int(S * 1.4), joint="curve")
    return im.resize((sz, sz), Image.LANCZOS)


def coin_icon(sz, red):
    S = 8
    n = sz * S
    im = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    fill = (214, 52, 40) if red else (255, 206, 40)
    rim = (150, 20, 16) if red else DKBROWN
    d.ellipse((n * .06, n * .06, n * .94, n * .94), fill=fill, outline=rim, width=int(S * 1.8))
    d.ellipse((n * .24, n * .24, n * .76, n * .76), outline=rim, width=int(S * 1.2))
    if red:
        d.rectangle((n * .44, n * .34, n * .56, n * .66), fill=(255, 226, 210))
    else:
        d.rectangle((n * .45, n * .36, n * .55, n * .64), fill=rim)
    return im.resize((sz, sz), Image.LANCZOS)


def edge_tab(side, plank_path, arrow_path, font_path, out):
    tw, th = 192, 340                                   # twice the 96 x 170 design size
    t = scaled(plank_path, w=th).rotate(90, expand=True).resize((tw, th), Image.LANCZOS)
    m = Image.new("L", (tw * 2, th * 2), 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, tw * 2 - 1, th * 2 - 1), 28, fill=255)
    m = m.resize((tw, th), Image.LANCZOS)
    t.putalpha(ImageChops.multiply(t.getchannel("A"), m))
    ring = Image.new("L", (tw * 2, th * 2), 0)
    ImageDraw.Draw(ring).rounded_rectangle((0, 0, tw * 2 - 1, th * 2 - 1), 28, fill=255)
    ImageDraw.Draw(ring).rounded_rectangle((6, 6, tw * 2 - 7, th * 2 - 7), 24, fill=0)
    ring = ring.resize((tw, th), Image.LANCZOS)
    col = Image.new("RGBA", (tw, th), DKBROWN + (255,))
    col.putalpha(ring)
    t.alpha_composite(col)
    a = Image.open(arrow_path).convert("RGBA")
    a = a.rotate(-90 if side == "L" else 90, expand=True, resample=Image.BICUBIC)   # the art points down; PIL turns counter clockwise
    a = a.crop(a.getbbox())
    f = 68 / max(a.width, a.height)
    a = a.resize((max(1, round(a.width * f)), max(1, round(a.height * f))), Image.LANCZOS)
    t.alpha_composite(a, (tw // 2 - a.width // 2, 36))
    st = star_icon(68, True)
    t.alpha_composite(st, (tw // 2 - 34, th // 2 - 16))
    d = ImageDraw.Draw(t)
    ft = ImageFont.truetype(font_path, 52)
    d.text((tw / 2, th / 2 + 104), "LB" if side == "L" else "RB", font=ft, fill=CREAM, anchor="mm", stroke_width=5, stroke_fill=DKBROWN)
    t.save(out, optimize=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--art", required=True, help="the launcher repo's res/art folder")
    ap.add_argument("--font", required=True, help="sm64ds_original.otf")
    ap.add_argument("--arrow", default=None, help="port/res/menu/arrow_down.png (the blue selection arrow)")
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument("--out", default=os.path.join(here, "..", "res", "menu", "pause3"))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    arrow = a.arrow or os.path.join(here, "..", "res", "menu", "arrow_down.png")
    for name, base, label, ol in PLANKS:
        print(name, lettered_plank(a.art, a.font, base, label, ol, os.path.join(a.out, name + ".png")))
    t = scaled(os.path.join(a.art, "page_title_blank.png"), w=960)
    t = t.crop(t.getbbox())
    t.save(os.path.join(a.out, "title_blank.png"), optimize=True)
    print("title_blank", t.size)
    plank = os.path.join(a.art, "play_base.png")
    edge_tab("L", plank, arrow, a.font, os.path.join(a.out, "tab_l.png"))
    edge_tab("R", plank, arrow, a.font, os.path.join(a.out, "tab_r.png"))
    star_icon(128, True).save(os.path.join(a.out, "star_got.png"), optimize=True)
    star_icon(128, False).save(os.path.join(a.out, "star_off.png"), optimize=True)
    coin_icon(128, False).save(os.path.join(a.out, "coin_gold.png"), optimize=True)
    coin_icon(128, True).save(os.path.join(a.out, "coin_red.png"), optimize=True)


main()
