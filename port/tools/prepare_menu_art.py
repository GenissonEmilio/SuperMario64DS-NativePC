"""Prepare the one-screen title menu's host-resolution art (run menu1, lane ASSET2).

Writes port/res/menu/*.png from the artist's sources, which are far larger than the port will ever draw and are NOT
committed. The port only decodes these PNGs (hal/menu_hostimg.cpp): no font code and no big images in the exe.

    python port/tools/prepare_menu_art.py --logo <logo_sm64_pc.png> --orange <multiplayer_base_hi.png> \
        --tan <play_base_hi.png> --green <green plank, e.g. the launcher's changelog_base.png> \
        --font <sm64ds_original.otf> [--out port/res/menu]

logo_sm64_pc.png   the colour "Super Mario 64 PC" logo, alpha fringes trimmed (stray specks dropped, the white matte the art
                   was cut on removed from the partly covered pixels, colour bled into the transparent pixels)
btn_mp.png         orange plank, "MP"
btn_adventure.png  tan plank, "ADVENTURE"
btn_rec.png        green plank, "REC ROOM" on two lines
Each plank is the artist's plank shrunk to the button height, its two ends kept and the middle compressed to the width the
ROM's own plate has on the title (the same footprint), lettered in the launcher's brown with a gold lower edge and a dark
upper edge, like the artist's PLAY label.
"""
import argparse, colorsys, os
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage as ndi

Image.MAX_IMAGE_PIXELS = None
BROWN, DARK, GOLD = (152, 80, 64), (108, 56, 44), (224, 168, 72)
H = 320                                   # every button is this tall
ADV_FW, ADV_FH = float(os.environ.get("SM64DS_ADV_FW", "0.90")), 0.44   # the ADVENTURE lettering box, as a fraction of the plank
SIZES = {"mp": 496, "adventure": 944, "rec": 512}   # ROM plate footprints 62 : 118 : 64 wide at 40 tall, x 8


def clean_logo(src, dst):
    a = np.array(Image.open(src).convert("RGBA")).astype(np.float64)
    al = a[..., 3]
    al[al <= 16] = 0
    lab, n = ndi.label(al > 0, structure=np.ones((3, 3)))
    sz = ndi.sum(np.ones_like(al), lab, range(1, n + 1))
    for i, s in enumerate(sz, 1):
        if s < 100:
            al[lab == i] = 0
    f = al / 255.0
    m = (f > 0) & (f < 1)
    for c in range(3):
        ch = a[..., c]
        ch[m] = np.clip((ch[m] - 255.0 * (1 - f[m])) / f[m], 0, 255)
    a[..., 3] = al
    solid = al >= 128
    idx = ndi.distance_transform_edt(~solid, return_distances=False, return_indices=True)
    for c in range(3):
        ch = a[..., c]
        near = ch[idx[0], idx[1]]
        ch[al == 0] = near[al == 0]
    Image.fromarray(a.astype(np.uint8), "RGBA").save(dst, optimize=True)


def plank(path, width, recolor=None):
    im = Image.open(path).convert("RGBA")
    box = im.getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox()
    im = im.crop(box)
    w0 = round(im.width * H / im.height)
    im = im.reduce(max(1, im.height // (H * 3))).resize((w0, H), Image.LANCZOS)
    if recolor is not None:
        im = recolor(im)
    c = round(H * 0.11)                    # the bevel and the rounded corners
    out = Image.new("RGBA", (width, H), (0, 0, 0, 0))
    out.paste(im.crop((0, 0, c, H)), (0, 0))
    out.paste(im.crop((w0 - c, 0, w0, H)), (width - c, 0))
    mid = im.crop((c, 0, w0 - c, H)).resize((width - 2 * c, H), Image.LANCZOS)
    out.paste(mid, (c, 0))
    return out


def hue_to(target_rgb_img):
    """a recolour that moves a plank's hue / saturation / value means to those of the target plank"""
    t = np.array(target_rgb_img.convert("RGBA")).astype(np.float64) / 255
    mask = t[..., 3] > 0.9
    tr = t[..., :3][mask]
    th = np.median([colorsys.rgb_to_hsv(*p)[0] for p in tr[::53]])
    ts = np.mean([colorsys.rgb_to_hsv(*p)[1] for p in tr[::53]])
    tv = np.mean([colorsys.rgb_to_hsv(*p)[2] for p in tr[::53]])

    def f(im):
        a = np.array(im).astype(np.float64) / 255
        rgb = a[..., :3]
        mx, mn = rgb.max(-1), rgb.min(-1)
        v = mx
        s = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
        sm = np.mean(s[a[..., 3] > 0.9]); vm = np.mean(v[a[..., 3] > 0.9])
        s2 = np.clip(s * ts / sm, 0, 1); v2 = np.clip(v * tv / vm, 0, 1)
        # hsv -> rgb at the target hue
        h = th * 6
        i = int(h) % 6; fr = h - int(h)
        p = v2 * (1 - s2); q = v2 * (1 - s2 * fr); tt = v2 * (1 - s2 * (1 - fr))
        r, g, b = [(v2, tt, p), (q, v2, p), (p, v2, tt), (p, q, v2), (tt, p, v2), (v2, p, q)][i]
        a[..., :3] = np.stack([r, g, b], -1)
        return Image.fromarray((a * 255 + 0.5).astype(np.uint8), "RGBA")
    return f


def label(font_path, lines, box_w, box_h):
    """the lettering: brown fill, dark upper-left edge, gold lower-right edge; supersampled, then fitted into box_w x box_h"""
    S = 4
    fs = 400
    font = ImageFont.truetype(font_path, fs)
    k = round(fs * 0.035)
    rows = []
    for ln in lines:
        l, t, r, b = font.getbbox(ln)
        im = Image.new("RGBA", (r - l + 4 * k + 20, b - t + 4 * k + 20), (0, 0, 0, 0))
        d = ImageDraw.Draw(im)
        ox, oy = 10 + 2 * k - l, 10 + 2 * k - t
        d.text((ox + k, oy + k), ln, font=font, fill=GOLD + (255,))
        d.text((ox - k, oy - k), ln, font=font, fill=DARK + (255,))
        d.text((ox, oy), ln, font=font, fill=BROWN + (255,))
        rows.append(im.crop(im.getchannel("A").getbbox()))
    gap = round(fs * 0.12)
    tw = max(r.width for r in rows)
    th = sum(r.height for r in rows) + gap * (len(rows) - 1)
    big = Image.new("RGBA", (tw, th), (0, 0, 0, 0))
    y = 0
    for r in rows:
        big.alpha_composite(r, ((tw - r.width) // 2, y)); y += r.height + gap
    sc = min(box_w / tw, box_h / th)
    return big.resize((max(1, round(tw * sc)), max(1, round(th * sc))), Image.LANCZOS)


def main():
    ap = argparse.ArgumentParser()
    for n in ("logo", "orange", "tan", "green", "font"):
        ap.add_argument("--" + n, required=False)
    ap.add_argument("--only", default="", help="write just this one plank (mp, adventure, rec); the logo is skipped too")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "res", "menu"))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    if not a.only:
        clean_logo(a.logo, os.path.join(a.out, "logo_sm64_pc.png"))
    green_ref = Image.open(a.green) if a.green else None
    # ADVENTURE's lettering fills 0.90 of the plank (round 3; it was 0.80), MP and REC ROOM are unchanged
    jobs = [("mp", a.orange, None, ["MP"], 0.62, 0.50), ("adventure", a.tan, None, ["ADVENTURE"], ADV_FW, ADV_FH),
            ("rec", a.tan, hue_to(green_ref) if green_ref else None, ["REC", "ROOM"], 0.70, 0.62)]
    for name, src, rec, lines, fw, fh in jobs:
        if a.only and name != a.only:
            continue
        w = SIZES[name]
        im = plank(src, w, rec)
        lab = label(a.font, lines, w * fw, H * fh)
        im.alpha_composite(lab, ((w - lab.width) // 2, (H - lab.height) // 2))
        im.save(os.path.join(a.out, "btn_%s.png" % name), optimize=True)
        print(name, im.size)


if __name__ == "__main__":
    main()
