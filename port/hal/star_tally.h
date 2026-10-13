// STAR TALLY. Run mp1, lane SMALLS9 (item 16).
//
// In a coop room the save is one shared file, so the star total the ghost tag
// used to print (the newest SYNG `stars` field = NumStars()) was the group
// total on every head. Tango: the number over a head is THAT player's own star
// count this session; the total stays top right.
//
// Each console counts the stars ITS OWN body takes (a PowerStar, class 178, in
// take state 5 with my body as its toucher, classified 0 = a normal star by the
// ROM's func_ov002_020e73ac: the same sight hal/star_popup.cpp uses) and sends
// that count in the SYNG `stars` field it already sends (comms_sync.cpp). A
// ghost tag reads it back unchanged, so nothing on the receiving side moved.
//
// THE HOST KEEPS IT. A console that restarts, or joins late, would otherwise
// start at 0. Coop SYNB sub-type 0x22 (TALLY, in the coop save's 0x20-0x2f
// range, handled by hal/coop_save.cpp): every guest tells the host
// "<key> <count>" when its count changes (and every 3 s), the host keeps a small
// table (max per key) and sends the whole table to everyone when it changes (and
// every 3 s). A console takes max(its own, table[its key]) so a return restores
// the count. The key is the FNV-1a hash of the player's launcher name
// (SM64DS_PLAYER_NAME); a console with no name is not restored (count only).
//
//   payload: u8 from_host (1 = host table, 0 = a guest's own entry), u8 n (<= 8),
//            u16 0, then n x { u32 key, u16 count, u16 0 }.
//
// KNOBS. SM64DS_STAR_TALLY=0: the must-fail control (the old total on every
// head). SM64DS_STAR_TALLY_LOG=1: "[startally]" lines.
// SM64DS_STAR_TALLY_FAKE=<n>@<call>[;..]: add n stars to my own count at that
// call of the wire value (about one per rendered frame), as if my body had
// taken them (rows without a real star course).
#ifndef PORT_HAL_STAR_TALLY_H
#define PORT_HAL_STAR_TALLY_H

#include <stdint.h>

namespace port {

enum : int { kStarTallySub = 0x22, kStarTallyMax = 8, kStarTallyPayloadMax = 4 + 8 * 8 };

// The value for the SYNG `stars` field: my own tally in a coop room, `total`
// (the save's NumStars) everywhere else. Also runs the star watch once per call.
int star_tally_wire_value(int total);

// coop_save.cpp: the sub-type 0x22 payload to send now (returns its length, 0 =
// nothing to send). `host` = this console is the room host.
int star_tally_take_outgoing(bool host, uint8_t *out /* kStarTallyPayloadMax */);
// A received 0x22 payload. Returns false when malformed.
bool star_tally_on_payload(bool host, int seat, const uint8_t *pl, int len);
// A new peer appeared: send the table / my entry again soon.
void star_tally_poke();

int star_tally_mine();

}  // namespace port

#endif
