// THE STAR POPUP PAINTER (tests/walk_window.cpp only; compiled after the chat
// painter and net_hud.h, whose glyph helpers it uses). Run mp1, lane STARPOP1.
// Draw-only: it reads hal/star_popup.h's view and writes overlay pixels.
#ifndef PORT_HAL_STAR_POPUP_PAINT_H
#define PORT_HAL_STAR_POPUP_PAINT_H

#include "star_popup.h"

#ifdef OVL_SCALE
static void star_popup_draw(const OvlSurface &fb)
{
    port::StarPopupView v;
    port::star_popup_view(&v);
    if (v.n <= 0) return;
    const int rom = chat_font_build();
    const int scale = OVL_SCALE;
    const int big = scale * 2;                      /* twice the chat size, centred */
    const int row_h = rom ? (NT_GH + 2) * big : OVL_LINE * 2;
    const int total_h = v.n * row_h;
    int y = ntr::active_h / 4 - total_h / 2;
    if (y < 4 * scale) y = 4 * scale;
    int maxw = 0;
    for (int i = 0; i < v.n; ++i) {
        int w = 0;
        for (const char *p = v.line[i]; *p; ++p) w += chat_adv((unsigned char)*p, rom, big);
        if (w > maxw) maxw = w;
    }
    const int pad = 6 * scale;
    int bx = (ntr::active_w - maxw) / 2 - pad;
    if (bx < 0) bx = 0;
    ovl_shade(fb, bx, y - pad / 2, maxw + 2 * pad, total_h + pad);
    for (int i = 0; i < v.n; ++i) {
        const char *s = v.line[i];
        int w = 0;
        for (const char *p = s; *p; ++p) w += chat_adv((unsigned char)*p, rom, big);
        const int x = (ntr::active_w - w) / 2;
        const int yy = y + i * row_h;
        const uint32_t col = (i == v.n - 1 && s[0] == 'E') ? 0xFFFFE060u : 0xFFFFFFFFu;
        if (rom) chat_text(fb, x, yy, s, (int)strlen(s), rom, big, 0xFFFFFFFFu, 0, 255);
        chat_text(fb, x, yy, s, (int)strlen(s), rom, big, col, 1, 255);
    }
}
#endif  // OVL_SCALE

#endif  // PORT_HAL_STAR_POPUP_PAINT_H
