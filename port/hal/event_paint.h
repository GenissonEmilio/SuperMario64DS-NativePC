// THE HOST EVENT PAINTER (tests/walk_window.cpp only; compiled after the chat
// painter and net_hud.h, whose glyph helpers it uses). Run hunt8, lane EVENTS1.
// Draw-only: it reads hal/event_client.h's view and writes overlay pixels.
// Headlines (a message, an event's announcement, a race win) are drawn big and
// centred near the top; the running modifiers sit under them, small, with the
// time left, so a player can always see what is running and for how long.
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
    int y = 10 * scale;
    for (int i = 0; i < v.n; ++i) {
        const int big = v.style[i] == 0 ? scale * 2 : scale;
        const int row_h = rom ? (NT_GH + 2) * big : OVL_LINE * (v.style[i] == 0 ? 2 : 1);
        const char *s = v.line[i];
        int w = 0;
        for (const char *p = s; *p; ++p) w += chat_adv((unsigned char)*p, rom, big);
        const int pad = 4 * scale;
        int x = (ntr::active_w - w) / 2;
        if (x < pad) x = pad;
        ovl_shade(fb, x - pad, y - pad / 2, w + 2 * pad, row_h + pad);
        const uint32_t col = v.style[i] == 0 ? 0xFFFFFFFFu : (v.style[i] == 1 ? 0xFFFFE060u : 0xFF80D0FFu);
        if (rom) chat_text(fb, x, y, s, (int)strlen(s), rom, big, 0xFFFFFFFFu, 0, 255);
        chat_text(fb, x, y, s, (int)strlen(s), rom, big, col, 1, 255);
        y += row_h + pad;
    }
}
#endif  // OVL_SCALE

#endif  // PORT_HAL_EVENT_PAINT_H
