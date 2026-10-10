"""Read the title's blue cursor arrows straight out of the ROM's texture (run menu1, lane ARROWART1).

Both arrows are in the "title_menu_U" texture (128 x 128, 256 colours, palette "title_menu_U_pl", palette index 0 transparent) of
data/DSMT/USA/texBank_U.bmd, as their own drawings (the down arrow is not a flip of the up one):
    up   at texel (57, 80), 22 x 15
    down at texel (33, 80), 22 x 15
The BMD is stored LZ77-wrapped ("LZ77" + an LZ10 stream); a plain, already decompressed BMD works too. Colours are BGR555 widened as
(v << 3) | (v >> 2). Each arrow is written 16x nearest (352 x 240, pixel art any window size can filter cleanly) as arrow_up_rom.png and
arrow_down_rom.png (the reference and fallback art; the shipped arrows are the SVG renders of prepare_menu_svg.py). The game never reads the picture's pixel size: it draws the arrow in the fixed 22 x 15 footprint of the ROM art, tip
at the bottom centre (down) or top centre (up), so a hi-res replacement of the same 22:15 picture drops in unchanged.

    python port/tools/extract_title_arrow.py <path to data/DSMT/USA/texBank_U.bmd> [--out port/res/menu] [--out1x <folder>]
"""
import argparse, os, struct
import numpy as np
from PIL import Image

TEX_NAME = b"title_menu_U"
PAL_NAME = b"title_menu_U_pl"
ARROWS = (("up", 57, 80), ("down", 33, 80))
AW, AH = 22, 15


def lz10(d, o):
    if d[o] != 0x10:
        raise SystemExit("not an LZ10 stream")
    n = d[o + 1] | d[o + 2] << 8 | d[o + 3] << 16
    out = bytearray()
    p = o + 4
    while len(out) < n:
        flags = d[p]; p += 1
        for b in range(8):
            if len(out) >= n:
                break
            if flags & (0x80 >> b):
                a, c = d[p], d[p + 1]; p += 2
                ln, disp = (a >> 4) + 3, ((a & 15) << 8 | c) + 1
                for _ in range(ln):
                    out.append(out[-disp])
            else:
                out.append(d[p]); p += 1
    return bytes(out[:n])


def load_bmd(path):
    d = open(path, "rb").read()
    return lz10(d, 4) if d[:4] == b"LZ77" else d


def name_at(b, o):
    return b[o:o + 32].split(b"\0")[0]


def read_texture(b):
    nt, to, npal, po = struct.unpack("<IIII", b[0x14:0x24])
    tex = pal = None
    for i in range(nt):
        no, off, size = struct.unpack("<III", b[to + i * 20:to + i * 20 + 12])
        if name_at(b, no) == TEX_NAME:
            tex = (off, size)
    for i in range(npal):
        no, off, size = struct.unpack("<III", b[po + i * 16:po + i * 16 + 12])
        if name_at(b, no) == PAL_NAME:
            pal = (off, size)
    if not tex or not pal or tex[1] != 128 * 128 or pal[1] != 512:
        raise SystemExit("title_menu_U texture / palette not found as 128x128 8bpp")
    idx = np.frombuffer(b[tex[0]:tex[0] + tex[1]], np.uint8).reshape(128, 128)
    cols = struct.unpack("<256H", b[pal[0]:pal[0] + 512])
    return idx, cols


def cut(idx, cols, x, y):
    w = lambda v: ((v & 31) << 3) | ((v & 31) >> 2)
    out = np.zeros((AH, AW, 4), np.uint8)
    for j in range(AH):
        for i in range(AW):
            k = int(idx[y + j, x + i])
            if k:
                c = cols[k]
                out[j, i] = (w(c), w(c >> 5), w(c >> 10), 255)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bmd", help="path to data/DSMT/USA/texBank_U.bmd from the extracted ROM")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "res", "menu"))
    ap.add_argument("--out1x", help="also write the 1x cuts (title_cursor_blue_<up|down>_1x.png) here")
    a = ap.parse_args()
    idx, cols = read_texture(load_bmd(a.bmd))
    os.makedirs(a.out, exist_ok=True)
    for name, x, y in ARROWS:
        g = cut(idx, cols, x, y)
        print("arrow_%s: title_menu_U texel rect (%d,%d) %dx%d" % (name, x, y, AW, AH))
        if a.out1x:
            os.makedirs(a.out1x, exist_ok=True)
            Image.fromarray(g, "RGBA").save(os.path.join(a.out1x, "title_cursor_blue_%s_1x.png" % name))
        Image.fromarray(g, "RGBA").resize((AW * 16, AH * 16), Image.NEAREST).save(os.path.join(a.out, "arrow_%s_rom.png" % name), optimize=True)


if __name__ == "__main__":
    main()
