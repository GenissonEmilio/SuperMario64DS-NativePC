// COOP CASTLE: castle Boos (209) and MIPS the rabbit (187), kept in step. Run mp1, lane COOPCASTLE1.
//
// The work is done by the phase-2 mover mechanism (hal/coop_actors.*): a profile ROW and a SIZE line per class in
// coop_actor_profiles.inc / coop_actor_sizes.inc. This file adds only what that mechanism does not own:
//   * a [coopcastle] log of the live Boos, MIPS and keys (the proof's view),
//   * proof knobs that drive the castle through the ROM's own paths.
//
//   Boo (209, daTrs_c): owner by proximity publishes position and yaw; a Boo killed on one console is gone on the others through
//     the death table (phase 1: the ROM's KillAndTrackInDeathTable). Whether a Boo hides when looked at keys on the LOCAL camera and
//     stays per console (it is a draw/chase decision of the Boo's own Behavior; the pose nudge never touches it).
//   MIPS (187, daMip_c): owner by proximity publishes the pose, so the rabbit that flees the player nearest to it flees that player
//     on every console. Being caught is the ROM's own flow on the console of the catcher; the star it gives is a save bit (phase 1
//     merges the save record), so a second catch on another console finds the star already taken.
//
// INERT unless SM64DS_COOP is set AND the group is established. Knobs (all optional):
//   SM64DS_COOP_CASTLE_LOG=1       [coopcastle] lines, one per second
//   SM64DS_CASTLE_HUNT=1           set the save bit that wakes the rabbits (data_0209caa0[2] & 0x20000), as the LakituBro intro does
//   SM64DS_CASTLE_CHASE=F:N[:cls[:d]]  from group frame F for N frames keep the local player d (150) units from the nearest MIPS (cls 187) or Boo (209)
//   (catching uses the existing SM64DS_RABBIT_TRIGGER=<frame> stand-in for the grab cylinder, hal/input_probe.cpp)

#ifndef PORT_HAL_COOP_CASTLE_H
#define PORT_HAL_COOP_CASTLE_H

namespace port {

struct CoopCastleStats {
    unsigned long long frames, chase_moves, hunt_sets;
    unsigned long long max_boo, max_mip, key_spawns;
};
CoopCastleStats coop_castle_stats();

}  // namespace port

#endif  // PORT_HAL_COOP_CASTLE_H
