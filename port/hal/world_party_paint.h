// PARTIES INSIDE THE WORLD, the painter half. Run mp1, lane PINVITE1.
// Compiled ONLY inside tests/walk_window.cpp, after hal/player_list_paint.h
// (it reuses that painter's head and colour helpers, the chat glyphs and the
// overlay surface). Model, party word and keys: hal/world_party.h.
//
// WHERE IT SITS (512 x 384 units times OVL_SCALE, like the player list):
//   the list    x = 4, y = 48: one row per party member (head, name, stage
//               code), only while in a party of two or more.
//   the prompt  top-centre, twice the chat size: "<name> invites you: UP to
//               join" or "Leave party? DOWN again". Plain text, no buttons.
//   the toast   one line under the prompt position, a few seconds.
// Draw-only: it writes overlay pixels after the game has finished the frame.

#ifndef PORT_HAL_WORLD_PARTY_PAINT_H
#define PORT_HAL_WORLD_PARTY_PAINT_H

#ifdef OVL_SCALE

#include "hal/world_party.h"

static int wpar_text_w(const char *s, int rom, int scale)
{
    int w = 0;
    for (const char *p = s; *p; ++p) w += chat_adv((unsigned char)*p, rom, scale);
    return w;
}

static void wpar_text(const OvlSurface &fb, int x, int y, const char *s, int rom,
                      int scale, uint32_t col, int alpha)
{
    if (rom) chat_text(fb, x, y, s, (int)strlen(s), rom, scale, col, 0, alpha);
    chat_text(fb, x, y, s, (int)strlen(s), rom, scale, col, 1, alpha);
}

static void world_party_draw(const OvlSurface &fb)
{
    port::WparView v;
    port::world_party_view(&v);
    if (v.n < 2 && !v.prompt[0] && !v.toast[0]) return;
    {   /* the ROM's own head portraits, once the game files can be read */
        static int tries, got;
        if (got < 4 && tries < 6) {
            ++tries;
            got = port::head_icons_load_rom(plist_rom_path);
        }
    }
    const int rom = chat_font_build();
    const int scale = OVL_SCALE;
    const int row_h = rom ? (NT_GH + 2) * scale : OVL_LINE;
    const int head = port::kHeadIconSize * scale;
    const int gap = 3 * scale;
    int panel[4] = {0, 0, 0, 0};
    const int have_panel = hal_sub_screen_inset_rect(panel);

    if (v.n >= 2) {
        const int x0 = 4 * scale;
        int y = 48 * scale;
        int name_w = 0, code_w = 0;
        for (int i = 0; i < v.n; ++i) {
            char nm[port::kWparNameMax + 1];
            strncpy(nm, v.row[i].name, 12);
            nm[12] = 0;
            const int w1 = wpar_text_w(nm, rom, scale);
            const int w2 = wpar_text_w(v.row[i].code, rom, scale);
            if (w1 > name_w) name_w = w1;
            if (w2 > code_w) code_w = w2;
        }
        const int row_w = 2 * gap + head + gap + name_w + 2 * gap + code_w + gap;
        for (int i = 0; i < v.n; ++i) {
            const port::WparRow &r = v.row[i];
            const int ry = y;
            y += row_h + scale;
            if (have_panel && ry < panel[3] && ry + row_h > panel[1] &&
                x0 < panel[2] && x0 + row_w > panel[0])
                continue;                                  /* never over the corner map */
            ovl_shade(fb, x0, ry, row_w, row_h);
            const int alpha = r.same_place ? 255 : 170;
            plist_head(fb, x0 + gap, ry + (row_h - head) / 2, scale,
                       port::head_icon(r.character), alpha);
            char nm[port::kWparNameMax + 1];
            strncpy(nm, r.name, 12);
            nm[12] = 0;
            const int nx = x0 + gap + head + gap;
            const int cx = nx + name_w + 2 * gap;
            const int ty = ry + scale;
            wpar_text(fb, nx, ty, nm, rom, scale,
                      i == 0 ? 0xFFD8D8D8u : plist_readable(r.body_rgb), alpha);
            wpar_text(fb, cx, ty, r.code, rom, scale, 0xFFFFFFFFu, alpha);
        }
    }

    const int big = scale * 2;
    const int bigrow = rom ? (NT_GH + 2) * big : OVL_LINE * 2;
    int y = 10 * scale;
    const char *lines[2] = {v.prompt, v.toast};
    for (int k = 0; k < 2; ++k) {
        if (!lines[k][0]) continue;
        const int w = wpar_text_w(lines[k], rom, big);
        const int pad = 4 * scale;
        int bx = (ntr::active_w - w) / 2 - pad;
        if (bx < 0) bx = 0;
        ovl_shade(fb, bx, y, w + 2 * pad, bigrow + pad);
        wpar_text(fb, bx + pad, y + pad / 2, lines[k], rom, big,
                  k == 0 ? 0xFFFFFFFFu : 0xFFFFE070u, 255);
        y += bigrow + pad + 2 * scale;
    }
}
#endif  // OVL_SCALE

#endif  // PORT_HAL_WORLD_PARTY_PAINT_H
