"""Rasterise the menu's vector art into the transparent PNGs of res/menu (run menu1, lane ARROWART1).

The port has no SVG renderer: this runs at prep time. Tool, pinned: Microsoft Edge (Chromium) 155.0.4283.45, headless
(msedge.exe --headless=new --screenshot with a transparent default background; it honours the SVGs' mix-blend-mode strokes, which
cairosvg does not). Sources live in res/menu/src/. A different Edge version may anti-alias a pixel differently; rerun and review.

Each entry becomes a PNG of a FIXED canvas: CANVAS_W x CANVAS_H = 22 x 15 canvas units at 32 px per unit (the ROM arrow's footprint).
The SVG is rendered, trimmed to its own drawn bounds (so a padded viewBox cannot make the art sit low or small), scaled to the
canvas width and pinned to the TIP edge: "bottom" keeps the art on the bottom edge (a down arrow, tip at the bottom centre), "top"
on the top edge (an up arrow). The game draws the PNG in the 22 x 15 footprint whatever its pixel size (menu_hostimg_size), so a
hi-res replacement made to the same rules drops in with no code change.

Add a picture = add one line to ENTRIES, then run this and python port/tools/embed_menu_art.py.

    python port/tools/prepare_menu_svg.py [--edge <msedge.exe>]
"""
import argparse, os, re, subprocess, tempfile, time
from PIL import Image

PX = 32
CANVAS_W, CANVAS_H = 22 * PX, 15 * PX
# (svg in res/menu/src, png out in res/menu, edge the art is pinned to)
ENTRIES = [
    ("title_cursor_blue_up.svg", "arrow_up.png", "top"),
    ("title_cursor_blue_down.svg", "arrow_down.png", "bottom"),
    ("title_cursor_orange_down.svg", "arrow_orange_down.png", "bottom"),
]
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"


def render(edge, svg_path):
    s = open(svg_path, encoding="utf-8").read()
    vb = [float(v) for v in re.search(r'viewBox="([^"]+)"', s).group(1).split()]
    w, h = int(round(vb[2] * PX)), int(round(vb[3] * PX))
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as td:
        page = os.path.join(td, "p.html")
        shot = os.path.join(td, "p.png")
        open(page, "w").write('<!doctype html><html><body style="margin:0;background:transparent">'
                              '<img src="file:///%s" style="display:block;width:%dpx;height:%dpx"></body></html>'
                              % (os.path.abspath(svg_path).replace("\\", "/"), w, h))
        subprocess.run([edge, "--headless=new", "--disable-gpu", "--user-data-dir=" + os.path.join(td, "prof"), "--hide-scrollbars", "--default-background-color=00000000",
                        "--screenshot=" + shot, "--window-size=%d,%d" % (w, h), "file:///" + page.replace("\\", "/")],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
        for _ in range(100):     # headless Edge can return before the screenshot file is written
            if os.path.exists(shot) and os.path.getsize(shot) > 0:
                time.sleep(0.3)
                break
            time.sleep(0.3)
        im = Image.open(shot).convert("RGBA")
        im.load()
    return im


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--edge", default=EDGE)
    a = ap.parse_args()
    here = os.path.dirname(os.path.abspath(__file__))
    res = os.path.join(here, "..", "res", "menu")
    for svg, png, edge in ENTRIES:
        im = render(a.edge, os.path.join(res, "src", svg))
        bb = im.getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox()
        art = im.crop(bb)
        sc = CANVAS_W / art.width
        art = art.resize((CANVAS_W, max(1, round(art.height * sc))), Image.LANCZOS)
        if art.height > CANVAS_H:
            raise SystemExit("%s: art is taller than the 22 x 15 footprint (%d)" % (svg, art.height))
        out = Image.new("RGBA", (CANVAS_W, CANVAS_H), (0, 0, 0, 0))
        out.paste(art, (0, CANVAS_H - art.height if edge == "bottom" else 0))
        out.save(os.path.join(res, png), optimize=True)
        print("%s -> %s: art bounds %s, drawn %dx%d, pinned %s, canvas %dx%d" % (svg, png, bb, art.width, art.height, edge, CANVAS_W, CANVAS_H))


if __name__ == "__main__":
    main()
