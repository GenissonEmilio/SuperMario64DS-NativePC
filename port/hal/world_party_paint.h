// PARTIES INSIDE THE WORLD, the painter half. Run mp1, lane PINVITE1.
// Compiled ONLY inside tests/walk_window.cpp, after hal/player_list_paint.h
// (it reuses that painter's head and colour helpers, the chat glyphs and the
// overlay surface). Model, party word and keys: hal/world_party.h.
//
// WHERE IT SITS (512 x 384 units times OVL_SCALE, like the player list):
//   the list    x = 4, y = 48: one row per party member (head, name, stage
//               code), only while in a party of two or more.
//   the prompt  top-centre, chat size: "<name> invites you: UP to
//               join" or "Leave party? DOWN again". Plain text, no buttons.
//   the toast   one line under the prompt position, a few seconds.
//   the D-pad   (MPEXTRAS1) a small cross on the right edge, a little above the
//               middle: arms that do something are lit and named to the left
//               ("Invite", "Accept", "Leave" ...), the rest stay dark. Clear of
//               the star / coin counters above, the corner map and the ping box
//               below, and the chat and lists on the left.
//   the count   (Tango 10-28) "N online" right under the cross, right-aligned
//               with it: everyone on this server, me included.
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


/* a translucent flat rectangle (the D-pad icon's plates), bounds-checked */
static void wpar_rect(const OvlSurface &fb, int x0, int y0, int w, int h, uint32_t rgb, int alpha)
{
    ntr::gx_card_settle_because("a host overlay (D-pad icon)");
    const int r = (int)((rgb >> 16) & 255), g = (int)((rgb >> 8) & 255), b = (int)(rgb & 255);
    for (int y = y0; y < y0 + h; ++y) {
        if (y < 0 || y >= ntr::active_h) continue;
        uint32_t *row = fb.px + (size_t)y * (size_t)fb.stride;
        for (int x = x0; x < x0 + w; ++x) {
            if (x < 0 || x >= ntr::active_w) continue;
            const uint32_t p = row[x];
            const int pr = (int)((p >> 16) & 255), pg = (int)((p >> 8) & 255), pb = (int)(p & 255);
            row[x] = 0xFF000000u | ((uint32_t)((pr * (255 - alpha) + r * alpha) / 255) << 16) |
                     ((uint32_t)((pg * (255 - alpha) + g * alpha) / 255) << 8) |
                     (uint32_t)((pb * (255 - alpha) + b * alpha) / 255);
        }
    }
}

/* one arm of the cross: a square plate with an arrow pointing out. dir 0 up, 1 down, 2 left, 3 right */
static void wpar_arm(const OvlSurface &fb, int x0, int y0, int a, int dir, bool lit)
{
    wpar_rect(fb, x0, y0, a, a, lit ? 0xFFF8D0u : 0x202020u, 255);
    const int ah = a / 2 + 1, hb = a / 3 + 1, q0 = (a - ah) / 2;
    const uint32_t col = lit ? 0x302008u : 0x606060u;
    for (int v = 0; v < a; ++v)
        for (int u = 0; u < a; ++u) {
            int p, q;
            switch (dir) {
                case 0: p = u; q = v; break;
                case 1: p = u; q = a - 1 - v; break;
                case 2: p = v; q = u; break;
                default: p = v; q = a - 1 - u; break;
            }
            if (q < q0 || q >= q0 + ah) continue;
            const int d = 2 * p - (a - 1);
            if ((d < 0 ? -d : d) <= 2 * hb * (q - q0) / (ah > 1 ? ah - 1 : 1) + 1)
                wpar_rect(fb, x0 + u, y0 + v, 1, 1, col, 255);
        }
}

/* THE D-PAD ICON: what UP and DOWN do right now (hal/world_party.h, WparHint). Draws nothing
   while the D-pad is not the party's (a menu, a message, the pause, typing, no world room). */
static void world_party_dpad_draw(const OvlSurface &fb)
{
    port::WparHint h;
    port::world_party_hint(&h);
    if (!h.shown) return;
    const int rom = chat_font_build();
    const int scale = OVL_SCALE;
    const int a = 8 * scale;
    const int row_h = rom ? (NT_GH + 2) * scale : OVL_LINE;
    int cx = ntr::active_w - 6 * scale - 3 * a;      /* the cross's left edge */
    int cy = ntr::active_h * 46 / 100 - (3 * a) / 2; /* its top edge */
    int lw = 0;
    const char *lab[2] = {h.up, h.down};
    for (int k = 0; k < 2; ++k)
        if (lab[k][0]) {
            const int w = wpar_text_w(lab[k], rom, scale);
            if (w > lw) lw = w;
        }
    /* the player count's line under the cross */
    char cnt[24] = {0};
    if (h.online > 0) snprintf(cnt, sizeof cnt, "%d online", h.online);
    const int cnt_w = cnt[0] ? wpar_text_w(cnt, rom, scale) : 0;
    const int cnt_h = cnt[0] ? 3 * scale + row_h : 0;
    /* never over the corner map or the ping box: lift the whole icon (and its count) above it */
    int panel[4];
    if (hal_sub_screen_inset_rect(panel)) {
        int left = cx - (lw ? lw + 5 * scale : 0);
        if (cx + 3 * a - cnt_w - 2 * scale < left) left = cx + 3 * a - cnt_w - 2 * scale;
        if (cy + 3 * a + cnt_h > panel[1] && cy < panel[3] && cx + 3 * a > panel[0] && left < panel[2])
            cy = panel[1] - 3 * a - cnt_h - 2 * scale;
        if (cy < 0) cy = 0;
    }
    /* a light rim round the cross, so its shape reads over any sky or wall */
    const int sq[5][2] = {{1, 0}, {1, 2}, {0, 1}, {2, 1}, {1, 1}};
    for (int k = 0; k < 5; ++k)
        wpar_rect(fb, cx + sq[k][0] * a - scale, cy + sq[k][1] * a - scale, a + 2 * scale, a + 2 * scale, 0xC8C8C8u, 235);
    wpar_arm(fb, cx + a, cy, a, 0, h.up[0] != 0);
    wpar_arm(fb, cx + a, cy + 2 * a, a, 1, h.down[0] != 0);
    wpar_arm(fb, cx, cy + a, a, 2, false);
    wpar_arm(fb, cx + 2 * a, cy + a, a, 3, false);
    wpar_rect(fb, cx + a, cy + a, a, a, 0x101010u, 255);
    /* the words, to the left, level with their arm */
    const int ys[2] = {cy + a / 2 - row_h / 2, cy + 2 * a + a / 2 - row_h / 2};
    for (int k = 0; k < 2; ++k) {
        if (!lab[k][0]) continue;
        const int w = wpar_text_w(lab[k], rom, scale);
        const int tx = cx - 4 * scale - w;
        ovl_shade(fb, tx - 2 * scale, ys[k] - scale, w + 4 * scale, row_h);
        wpar_text(fb, tx, ys[k], lab[k], rom, scale, 0xFFFFFFFFu, 255);
    }
    if (cnt[0]) {
        const int tx = cx + 3 * a - cnt_w, ty = cy + 3 * a + 3 * scale;
        ovl_shade(fb, tx - 2 * scale, ty - scale, cnt_w + 4 * scale, row_h);
        wpar_text(fb, tx, ty, cnt, rom, scale, 0xFFFFFFFFu, 255);
    }
}

static void world_party_draw(const OvlSurface &fb)
{
    world_party_dpad_draw(fb);
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

    const int big = scale;   /* chat size: the toasts and prompts were twice that (Tango 10-15: too big) */
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
