// STAR POPUP AND SHARED EXIT. Run mp1, lane STARPOP1.
//
// In a party or coop room, when a player collects a star or a Bowser key, every
// other console standing in the SAME level and act (star number) shows
//
//     <name> collected the star!        (or "... the key!")
//     Exiting in 5...  4...  3...  2...  1...
//
// and then leaves the level the way the collector does. A console in another
// level or act only gets a one-line toast in the chat (no popup, no exit).
// The collector plays the ROM's own star-get cutscene and exit untouched.
// VS (arena) and solo never reach any of this.
//
// WHAT COUNTS. The sender watches the actors, not the save: a PowerStar (class
// 178) whose take state (+0x440) is 5 with MY body as its toucher, or Chief
// Chilly's key (class 282) in state 3 with my body as its owner (+0x110). The
// ROM's own classifier func_ov002_020e73ac says what the star does to the
// player: 0 = normal star, 3 = Bowser key (idx 8), 1 = the special-level /
// star 0 flavour, 2 = silver / VS. Only 0 and 3 (and the 282 key) end in
// ExitLevel (Player func_ov002_020c94a4, sub 0 and sub 3); 1 and 2 keep
// playing, so for them nobody else exits either (toast only). A star that was
// already collected still exits the collector, so the watch does not look at
// the save bits.
//
// THE EXIT. The receiver mirrors func_ov002_020c94a4: the same per-level
// landing overrides (data_0209211c / data_0209f200: 0x21 for a star, 0x24 0x26
// 0x2d 0x2f 0x31 for a key), then the ROM's own ExitLevel(). So it lands where
// the collector lands. It waits for a safe frame (no pause, star select,
// message, level clear, cutscene, fade or level change pending) and never
// starts a second exit when the console is already leaving.
//
// WIRE. Coop: SYNB sub-type 0x1e (8 B payload, hal/star_popup.cpp). Party:
// SYNX with a non-zero `reserved` word (bit0 star, bit1 key, bit2 no exit):
// an event, not a star to save; PSTAR1's receiver hands those to this file.
// Each event goes out 3 times (same sequence) against packet loss.
//
// KNOBS (env). SM64DS_STAR_POPUP=0 is the must-fail control: nothing is sent,
// nothing received acts. SM64DS_STAR_POPUP_LOG=1 prints "[starpop]" lines.
// SM64DS_STAR_POPUP_FAKE=<kind>:<exits>@<frame>[;..] sends an event as if the
// local player had collected one (kind 1 star, 2 key; exits 0 or 1).
// SM64DS_STAR_POPUP_HOLD=<frame>:<n> forces "not safe" for n frames.
#ifndef PORT_HAL_STAR_POPUP_H
#define PORT_HAL_STAR_POPUP_H

namespace port {

enum : int { kStarPopupLines = 4, kStarPopupLineMax = 64 };

struct StarPopupView {
    int  n;                                          // lines to draw, 0 = nothing
    char line[kStarPopupLines][kStarPopupLineMax];   // printable ASCII
};
void star_popup_view(StarPopupView *out);

// PSTAR1's SYNX handler hands a datagram with a non-zero reserved word here.
// Returns true when it was an event (consumed, whatever came of it).
bool star_popup_party_rx(int seat, const unsigned char *d, int len);

struct StarPopupStats {
    unsigned long long tx, rx, shown, toast, exits, dropped;
};
StarPopupStats star_popup_stats();

}  // namespace port

// The one hook line (tests/walk_window.cpp): once per frame.
extern "C" void port_star_popup_frame(void);

#endif  // PORT_HAL_STAR_POPUP_H
