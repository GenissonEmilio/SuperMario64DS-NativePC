// HOST EVENTS, the game's side. Run hunt8, lane EVENTS1. PROTOTYPE: its own
// branch, not part of 0.9.3.
//
// Tango hosts an event in the always-online world: a message on every
// player's screen, a song for everyone, or a timed modifier (triple speed, one
// hit mode, chaos mode, a race to 50 stars). Adventure online is every
// console's OWN game, so each console applies the event to itself; the relay
// only carries the event and its clock (tools/relay/relay.py, 'SYNZ').
//
// WHAT COMES IN. One kind, 'SYNZ' (hal/comms_hub.h kHubEvent), 96 bytes, taken
// ONLY from the relay itself (byte 5 == 0xFD) in a world room. A player cannot
// make one: the relay's kind table has no row for it, and the hub takes it
// from nobody else. Layout (little endian):
//   0 tag | 4 version 1 | 5 0xFD | 6 type | 7 0
//   8 event id u32 | 12 remaining ms u32 | 16 total ms u32
//   20 arg u16 | 22 text_len u8 | 23 0 | 24 text (72 bytes)
//   type 1 message, 2 music (arg = a sublevel whose course song plays),
//   3 modifier (arg 1 triple speed, 2 one hit, 3 chaos, 4 race, 5 castle
//   unlocked), 4 clear, 5 race leader (arg = its stars, text = its name),
//   6 race winner (arg = its stars, text = its name; byte 7 = the target).
//   Byte 7 of a race modifier is its target in stars (0 = 50).
// The relay repeats every live event once a second with its REMAINING time, so
// a lost datagram costs nothing and a late joiner picks up what still runs.
//
// WHAT EACH ONE DOES TO THIS CONSOLE (nothing here writes a save):
//   message  the text, big, at the top of the picture for its length
//   music    Sound layer 3 (the same override the boss songs use) plays the
//            sublevel's course song; stopping it hands the level song back
//   triple   the per-character speed factor (data_ov002_020ff170, what every
//            Player_ScaleByCharFactor speed limit multiplies by) is tripled
//            for the local character and put back when the clock runs out
//   one hit  any drop in health kills (Player::Heal of everything that is left)
//   chaos    Mario stands still for 2 s (120 game ticks) and explodes: the
//            bob-omb's own particles and sound, a coin out of his head, then a
//            damage the health meter cannot survive (the normal death, which
//            ends in the Bowser iris-out)
//   castle   every star door, key door and star gate behaves as if the player
//            held every star and both keys; nothing is written to the save and
//            the doors are the save's own again the moment the event ends
//   race     the RELAY tallies the star totals every console already sends
//            (presence) and says who leads (every few seconds) and who won
//            (once). A console shows "<name> WINS THE RACE" from that word
//            only; it never decides from its own count
//
// SAFETY. Off entirely with SM64DS_EVENTS=0 (the kill switch) or the player's
// own "Online events" switch in the GENERAL settings tab (default on). Nothing happens
// outside a live world hub: arena VS, party, coop and solo never take a 'SYNZ'
// (the hub's room gate). The banner always says what is running and for how
// long; leaving the world (close the game) ends every effect, and none of it
// is written to disk.
//
// KNOBS:
//   SM64DS_EVENTS=0            the kill switch (also in the kit)
//   SM64DS_EVENT_LOG=1         "[event]" lines: starts, ends, every 30th tick
//                              of the local body (pos, horizontal speed, hp)
//   The proof-only knobs SM64DS_EVENT_FAKE and SM64DS_EVENT_TESTHURT exist in
//   a dev build only and are compiled out of the kit (PORT_ROM_CLEAN).
#ifndef PORT_HAL_EVENT_CLIENT_H
#define PORT_HAL_EVENT_CLIENT_H

namespace port {

enum : int {
    kEventLines = 6,
    kEventLineMax = 80,
};

// What the painter shows this frame. n == 0: nothing.
struct EventView {
    int  n;
    char line[kEventLines][kEventLineMax];
    int  style[kEventLines];     // 0 headline, 1 modifier, 2 note
};
void event_view(EventView *out);

// One line for the pause menu: the live event and the time left ("Triple speed
// 0:42"). False (and out untouched) when nothing runs or events are off.
bool event_pause_line(char *out, int cap);

struct EventStats {
    unsigned long long rx, applied, ended, dropped_bad, explosions, kills;
};
EventStats event_stats();

// Pure helpers (the unit tests call these).
// Decodes a 96-byte 'SYNZ'. False when it is not a well formed event.
struct EventWire {
    unsigned id, remain_ms, total_ms;
    int      type, arg;
    int      extra;          // byte 7: the race target (0 = 50)
    char     text[73];
};
bool event_decode(const unsigned char *d, int len, EventWire *out);

}  // namespace port

// The one hook line (tests/walk_window.cpp): once per frame, after the hub's
// own frame hook (port_hub_presence_frame) has pumped the queues.
extern "C" void port_event_frame(void);

// 1 while a castle-unlocked event is live on this console (hal/event_client.cpp
// ; the door hooks read it).
extern "C" int port_event_castle_live(void);

#endif  // PORT_HAL_EVENT_CLIENT_H
