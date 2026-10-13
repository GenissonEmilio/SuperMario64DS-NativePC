// PARTIES INSIDE THE WORLD. Run mp1, lane PINVITE1 (Tango 10-12: "invite players
// needs to be EZ like you walk near someone and press UP on dpad to send and they
// press up to accept. Then down on dpad it will ask if you want to leave party
// then down again to confirm").
//
// In "Play adventure online" (the public world hub) a party is made and left
// WITHOUT leaving the world: no new room, no relaunch, no launcher or lobby
// involved. Party members are solid bodies for each other (hal/party.h's
// run-time membership), everyone else stays an adventure ghost, and the members
// get a small list top-left.
//
// HOW IT TRAVELS. Nothing new on the wire. Every console already sends its
// presence ('SYNL', 20 B, 2 Hz and on change) in a world room, and its
// level_clock word is unused outside coop. This module publishes a PARTY WORD
// there and reads everybody else's with hub_presence_get (a peek, nothing is
// consumed). The state is symmetric and held by each console:
//
//   word  = 0xA7 << 24 | nonce << 13 | (inv_to + 1) << 8 | party_id
//   party_id   0 = not in a party, else a random 1..255 the founder picked
//   inv_to     the seat I am inviting right now (0 none, else seat + 1)
//   nonce      4 bits, changes with every invite so a repeat is a new event
//
//   * INVITE   press UP (d-pad, or the key below) within INVITE_RANGE of someone in
//              the same level and area: I take a party id (if I had none), set
//              inv_to to the nearest such player, bump the nonce, and keep it
//              INVITE_MS. Nobody answering = it just ends, quietly.
//   * ACCEPT   the invited console sees a word with inv_to == my seat + 1 and a
//              party id, shows "<name> invites you: UP to join" for INVITE_MS and,
//              on UP, adopts that party id. The inviter sees the same id on the
//              friend's word: "<name> joined the party".
//   * MEMBERS  seats whose party id equals mine (present, at most four). Each
//              change is pushed into party::set_member, which makes those bodies
//              solid. A console that is alone in its id drops back to id 0.
//   * LEAVE    press DOWN: "Leave party? DOWN again" for CONFIRM_MS; DOWN again =
//              party id 0, "You left the party". Anything else or a timeout cancels.
//
// KEYS. Pad: d-pad UP = invite / accept, d-pad DOWN = leave. Keyboard: B = invite /
// accept, N = leave (B was the old voice mic key; voice is shelved, if it returns
// it must take another key). Overrides for the proofs / a later settings page:
// SM64DS_PARTY_KEY_INVITE / SM64DS_PARTY_KEY_LEAVE = a Windows virtual-key code
// (decimal or 0x..), 0 = none; SM64DS_WORLD_PARTY=0 turns the whole thing off.
//
// ONLY IN A WORLD ROOM (comms_hub_room_kind() == world, a live hub, not an
// arena). Party, coop, VS and solo never read or send anything: the word is not
// published (level_clock stays what it was) and the tick returns on its first line.
//
// TRUST. Like every presence field it is the sender's own word. A modified
// client can claim any id; the cost is a solid body on a friend's console, never
// a save write or a warp. Party STARS in a world room need the relay to forward
// 'SYNX' to world sessions (see out\PINVITE1\DEPLOY.md); the bodies, the list and
// the invite do not.

#ifndef PORT_HAL_WORLD_PARTY_H
#define PORT_HAL_WORLD_PARTY_H

#include <cstdint>

namespace port {

enum : int {
    kWparMaxMembers = 4,           // a party holds at most four players
    kWparNameMax = 24,
    kWparCodeMax = 12,
    kWparLineMax = 64,
    kWparInviteMs = 8000,          // an invite stays out this long
    kWparConfirmMs = 4000,         // "Leave party?" waits this long for the second DOWN
    kWparCooldownMs = 3000,        // after an invite ends, the next one waits
    kWparToastMs = 3500,
    kWparRange = 400,              // world units, "about four body widths"
    kWparRangeY = 300,             // and not more than this above / below
};

constexpr uint32_t kWparMagic = 0xA7u;

// ---- the party word -------------------------------------------------------
inline uint32_t wpar_word(int party_id, int inv_to_seat, int nonce) {
    const uint32_t inv = inv_to_seat >= 0 && inv_to_seat < 16
                             ? (uint32_t)(inv_to_seat + 1) : 0u;
    return (kWparMagic << 24) | ((uint32_t)(nonce & 15) << 13) | (inv << 8) |
           (uint32_t)(party_id & 255);
}
struct WparWord {
    bool ok;                       // false: not a party word (magic differs)
    int party_id;                  // 0 none
    int inv_to;                    // seat, -1 none
    int nonce;
};
inline WparWord wpar_decode(uint32_t w) {
    WparWord d = {false, 0, -1, 0};
    if ((w >> 24) != kWparMagic) return d;
    if (w & 0x00FE0000u) return d;               // unused bits must be zero
    const int inv = (int)((w >> 8) & 31u);
    if (inv > 16) return d;
    d.ok = true;
    d.party_id = (int)(w & 255u);
    d.inv_to = inv ? inv - 1 : -1;
    d.nonce = (int)((w >> 13) & 15u);
    return d;
}

struct WparRow {
    int  seat;
    char name[kWparNameMax + 1];
    int  character;
    unsigned body_rgb;
    bool same_place;
    char code[kWparCodeMax];
};
struct WparView {
    int  n;                        // member rows, me first (0 = draw no list)
    WparRow row[kWparMaxMembers];
    char prompt[kWparLineMax];     // "" or the line that needs a key
    char toast[kWparLineMax];      // "" or a short message
};

// True while the feature is on (a world room with a live hub).
bool world_party_active();

// Once a frame, from the input hook: this frame's pad word (XInput buttons, 0 when
// the pad is not live), the invite key and the leave key as the game's key reader
// sees them. Also the module's clock (reads the others' words, publishes mine).
void world_party_input(unsigned pad_buttons, bool key_invite, bool key_leave);
int  world_party_vk_invite();
int  world_party_vk_leave();

// The picture. Cheap; once per presented frame.
void world_party_view(WparView *out);

// The word hub_presence.cpp puts in my SYNL (0 outside a world room).
uint32_t world_party_word();

// Am I in a party / is that seat in mine.
bool world_party_in_party();
bool world_party_member(int seat);

// Test seam (the unit program): feed the others' words directly.
void world_party_test_reset();

struct WparStats {
    unsigned long long invites, accepts, leaves, joined, left_by_other;
};
WparStats world_party_stats();

}  // namespace port

#endif  // PORT_HAL_WORLD_PARTY_H
