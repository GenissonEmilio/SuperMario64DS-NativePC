"""Prepare the level-clear (save screen) planks for the one-screen menus (run menu1, lane SAVE2).

Writes port/res/menu/btn_save_cont.png, btn_save_quit.png and btn_cont.png: the artist's tan plank at one size (7.5 : 1),
lettered in the DS font at ONE common letter size so the three read as a set. Same look as the title planks
(prepare_menu_art.py): brown fill, dark upper-left edge, gold lower-right edge.

    python port/tools/prepare_lc_art.py --tan <play_base_hi.png> --font <sm64ds_original.otf> [--out port/res/menu]
"""
import argparse, os, sys
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prepare_menu_art as pm

PW, PH = 1500, 200                      # every plank: 7.5 : 1
LABELS = [("save_cont", "SAVE & CONTINUE"), ("save_quit", "SAVE & QUIT"), ("cont", "CONTINUE, DON'T SAVE")]
FILL_W, FILL_H = 0.90, 0.50             # the longest label fills this much of the plank, the tallest line this much


def render(font_path, text):
    fs = 400
    font = ImageFont.truetype(font_path, fs)
    k = round(fs * 0.035)
    l, t, r, b = font.getbbox(text)
    im = Image.new("RGBA", (r - l + 4 * k + 20, b - t + 4 * k + 20), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    ox, oy = 10 + 2 * k - l, 10 + 2 * k - t
    d.text((ox + k, oy + k), text, font=font, fill=pm.GOLD + (255,))
    d.text((ox - k, oy - k), text, font=font, fill=pm.DARK + (255,))
    d.text((ox, oy), text, font=font, fill=pm.BROWN + (255,))
    return im.crop(im.getchannel("A").getbbox())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tan", required=True)
    ap.add_argument("--font", required=True)
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "res", "menu"))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    pm.H = PH
    plank = pm.plank(a.tan, PW)
    labs = [(n, render(a.font, t)) for n, t in LABELS]
    # one scale for all: the widest label fits FILL_W, no label is taller than FILL_H (the cap letters share one height)
    sc = min(PW * FILL_W / max(i.width for _, i in labs), PH * FILL_H / max(i.height for _, i in labs))
    for n, im in labs:
        lab = im.resize((max(1, round(im.width * sc)), max(1, round(im.height * sc))), Image.LANCZOS)
        out = plank.copy()
        out.alpha_composite(lab, ((PW - lab.width) // 2, (PH - lab.height) // 2))
        out.save(os.path.join(a.out, "btn_%s.png" % n), optimize=True)
        print(n, out.size, "label", lab.size)


if __name__ == "__main__":
    main()
