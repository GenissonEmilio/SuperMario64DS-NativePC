"""Regenerate hal/menu_logo_data.cpp (the menu art PNGs of res/menu as C byte arrays, so the exe carries them).
Run after tools/prepare_menu_art.py:   python port/tools/embed_menu_art.py"""
import os
here = os.path.dirname(os.path.abspath(__file__))
res = os.path.join(here, "..", "res", "menu")
names = [("logo", "logo_sm64_pc.png"), ("btn_mp", "btn_mp.png"), ("btn_adventure", "btn_adventure.png"), ("btn_rec", "btn_rec.png"),
         ("arrow_up", "arrow_up.png"), ("arrow_down", "arrow_down.png"), ("pattern", "pattern.png"), ("title_star", "title_star.png"),
         ("btn_save_cont", "btn_save_cont.png"), ("btn_save_quit", "btn_save_quit.png"), ("btn_cont", "btn_cont.png"),
         ("arrow_orange", "arrow_orange_down.png")]
out = ["/* menu_logo_data.cpp -- the menu art PNGs of res/menu as bytes (tools/embed_menu_art.py). Generated; do not edit. */"]
for tag, f in names:
    d = open(os.path.join(res, f), "rb").read()
    out.append("extern \"C\" const unsigned char g_menu_png_%s[] = {" % tag)
    for i in range(0, len(d), 32):
        out.append(",".join(str(b) for b in d[i:i + 32]) + ",")
    out.append("};")
    out.append("extern \"C\" const unsigned int g_menu_png_%s_len = %d;" % (tag, len(d)))
open(os.path.join(here, "..", "hal", "menu_logo_data.cpp"), "w", newline="\n").write("\n".join(out) + "\n")
