// THE HOST EVENT PAINTER (tests/walk_window.cpp only; compiled after the chat
// painter and net_hud.h, whose glyph helpers it uses). Run hunt8, lane EVENTS1.
// Draw-only: it reads hal/event_client.h's view and writes overlay pixels.
// Headlines (a message, an event's announcement, a race win) are drawn big and
// centred near the top, wrapped at spaces so a 70 character message stays on
// the picture; the running modifiers sit under them, small, with the time
// left, so a player can always see what is running and for how long.
#ifndef PORT_HAL_EVENT_PAINT_H
#define PORT_HAL_EVENT_PAINT_H

#include "event_client.h"

#ifdef OVL_SCALE
static void event_draw(const OvlSurface &fb)
{
    port::EventView v;
    port::event_view(&v);
    if (v.n <= 0) return;
    const int rom = chat_font_build();
    const int scale = OVL_SCALE;
    const int pad = 4 * scale;
    const int max_w = ntr::active_w - 2 * pad;
    int y = 10 * scale;
    for (int i = 0; i < v.n; ++i) {
        const int big = v.style[i] == 0 ? scale * 2 : scale;
        const int row_h = rom ? (NT_GH + 2) * big : OVL_LINE * (v.style[i] == 0 ? 2 : 1);
        const uint32_t col = v.style[i] == 0 ? 0xFFFFFFFFu : (v.style[i] == 1 ? 0xFFFFE060u : 0xFF80D0FFu);
        const char *s = v.line[i];
        // greedy word wrap: each row is the longest run of words that fits
        const char *p = s;
        int rows = 0;
        while (*p && rows < 4) {
            const char *end = p;          // the end of the run that fits so far
            const char *q = p;
            int w = 0;
            while (*q) {
                const char *word_end = q;
                while (*word_end && *word_end != ' ') ++word_end;
                int ww = 0;
                for (const char *c = q; c < word_end; ++c) ww += chat_adv((unsigned char)*c, rom, big);
                const int sp = (q == p) ? 0 : chat_adv((unsigned char)' ', rom, big);
                if (q != p && w + sp + ww > max_w) break;
                w += sp + ww;
                end = word_end;
                q = word_end;
                while (*q == ' ') ++q;
            }
            if (end == p) {               // one word wider than the picture: cut it
                end = p + 1;
                while (*end && end - p < 40) ++end;
            }
            const int n = (int)(end - p);
            int rw = 0;
            for (int k = 0; k < n; ++k) rw += chat_adv((unsigned char)p[k], rom, big);
            int x = (ntr::active_w - rw) / 2;
            if (x < pad) x = pad;
            ovl_shade(fb, x - pad, y - pad / 2, rw + 2 * pad, row_h + pad);
            if (rom) chat_text(fb, x, y, p, n, rom, big, 0xFFFFFFFFu, 0, 255);
            chat_text(fb, x, y, p, n, rom, big, col, 1, 255);
            y += row_h + pad;
            ++rows;
            p = end;
            while (*p == ' ') ++p;
        }
    }
}
#endif  // OVL_SCALE

#endif  // PORT_HAL_EVENT_PAINT_H
