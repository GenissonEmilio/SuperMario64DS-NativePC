// THE ONLINE PING READOUT. Run mp1, lane PINGHUD1.
//
// Two short lines at the bottom right of the game picture while a console is
// online, in the chat's font:
//
//   adventure room (hal/comms_hub.cpp)   "ping 48 ms"      "loss 0.3%"
//   VS, a joiner (hal/comms_loopback.cpp) "ping 94 ms"     "delay 5"
//   VS, the host                          "delay 5"        "worst 96 ms P3"
//
// WHAT EACH NUMBER IS, and every one is a number the carrier already keeps
// (this file measures nothing and sends nothing):
//
//   ping (room)   the mean of the newest round trip to every other player in
//                 the room: the hub's own SYNP / SYNQ pings, one asked seat
//                 every 500 ms (comms_hub_seat_info().rtt_ms; the "rtt=[..]"
//                 list of the hub's periodic line). "ping --" while nobody
//                 else is in the room or no pong has come back yet.
//   loss (room)   the share of those pings that got no pong, over the last
//                 ten seconds (comms_hub_stats() pings_sent / pongs_recvd,
//                 counted only while somebody was there to answer). A ping
//                 and its pong both cross the network, so this is round-trip
//                 loss: 20% loss each way reads about 36%.
//   ping (VS)     the joiner's round trip to the host as the carrier measured
//                 it when the match was joined (the smallest of five exact
//                 round trips; "hsrtt=" in the carrier's own line). The VS
//                 carrier does not measure it again during a match, so it
//                 stays put.
//   delay (VS)    the match's input delay in 30 Hz rounds ("indelay=").
//   worst (VS)    host only: the slowest joiner's round trip and its player
//                 number (the per-child list of the host's sizing line). A
//                 joiner never learns the other joiners' round trips.
//
// VS HAS NO LOSS LINE. Nothing in the VS carrier counts lost datagrams (it
// re-sends until a round lands and never learns which copies the network ate),
// and showing one of its other counters under that label would be a guess.
//
// WHEN IT SHOWS. A VS carrier with at least two live players, or the hub while
// it is pairing or live. Solo nothing here runs past two reads and nothing is
// drawn; once the match is down to one player, or the session closes, the
// readout goes. No key and no setting.
//
// IT IS DRAW-ONLY. The view reads public stats (no carrier pump, no socket,
// no game memory) and the painter writes only overlay pixels, after the game
// has finished the frame. The text refreshes twice a second, so it does not
// flicker with every frame.
//
// WHERE IT SITS. Right-aligned in the strip under the chat log (the chat never
// draws below 40 * scale rows from the bottom): against the left edge of the
// corner map panel when the panel is up, against the picture's right edge when
// it is not (the stacked layout, the panel switched off). Clear of the HUD's
// corner icon on the left, of the map panel and of the chat log above.
//
// The painter half compiles only inside tests/walk_window.cpp, after its chat
// painter (it draws through the chat's glyphs and the overlay's surface); the
// view half is hal/net_hud.cpp.

#ifndef PORT_HAL_NET_HUD_H
#define PORT_HAL_NET_HUD_H

namespace port {

enum : int { kNetHudLines = 2, kNetHudLineMax = 24 };

struct NetHudView {
    int  n;                                   // lines to draw, 0 = nothing
    char line[kNetHudLines][kNetHudLineMax];  // printable ASCII
};

// The current readout. Cheap enough to call every presented frame: the text
// is rebuilt at most every 500 ms and otherwise copied.
void net_hud_view(NetHudView *out);

}  // namespace port

#ifdef OVL_SCALE
/* ---- THE PAINTER (tests/walk_window.cpp only) ---------------------------- */

/* hal/sub_screen.cpp: the rectangle this frame's corner map panel was drawn
   in, framebuffer pixels (x0, y0, x1, y1); 0 when no panel was drawn. */
extern "C" int hal_sub_screen_inset_rect(int r[4]);

static void net_hud_draw(const OvlSurface &fb)
{
    port::NetHudView v;
    port::net_hud_view(&v);
    if (v.n <= 0) return;
    const int rom = chat_font_build();
    const int scale = OVL_SCALE;
    const int row_h = rom ? (NT_GH + 2) * scale : OVL_LINE;
    const int top = ntr::active_h - 2 * scale - v.n * row_h;
    int right = ntr::active_w - 4 * scale;
    int r[4];
    if (hal_sub_screen_inset_rect(r) && r[1] < ntr::active_h && r[3] > top &&
        r[0] - 4 * scale < right)
        right = r[0] - 4 * scale;
    for (int i = 0; i < v.n; ++i) {
        const char *s = v.line[i];
        int w = 0;
        for (const char *p = s; *p; ++p) w += chat_adv((unsigned char)*p, rom, scale);
        int x = right - w;
        if (x < 2 * scale) x = 2 * scale;
        const int y = top + i * row_h;
        ovl_shade(fb, x - 2 * scale, y - scale, w + 4 * scale, row_h);
        if (rom) chat_text(fb, x, y, s, (int)strlen(s), rom, scale, 0xFFFFFFFFu, 0, 255);
        chat_text(fb, x, y, s, (int)strlen(s), rom, scale, 0xFFFFFFFFu, 1, 255);
    }
}
#endif  // OVL_SCALE

#endif  // PORT_HAL_NET_HUD_H
