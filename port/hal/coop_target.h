// COOP SHARED TARGETING: "the closest player", the same answer on every console. Run mp1, lane BOSSCORE1.
//
// WHAT THE ROM DOES. Every enemy and boss that decides something about "the player" asks
// dActor_c::ClosestPlayer (src/_ZN8dActor_c13ClosestPlayerEv.cpp): it walks the player table
// (data_0209f394) once per actor per frame and caches the nearest body in data_0209b458. HorzAngleToCPlayer,
// DistToCPlayer, IsPlayerInRange, IsTooFarAwayFromPlayer and 168 sources call it. A friend's body is PARKED
// 200000 units above the local player between draws (PBODY1 / comms_sync.cpp gw_park_body), so today every
// console's enemies see only their own player.
//
// WHAT THIS FILE DOES (all coop, inert elsewhere; nothing in src/ changes):
//   1. Replaces the entry of ClosestPlayer at run time (a 5-byte jump, installed once when a coop room goes
//      live, never in a solo / world / party / VS run). The replacement keeps the ROM's loop and cache exactly,
//      with two differences: a friend who is standing in my level and area is put at the place the follower drew
//      him (port_adventure_ghost_drawpos) before the loop looks, and an actor that has a PUBLISHED TARGET SEAT
//      gets that seat's body instead of the nearest.
//   2. The group owner (coop_group_owner) is the one decider. At 10 Hz it measures every placed actor near a
//      player against every player it can place (itself exactly, friends at their drawn poses) and keeps, per
//      actor slot, a target seat. A closer player does not take over until he has been closer for the hold time
//      (1 s default), so the choice does not snap back and forth; a target that leaves the group or the level
//      is dropped at once with no hold. Changes (and a 1.5 s heartbeat) go out as one SYNW datagram.
//   3. Everyone else (and the owner itself) uses the table: the seat an actor is told to look at is the seat its
//      ClosestPlayer returns on every console. An actor with no fresh entry (a runtime spawn with no slot, a
//      lost datagram) falls back to the nearest placed player here, no hold.
//   4. Whomp / Whomp King never ask ClosestPlayer: Whomp::InitResources copies the player table into the actor (+0x39c) and
//      func_ov079_02123f34 picks the nearest by horizontal distance. That function is replaced the same way, with the table's
//      seat winning. Other classes that read the player table themselves are listed in the lane's BREAKS.md.
//   Boss authority (coop_boss.*) is "the console of the boss's target seat".
//
// WIRE. SYNW 0x50 TARGET_SET, payload after the 12-byte head: TargetHead (8 B) + count * TargetRec (4 B).
//   SYNW 0x50-0x5f are this lane's (PLAN 7.0); 0x51-0x53 belong to coop_boss.* (it is dispatched from here: the
//   hub gives the world range out four times only).
//   Only the group owner's TARGET_SET is accepted. Everything is range-checked; a short / long / forged datagram
//   is dropped and counted.
//
// ENVIRONMENT (proof knobs, all inert when unset):
//   SM64DS_COOP_TARGET=0         MUST-FAIL control: no replacement, no table (each console picks its own player)
//   SM64DS_COOP_TARGET_HOLD_MS=N the switch hold (default 1000)
//   SM64DS_COOP_TARGET_LOG=1     [cotgt] lines: every decider switch, every change of the seat a console's
//                                ClosestPlayer returns for a logged class, the stats line
//   SM64DS_COOP_TARGET_CLS=a,b   classes whose "use" lines are logged (default 165,189 and the Goomba 200)
//   SM64DS_COOP_TARGET_STAND=F:cls:d[:every]  at tick F put the local player d units beside the first actor of class cls (proof only)
//   SM64DS_COOP_TARGET_SAMPLE=cls  [cts] lines: every 500 ms the pose and table seat of every actor of that class (proof only)
//   SM64DS_COOP_TARGET_FORGE=F:name  at tick F one forged datagram: short long count seat slot key flags nonowner

#ifndef PORT_HAL_COOP_TARGET_H
#define PORT_HAL_COOP_TARGET_H

#include "coop_wire.h"

#include <cstdint>

namespace port {

enum : uint8_t {
    kWorldTargetSet  = 0x50,
    kWorldBossState  = 0x51,   // coop_boss.*
    kWorldBossHit    = 0x52,   // coop_boss.*
    kWorldBossDefeat = 0x53,   // coop_boss.*
    kWorldBossLast   = 0x5f,
};

enum : int {
    kTargetMaxRecs   = 60,
    kTargetSlots     = 512,
    kTargetNoSeat    = 0xff,
};

#pragma pack(push, 1)
struct TargetHead {          // 8 B
    CoopKeyWire key;
    uint8_t count;           // 1..kTargetMaxRecs
    uint8_t flags;           // 0
    uint16_t reserved;       // 0
};
struct TargetRec {           // 4 B
    uint16_t slot;           // death-table slot 0..511
    uint8_t seat;            // 0..3, or 0xff = no target (the entry is cleared)
    uint8_t reserved;        // 0
};
#pragma pack(pop)
static_assert(sizeof(TargetHead) == 8, "target head is 8 bytes");
static_assert(sizeof(TargetRec) == 4, "target record is 4 bytes");

struct CoopTargetStats {
    unsigned long long ticks, decisions, switches, holds, drops_at_once, sent, recs_sent, rx, recs_rx, applied;
    unsigned long long cp_calls, cp_used_table, cp_natural, cp_unavailable, patched;
    unsigned long long drop_bad, drop_group, drop_notowner, drop_member, drop_off, drop_seat;
};
CoopTargetStats coop_target_stats();

// Validator (payload = after the 12-byte head). Exposed for the forged-input rows.
bool coop_target_validate(const void *payload, int len);

// ---- API for other lanes (COOPSEE1, coop_boss, the per-boss lanes) ------------------------------------------
// The seat an actor in placed slot `slot` is told to look at, or -1 when there is no fresh entry.
int coop_target_seat_of_slot(int slot);
// Same for an actor pointer (reads its death-table slot).
int coop_target_seat_of_actor(const void *actor);
// The body of a seat, placed where that player is drawn (a friend) or the local body. Null when that player is not
// in my level and area, or the seat is not in my group.
void *coop_target_body_for_seat(int seat);
// The ClosestPlayer answer for this actor, as the replacement would give it right now (does not touch the cache).
void *coop_target_player_for(const void *actor);
// The seat of a body returned by ClosestPlayer (my seat for the local body), -1 for an unknown pointer.
int coop_target_seat_of_body(const void *body);

}  // namespace port

#endif  // PORT_HAL_COOP_TARGET_H
