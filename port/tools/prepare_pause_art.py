"""Prepare the pause menu's host-resolution art (run menu1, lane PAUSE2).

Writes port/res/menu/pause/*.png from the artist's plank sources (not committed) and the DS lettering font:
planks lettered in the DS font at one common letter size, a cream card with a gold edge, small glyph art.
    python port/tools/prepare_pause_art.py --orange <multiplayer_base_hi.png> --tan <play_base_hi.png> \
        --font <sm64ds_original.otf> [--look ds|menu] [--out port/res/menu/pause] [--sheet <png>]
--look ds   : the DS plate colours: orange wood, green gradient lettering with a black outline (DEFAULT)
--look menu : the main menu's look: orange plank, brown lettering with a gold lower edge
"""
import argparse, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prepare_menu_art as pm

PW, PH = 1500, 200                      # every plank: 7.5 : 1 (the ROM plate is 240 x 32)
FILL_W, FILL_H = 0.90, 0.56

# the DS plate's own colours, measured on the reference exe (castle pause, CONTINUE plate)
DS_TOP, DS_BOT, DS_EDGE = (173, 231, 74), (90, 206, 57), (24, 24, 8)


def glyphs(font_path, text, fs=400):
    font = ImageFont.truetype(font_path, fs)
    l, t, r, b = font.getbbox(text)
    return font, (l, t, r, b)


def render(font_path, text, look):
    fs = 400
    font = ImageFont.truetype(font_path, fs)
    k = round(fs * 0.04)
    l, t, r, b = font.getbbox(text)
    W, H = r - l + 6 * k + 20, b - t + 6 * k + 20
    ox, oy = 10 + 3 * k - l, 10 + 3 * k - t
    if look == "menu":
        im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(im)
        d.text((ox + k, oy + k), text, font=font, fill=pm.GOLD + (255,))
        d.text((ox - k, oy - k), text, font=font, fill=pm.DARK + (255,))
        d.text((ox, oy), text, font=font, fill=pm.BROWN + (255,))
        return im.crop(im.getchannel("A").getbbox())
    # ds: black outline (dilated mask), a drop shadow, green vertical gradient fill
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).text((ox, oy), text, font=font, fill=255)
    bb = m.getbbox()
    out = m.filter(ImageFilter.MaxFilter(2 * k + 1))
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sh.paste((0, 0, 0, 120), (k, k), out)
    im.alpha_composite(sh)
    o = Image.new("RGBA", (W, H), DS_EDGE + (255,)); o.putalpha(out)
    im.alpha_composite(o)
    g = np.zeros((H, W, 4), np.uint8)
    y0, y1 = bb[1], bb[3]
    for y in range(H):
        f = min(1.0, max(0.0, (y - y0) / max(1, y1 - y0)))
        g[y, :, :3] = [round(DS_TOP[i] + (DS_BOT[i] - DS_TOP[i]) * f) for i in range(3)]
    g[..., 3] = np.array(m)
    im.alpha_composite(Image.fromarray(g, "RGBA"))
    return im.crop(im.getchannel("A").getbbox())


def planks_with(plank, labels, font, look, fw=FILL_W, fh=FILL_H):
    labs = [(n, render(font, t, look)) for n, t in labels]
    sc = min(PW * fw / max(i.width for _, i in labs), PH * fh / max(i.height for _, i in labs))
    res = {}
    for n, im in labs:
        lab = im.resize((max(1, round(im.width * sc)), max(1, round(im.height * sc))), Image.LANCZOS)
        o = plank.copy()
        o.alpha_composite(lab, ((PW - lab.width) // 2, (PH - lab.height) // 2))
        res[n] = o
    return res


LONG = [("continue", "CONTINUE"), ("ctl", "CONTROLLER MODES"), ("options", "OPTIONS"), ("quit", "QUIT"), ("exit", "EXIT COURSE"),
        ("save", "SAVE"), ("dontsave", "DON'T SAVE"), ("standard", "STANDARD MODE"), ("touch", "TOUCH MODE"), ("dual", "DUAL-HAND MODE")]
SHORT = [("ok", "OK"), ("yes", "YES"), ("no", "NO")]
SW = 500                                  # the short plank (OK / YES / NO): 80 x 32 DS, 2.5 : 1


def short_plank(plank):
    """a 2.5 : 1 plank from the long one: both ends kept, the middle compressed"""
    c = round(PH * 0.11)
    o = Image.new("RGBA", (SW, PH), (0, 0, 0, 0))
    o.paste(plank.crop((0, 0, c, PH)), (0, 0))
    o.paste(plank.crop((PW - c, 0, PW, PH)), (SW - c, 0))
    o.paste(plank.crop((c, 0, PW - c, PH)).resize((SW - 2 * c, PH), Image.LANCZOS), (c, 0))
    return o


def card(w=1500, h=1000):
    """the course card: cream paper, a gold frame with a light inner bevel and a dark outer line (the DS card, redrawn at window resolution)"""
    S = 2
    W, H = w * S, h * S
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    r = 26 * S
    d.rounded_rectangle((0, 0, W - 1, H - 1), r, fill=(92, 62, 20, 255))                                   # outer dark line
    t = 6 * S
    d.rounded_rectangle((t, t, W - 1 - t, H - 1 - t), r - t, fill=(222, 182, 92, 255))                      # gold band
    b = 30 * S
    d.rounded_rectangle((b, b, W - 1 - b, H - 1 - b), max(2, r - b), fill=(246, 226, 150, 255))            # light bevel
    c = 38 * S
    d.rounded_rectangle((c, c, W - 1 - c, H - 1 - c), max(2, r - c), fill=(150, 112, 48, 255))             # inner dark line
    p = 42 * S
    paper = np.zeros((H, W, 4), np.uint8)
    for y in range(H):
        f = y / H
        paper[y, :, :3] = [round(240 - 14 * f), round(232 - 16 * f), round(210 - 20 * f)]
    paper[..., 3] = 255
    pm_ = Image.new("L", (W, H), 0)
    ImageDraw.Draw(pm_).rounded_rectangle((p, p, W - 1 - p, H - 1 - p), max(2, r - p), fill=255)
    pi = Image.fromarray(paper, "RGBA")
    pi.putalpha(pm_)
    im.alpha_composite(pi)
    return im.resize((w, h), Image.LANCZOS)


def arrow(left, w=240, h=360):
    """the course paging arrow: a gold triangle, light top edge, dark outline"""
    S = 3
    W, H = w * S, h * S
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    m = int(0.10 * W)
    if left:
        tri = [(W - m, m), (W - m, H - m), (m, H // 2)]
    else:
        tri = [(m, m), (m, H - m), (W - m, H // 2)]
    out = Image.new("L", (W, H), 0)
    ImageDraw.Draw(out).polygon(tri, fill=255)
    out = out.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(S * 1.2)).point(lambda v: 255 if v > 90 else 0)
    k = Image.new("RGBA", (W, H), (60, 40, 8, 255)); k.putalpha(out)
    im.alpha_composite(k)
    fill = Image.new("L", (W, H), 0)
    inner = int(0.035 * W)
    ImageDraw.Draw(fill).polygon(tri, fill=255)
    fill = fill.filter(ImageFilter.MinFilter(2 * inner + 1))
    g = np.zeros((H, W, 4), np.uint8)
    for y in range(H):
        f = y / H
        g[y, :, :3] = [round(255 - 40 * f), round(236 - 50 * f), round(110 - 60 * f)]
    g[..., 3] = np.array(fill)
    im.alpha_composite(Image.fromarray(g, "RGBA"))
    return im.resize((w, h), Image.LANCZOS)


def status_card(font, text):
    c = card(1440, 800)
    lab = render_dark(font, text)
    sc = min(1440 * 0.62 / lab.width, 800 * 0.26 / lab.height)
    lab = lab.resize((max(1, round(lab.width * sc)), max(1, round(lab.height * sc))), Image.LANCZOS)
    c.alpha_composite(lab, ((1440 - lab.width) // 2, (800 - lab.height) // 2))
    return c


def render_dark(font_path, text):
    font = ImageFont.truetype(font_path, 300)
    l, t, r, b = font.getbbox(text)
    im = Image.new("RGBA", (r - l + 20, b - t + 20), (0, 0, 0, 0))
    ImageDraw.Draw(im).text((10 - l, 10 - t), text, font=font, fill=(78, 52, 18, 255))
    return im.crop(im.getchannel("A").getbbox())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orange", required=True)
    ap.add_argument("--tan", required=True)
    ap.add_argument("--font", required=True)
    ap.add_argument("--look", default="ds")
    ap.add_argument("--sheet", default="")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "res", "menu", "pause"))
    a = ap.parse_args()
    pm.H = PH
    if a.sheet:
        rows = []
        for src, look in ((a.orange, "ds"), (a.orange, "menu"), (a.tan, "ds"), (a.tan, "menu")):
            p = pm.plank(src, PW)
            rows.append(planks_with(p, [("c", "CONTROLLER MODES")], a.font, look)["c"])
        sh = Image.new("RGBA", (PW, PH * 4 + 30), (60, 90, 140, 255))
        for i, r in enumerate(rows):
            sh.alpha_composite(r, (0, i * (PH + 10)))
        sh.resize((PW // 2, (PH * 4 + 30) // 2), Image.LANCZOS).save(a.sheet)
        return
    os.makedirs(a.out, exist_ok=True)
    plank = pm.plank(a.orange if a.look != "tan" else a.tan, PW)
    look = "menu" if a.look == "menu" else "ds"
    labs = {n: render(a.font, t, look) for n, t in LONG + SHORT}
    # ONE letter size for every plank: the widest long label fills FILL_W, the tallest line FILL_H
    sc = min(PW * FILL_W / max(labs[n].width for n, _ in LONG), PH * FILL_H / max(i.height for i in labs.values()))
    sp = short_plank(plank)
    for n, im in labs.items():
        lab = im.resize((max(1, round(im.width * sc)), max(1, round(im.height * sc))), Image.LANCZOS)
        base = sp if n in dict(SHORT) else plank
        o = base.copy()
        o.alpha_composite(lab, ((o.width - lab.width) // 2, (PH - lab.height) // 2))
        o.save(os.path.join(a.out, "pk_%s.png" % n), optimize=True)
        print(n, o.size, "label", lab.size)
    card().save(os.path.join(a.out, "card.png"), optimize=True)
    arrow(True).save(os.path.join(a.out, "arrow_l.png"), optimize=True)
    arrow(False).save(os.path.join(a.out, "arrow_r.png"), optimize=True)
    status_card(a.font, "Saving file...").save(os.path.join(a.out, "saving.png"), optimize=True)
    status_card(a.font, "File saved.").save(os.path.join(a.out, "saved.png"), optimize=True)


if __name__ == "__main__":
    main()
