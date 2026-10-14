// COOP MINIMAP. Run mp1, lane SMALLS9 (item 5): the other players on the
// bottom-screen minimap in party, coop and world rooms.
//
// The ROM draws other players on the minimap only in VS (Minimap::Render's
// `data_0209f2d8 == 1` branch). In adventure a remote player is a hub ghost
// (hal/comms_sync.cpp): a body that is parked 200000 units up between draws, so
// the position Minimap::Behavior reads off the body is garbage. The port's two
// host copies of Behavior and Render (port/unmatched/Minimap_*.cpp) ask a hook,
// g_mm_ghost_pos (Minimap_wide.h), per slot; this file installs it. The answer
// is where the ghost follower drew the body this frame, and "not in this level"
// for a seat whose ghost is not drawn, so a friend in another course shows no
// icon. The icon is the VS small icon of the seat's colour row and character.
//
// SM64DS_COOP_MINIMAP=0 is the must-fail control (hook answers 0: no icons).
// SM64DS_COOP_MINIMAP_LOG=1 prints "[comap]" lines.
#ifndef PORT_HAL_COOP_MINIMAP_H
#define PORT_HAL_COOP_MINIMAP_H

extern "C" int port_coop_minimap_ghost(int slot, int pos[3]);

#endif
