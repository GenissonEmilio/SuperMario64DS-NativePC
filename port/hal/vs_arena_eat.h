// ARENA MODE, eating. Run mp1, lane EAT1 (plan A5 of runs/mp1/out/VSSYNC1/plan.txt).
//
// A Yoshi tongue that touches another PLAYER is the hardest interaction in a
// VS arena: two state machines on two consoles (VSSYNC1 plan 4.5, risk R3).
//
// PART 1 (this file's first commit): in arena mode the tongue never eats a
// player. The Player's OnYoshiTryEat (vtable slot 18) is the ROM's own yes / no
// the tongue asks of whatever it touched (func_ov002_020d6998 calls it on the
// touched actor); answering 0 is the ROM's refusal path, the one an uneatable
// object takes: Yoshi flinches back (func_ov002_020d71ec step 2) and nothing is
// eaten. Outside arena mode the slot is the ROM's own, untouched.
//
// WIRING: one call, from hal/vs_arena.cpp vs_arena_before_walk (after the
// puppet hold): port::vs_eat_before_walk(frame). The first call wraps the slot.
//
// Lab knobs (off by default): SM64DS_EAT_RIG=tongue seats a scripted tongue
// (each of seats 0 and 1 stands 40 units behind the other puppet and uses the tongue every 4 s);
// SM64DS_EAT_LOG=1 one line per tongue question.
//
// PORT_HOST_ABI: host plumbing; nothing in src/ knows this file exists.

#ifndef PORT_HAL_VS_ARENA_EAT_H
#define PORT_HAL_VS_ARENA_EAT_H

namespace port {

// Once per arena frame, before the actor walk, after the puppet hold. No-op
// outside arena mode.
void vs_eat_before_walk(int frame);

struct VsEatStats {
    bool wrapped;                      // slot 18 is ours
    unsigned long long asked;          // tongue questions in arena mode
    unsigned long long refused;        // answered 0 (never eaten)
    unsigned long long rig_tongues;    // scripted tongues started
};
VsEatStats vs_eat_stats();

}  // namespace port

#endif  // PORT_HAL_VS_ARENA_EAT_H
