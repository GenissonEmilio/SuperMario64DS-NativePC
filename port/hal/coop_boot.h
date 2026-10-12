// COOP BOOT: everyone boots into the host's file together, and a new file's
// opening plays for everyone. Run mp1 wave 56, lane COOPBOOT1. Wire:
// hal/coop_boot_wire.h. Inert unless SM64DS_COOP=1 (a coop room member).
//
// WHAT IT DOES
//   1. ENV CONTRACT. In a coop room the process boots straight to the file:
//      SM64DS_SKIP_MENU=1 and SM64DS_BOOT_FILE=0 are forced, whatever else is
//      set. No main menu, no file select shown by hand.
//   2. GUEST GATE. A guest first hears the host (its STATE beat), waits while
//      the host's opening is running (a joiner must not start its own
//      intro), then fetches the host's save (coop_save_fetch). No host within
//      3 s, or no save answer within 3 s: plain line on stderr, exit code
//      kCoopExitNoHost (6). The host waits a short while at its own gate
//      (until every present guest has asked for the save, at most 3 s) so
//      those requests are answered before its slow ROM load.
//   3. START BARRIER. Every console holds the title at the file press (no
//      input, no game tick) and says READY; the host sends GO when all
//      present members are READY, or 8 s after its own READY. A console that
//      arrives after GO goes straight in.
//   4. OPENING TOGETHER (new file). Each console runs the opening locally.
//      At the opening's closing level load each says ARRIVED and holds (no
//      game tick) until the host sends CLOSE (everyone arrived, or 5 s after
//      the first arrival). Then everyone starts the grounds together.
//      An existing file never takes this.
//   5. HOST LEFT. The host process says HOST_LEFT as it exits; a guest also
//      treats the host's seat missing from the hub as the host gone. The
//      guest shows "The host left. Room closed." for 3 s and exits with
//      kCoopExitHostLeft (7). The host's save is already on its disk.
//
// EXIT CODES the launcher can map (6 and 7 are new; 3 and 4 are the hub's):
//   3 session mismatch   4 room full   6 could not reach the host
//   7 the host left, room closed       8 coop save fence refusal (COOPSAVE1)
//
// SYNB IS ONE HANDLER, COOPWORLD1's (hal/coop_world.cpp, coop_wire.h). This
// file registers two ranges with coop_ctl_register: 0x20-0x2f is routed to
// port::coop_save_handle() (COOPSAVE1; coop_save_set_external_dispatch(true)
// is called so it does not install its own handler), 0x30-0x3f is this lane.
// The world core installs its handler at the first level frame; until then
// (guest gate, title) a stand-in handler here fans the same two ranges out.
//
// HOOKS (tests/walk_window.cpp, one line each):
//   port_coop_boot_begin()        in main before port_coop_save_boot()
//   port_coop_boot_scene_frame()  pumps the hub while the title runs (no hold)
//   port_coop_boot_frame()        returns 1 = hold this level tick
#ifndef PORT_HAL_COOP_BOOT_H
#define PORT_HAL_COOP_BOOT_H

namespace port {

enum CoopExit : int {
    kCoopExitNoHost   = 6,   // no host STATE or no save answer in 3 s
    kCoopExitHostLeft = 7,   // the host left (or vanished): room closed
};

bool coop_boot_active();     // coop room member with the boot gate running
bool coop_boot_released();   // GO has been given (or heard) on this console
bool coop_boot_holding();    // a barrier is holding this console right now
bool coop_boot_ticking();    // game frames really tick: released, no barrier, no opening playing (true when not a coop room)

struct CoopBootStats {
    unsigned ready_sent, ready_recv, state_sent, state_recv;
    unsigned arrived_sent, arrived_recv, close_sent, close_recv;
    unsigned hostleft_sent, hostleft_recv;
    unsigned dropped_bad, dropped_role, dropped_world, dropped_unknown;
};
CoopBootStats coop_boot_stats();

}  // namespace port

extern "C" void port_coop_boot_begin(void);
extern "C" int port_coop_boot_scene_frame(void);
extern "C" int port_coop_boot_frame(void);

#endif  // PORT_HAL_COOP_BOOT_H
