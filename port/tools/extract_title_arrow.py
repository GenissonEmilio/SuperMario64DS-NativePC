"""Cut the title's own cursor arrow out of two captures of the main menu (run menu1, lane ASSET3).

The ROM draws one small blue triangle (a sprite object, id 0x21 in func_ov007_020c421c's object, the pad cursor of the title's
main menu) over the selected element. Capture the one-screen main menu twice at RenderScale 4 (4 host pixels per DS pixel) with the
cursor on the wrench: once as it is, once with that object skipped (SM64DS_TITLE_SKIPK with the cursor's per-frame draw index bit).
The pixels that differ are the arrow; its black drop shadow is dropped, the 4x blocks are read back to the DS pixel grid and the
grid is written 16x nearest (pixel art that any window size can filter cleanly) as arrow_up.png (tip up, as the ROM draws it at
the wrench) and arrow_down.png (flipped, for the plates along the bottom).

    python port/tools/extract_title_arrow.py <with.png> <without.png> [--out port/res/menu]
"""
import argparse, os
import numpy as np
from PIL import Image

SHADOW = (10, 15, 20)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("with_arrow")
    ap.add_argument("without_arrow")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "res", "menu"))
    a = ap.parse_args()
    A = np.array(Image.open(a.with_arrow).convert("RGB")).astype(int)
    B = np.array(Image.open(a.without_arrow).convert("RGB")).astype(int)
    diff = (abs(A - B).sum(-1) > 0) & ~((A == np.array(SHADOW)).all(-1))
    # the arrow's shadow also darkens whatever it falls on (the wrench plate): keep only the blue arrow colours
    diff &= (A[..., 2] - A[..., 0]) >= 70
    ys, xs = np.nonzero(diff)
    x0, x1, y0, y1 = xs.min() // 4, xs.max() // 4, ys.min() // 4, ys.max() // 4
    w, h = x1 - x0 + 1, y1 - y0 + 1
    g = np.zeros((h, w, 4), np.uint8)
    for j in range(h):
        for i in range(w):
            py, px = (y0 + j) * 4 + 2, (x0 + i) * 4 + 2
            if diff[py, px]:
                g[j, i, :3] = A[py, px]
                g[j, i, 3] = 255
    print("arrow grid %dx%d DS pixels at DS (%d,%d)" % (w, h, x0, y0))
    up = Image.fromarray(g, "RGBA").resize((w * 16, h * 16), Image.NEAREST)
    os.makedirs(a.out, exist_ok=True)
    up.save(os.path.join(a.out, "arrow_up.png"), optimize=True)
    up.transpose(Image.FLIP_TOP_BOTTOM).save(os.path.join(a.out, "arrow_down.png"), optimize=True)


if __name__ == "__main__":
    main()
