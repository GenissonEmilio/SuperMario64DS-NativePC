// ARENA MODE, the Wario hold: catching a stunned player, carrying, swinging and
// throwing him. Run mp1, lane GRAB1 (wave 55, field finding F2). Read
// hal/vs_arena_grab.cpp first for how the two consoles share it.
//
// What the ROM does (src/func_ov002_020d869c.cpp, func_ov002_020d85fc.cpp,
// func_ov002_020db5f4.cpp, _ZN6Player7TryGrabER8dActor_c.cpp): a Wario hit
// (Player::Hurt called with the attacker's param1 2) leaves the victim in a 90
// frame stun (St_Hurt, phase 2: func_ov002_020d98b4 sets the grabbable actor flag
// 0x80 every frame). A Wario in a punch or a dive whose attack cylinder (hit flag
// 0x1000) touches such a player calls Player::TryGrab and func_ov002_020db5f4:
// the victim goes into St_Grabbed (+0x35c = the holder), the holder into
// St_HoldLight; the holder's own states then carry (St_HoldLight), spin
// (St_SwingPlayer) and throw (St_Throw -> func_ov002_020daa74 ->
// func_ov002_020db54c) and the victim's own St_Grabbed_Main counts the buttons it
// mashes (func_ov002_020beb38) and breaks free by itself.
//
// In an arena the other seats are puppets whose Behavior never runs, so a remote
// Wario could never grab me and my own Wario's grab contact was dropped. This
// file turns the contact into a CATCH claim and the referee's GRABBED / THROWN
// events into the ROM's own calls on each console (wire: arena_wire.h).
//
// PORT_HOST_ABI: host plumbing; nothing in src/ knows this file exists. Inert
// unless arena mode is installed AND the hub is in relay mode (a referee).
//
// WHO CALLS WHAT (the hooks, each one line, in other lanes' files):
//   vs_arena.cpp  vs_arena_before_walk   vs_grab_before_walk(frame)
//   vs_arena.cpp  vs_arena_tick          vs_grab_after_walk(frame)
//   vs_arena_ref.cpp  hit_detect         if (vs_grab_contact(k, flags)) continue;
//   vs_arena_ref.cpp  apply_event        vs_grab_on_event(&e, g.seat, directed);
//   (plus ref_event_valid for the two new events, and finish_claim for them)

#ifndef PORT_HAL_VS_ARENA_GRAB_H
#define PORT_HAL_VS_ARENA_GRAB_H

#include "../tools/relay/arena_wire.h"

namespace port {

// Once a frame around the actor walk (vs_arena.cpp).
void vs_grab_before_walk(int frame);
void vs_grab_after_walk(int frame);

// hit_detect (vs_arena_ref.cpp) read a contact of MY attack on the puppet of
// seat `victim` with these hit flags (already masked with the attack mask),
// after it wiped the puppet's words. True = the contact is a grab attempt and
// is not a hit (no HIT claim for it); false = hit_detect goes on.
bool vs_grab_contact(int victim, unsigned flags);

// Every applied referee event, in order (after vs_arena_ref.cpp's own effect).
void vs_grab_on_event(const arena_wire::RefEvent *ev, int my_seat, bool directed);

struct VsGrabStats {
    bool wrapped;
    unsigned long long contacts, tries, claims, confirmed, denied, unwound,
                       thrown_sent, v_grabbed, v_released, v_breaks, h_dropped,
                       bystander_links;
};
VsGrabStats vs_grab_stats();

}  // namespace port

#endif  // PORT_HAL_VS_ARENA_GRAB_H
