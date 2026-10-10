// THE PLAYER LIST. Run mp1 wave 56, lane PLIST1.
//
// Top-left of the picture, UNDER the ROM's own HUD, in party and coop rooms
// only: one row per player, each row a head picture of the character, the
// in-game name and the short stage code of where that player is ("CGR" on
// the castle grounds, "BOB S1" in Bob-omb Battlefield star 1). Tango, 10-05:
// "add players names top left under the UI and also a head sprite of the
// character they are and also short form of stage they are in."
//
// SPLIT, as the ping readout (hal/net_hud.h) is split:
//   * this file + player_list.cpp   the model (rows, stage codes), the click
//                                   API. No game writes, no pixels.
//   * player_list_paint.h           the painter, compiled only inside
//                                   tests/walk_window.cpp (OVL_SCALE).
//   * head_icons.h / .cpp           the four head bitmaps.
//
// WHERE THE DATA COMES FROM (nothing is sent by this lane):
//   me        the game's own words (the same globals hub_presence samples)
//   others    hub_presence_get(seat) = the newest 'SYNL' (level, area, act,
//             character, in-level flag), and the newest 'SYNI' for the name
//             (comms_hub_latest, peeked, never taken).
//
// WHEN IT SHOWS. Only while a hub is live in a PARTY or COOP room
// (comms_hub_room_kind). Solo, VS (loopback / arena) and world rooms: the
// model answers n == 0 and the painter returns before it touches a pixel.
//
// THE CLICK API (coop uses it for "TP to friends"; party does not):
//     player_list_set_click_handler(fn)   fn(seat) runs on a click on a row.
//     player_list_click(fb_x, fb_y)       the mouse hook: true = CONSUMED.
//     player_list_hit_test(fb_x, fb_y)    the seat under that pixel or -1.
// A click on a row is consumed ONLY while a handler is set. With no handler
// (party) the click goes on to the game exactly as it did before. The list
// never draws over the corner map panel (rows that would overlap it are not
// shown and not clickable), so a consumed click can never be one the stylus
// would have taken: the stylus lives on the bottom screen / the panel.
//
// ROW EXTRA (coop marks rows without touching the painter):
//     player_list_set_row_extra(fn)       fn(seat) returns a short ASCII
//     badge (up to 4 characters, e.g. "TP") drawn at the end of that row, or
//     null / empty for none. Called once per frame per visible row.
//
// FRAMEBUFFER PIXELS everywhere: the same units as g_mouse_click_x / _y.

#ifndef PORT_HAL_PLAYER_LIST_H
#define PORT_HAL_PLAYER_LIST_H

namespace port {

enum : int {
    kPlistMaxRows = 4,       // party and coop cap: 4 players
    kPlistNameMax = 24,      // the SYNI name cap
    kPlistNameShown = 12,    // characters drawn
    kPlistCodeMax = 12,
    kPlistBadgeMax = 5,
};

struct PlayerListRow {
    int  seat;               // 0..3
    bool me;
    char name[kPlistNameMax + 1];   // printable ASCII, never empty
    int  character;          // 0 Mario, 1 Luigi, 2 Wario, 3 Yoshi
    unsigned body_rgb;       // 0xRRGGBB, the player's pick (palette default)
    bool in_level;           // false: menu / title / unknown (code is "--")
    bool same_place;         // same level AND area as me (dim when false)
    char code[kPlistCodeMax];       // "CGR", "BOB S1", "--"
    char badge[kPlistBadgeMax];     // coop's row extra, usually ""
};

struct PlayerListView {
    int n;                   // 0 = draw nothing
    PlayerListRow row[kPlistMaxRows];
};

// True while the list is on (a live party or coop hub).
bool player_list_active();

// The rows to draw this frame. Cheap; call once per presented frame.
void player_list_view(PlayerListView *out);

// "BOB S1" / "CGR" / "BFS S1" for a sublevel id (data_0209f2f8) and act
// (data_0209f220[0]); `out` must hold kPlistCodeMax. A course level carries
// " S<act>" when act is 1..8; castle areas never do. Unknown id: "---".
const char *player_list_stage_code(int level, int act, char *out);

// ---- click API ------------------------------------------------------------
typedef void (*PlayerListClickFn)(int seat);
void player_list_set_click_handler(PlayerListClickFn fn);   // null = none
bool player_list_has_click_handler();
int  player_list_hit_test(int fb_x, int fb_y);               // seat or -1
bool player_list_click(int fb_x, int fb_y);                  // true = consumed

typedef const char *(*PlayerListExtraFn)(int seat);
void player_list_set_row_extra(PlayerListExtraFn fn);        // null = none

// ---- the painter's side (player_list_paint.h) -----------------------------
void player_list_layout_begin();     // forget last frame's rectangles
void player_list_layout_row(int seat, int x, int y, int w, int h);
// Hover: the framebuffer pixel under the pointer (or -1,-1); the row it is on
// answers true from player_list_hovered(seat) while a handler is set.
void player_list_set_pointer(int fb_x, int fb_y);
bool player_list_hovered(int seat);
// Counters for the proofs ([plist] lines).
struct PlayerListStats {
    unsigned long long clicks, consumed, passed, handler_calls;
};
PlayerListStats player_list_stats();

}  // namespace port

#endif  // PORT_HAL_PLAYER_LIST_H
