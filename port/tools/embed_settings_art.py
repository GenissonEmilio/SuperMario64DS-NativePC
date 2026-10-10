"""Regenerate hal/settings_art_data.cpp (the settings page's PNGs of res/menu/settings as C byte arrays, so the exe carries them).
Run after tools/prepare_settings_art.py:   python port/tools/embed_settings_art.py"""
import os
here = os.path.dirname(os.path.abspath(__file__))
res = os.path.join(here, "..", "res", "menu", "settings")
names = ["parchment", "title", "back", "key", "plank", "dd_top", "dd_mid", "dd_bot", "dd_arrow", "font"]
out = ["/* settings_art_data.cpp -- the settings page's PNGs of res/menu/settings as bytes (tools/embed_settings_art.py). Generated; do not edit. */"]
for tag in names:
    d = open(os.path.join(res, tag + ".png"), "rb").read()
    out.append("extern \"C\" const unsigned char g_setart_png_%s[] = {" % tag)
    for i in range(0, len(d), 32):
        out.append(",".join(str(b) for b in d[i:i + 32]) + ",")
    out.append("};")
    out.append("extern \"C\" const unsigned int g_setart_png_%s_len = %d;" % (tag, len(d)))
open(os.path.join(here, "..", "hal", "settings_art_data.cpp"), "w", newline="\n").write("\n".join(out) + "\n")
