// CASTLE UNLOCKED, the door side (run after093, lane EVENTS2). See hal/event_client.h for the event itself.
//
// While a castle-unlocked event is live on this console every star door, key door and star gate behaves as if the player held every star
// and both keys, and NOTHING is written to the save. How, with no change to the cartridge's code:
//
// The ROM's closed door (daDoor_c idle callback, src/game/actors/d_a_door.cpp func_ov100_02144cf8) and the big star gate's waiting state
// (daStarGate_c::St_Wait_Main, src/game/actors/d_a_star_gate.cpp) each ask the save's flag word (data_0209caa0 + 4) FIRST whether the door's
// own "unlocked" bit is set, and if it is they send the player straight through without looking at the star count or the keys:
//     star door  flags1 & (0x8000 << entry->flagShift)      (DOOR_SAVE_STAR_UNLOCKED)
//     key door   flags1 & (0x100  << entry->flagShift)      (DOOR_SAVE_KEY_UNLOCKED)
//     star gate  flags1 & (0x8000 << entry->saveFlag)
// So the hook raises exactly that one bit for the one door being asked, calls the ROM body unchanged, and puts the bit back to what it was
// before returning. The bit is raised only for the duration of that one call (single-threaded host), so the save never carries it into a
// save write, a pause, or another door; the "opened by a key" / "opened by a star" writes (d_a_door.cpp lines 649 and 730) live in the
// unlock states, which this path never enters (it takes the same exit as a door that was unlocked long ago). When the event ends the next
// idle call finds the bit clear and the door asks for its stars or its key again.
//
// Seam: unmatched/Door_Behavior.cpp (the idle face) and hal/actor_overlays.cpp (the gate's Wait face) call a hook pointer that is null on
// every target that does not seat it; this file seats them (walk_window targets only, like hal/char_door.cpp).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include "event_client.h"

extern "C" {
extern unsigned char data_0209caa0[];
/* the ROM bodies the faces would call, C names as the faces use them */
int _ZN8daDoor_c19func_ov100_02144cf8EP6Player(void *self, void *player);
int _ZN12daStarGate_c12St_Wait_MainEP6Player(void *self, void *player);
/* the seams (unmatched/Door_Behavior.cpp, hal/actor_overlays.cpp) */
extern int (*port_event_door_idle_hook)(void *door, void *player);
extern int (*port_event_gate_wait_hook)(void *gate, void *player);
/* the ROM tables the ROM bodies index by the actor's param1 (+8) */
extern unsigned char data_ov100_02148204[];      /* DoorEntry[]: 16 bytes each; +8 starsNeeded, +9 keyIndex, +10 flagShift */
extern unsigned char data_ov100_02148390[];      /* daStarGateInfo[4]: 6 bytes each; +0 numStars, +1 saveFlag */
}

namespace {

enum : unsigned { kStarUnlocked = 0x8000u, kKeyUnlocked = 0x100u };
enum { kDoorEntries = 0x130 / 16, kGateEntries = 4 };

unsigned g_door_calls, g_gate_calls;

// The door's own unlock bit for this entry (0 = none: a plain door, or a shift that does not name a bit).
unsigned door_bit(void *door)
{
    const unsigned idx = *(unsigned *)((char *)door + 8);
    if (idx >= (unsigned)kDoorEntries) return 0;
    const signed char *e = (const signed char *)(data_ov100_02148204 + idx * 16);
    const int stars = e[8], key = e[9], sh = e[10];
    if (sh < 0 || sh > 8) return 0;
    if (stars >= 0) return kStarUnlocked << sh;
    if (key >= 0) return kKeyUnlocked << sh;
    return 0;
}

#ifndef PORT_ROM_CLEAN
// proof aid (dev builds only): SM64DS_CASTLE_LOG=1 prints each door once, with the position and what it asks for
void castle_log_once(const char *kind, void *a, unsigned idx, int stars, int key, int sh)
{
    static int lg = -1;
    static void *seen[64];
    static int nseen = 0;
    if (lg < 0) lg = getenv("SM64DS_CASTLE_LOG") ? 1 : 0;
    if (!lg) return;
    for (int i = 0; i < nseen; ++i) if (seen[i] == a) return;
    if (nseen < 64) seen[nseen++] = a;
    const int *pos = (const int *)((char *)a + 0x5c);
    fprintf(stderr, "[castle-log] %s %p param %u pos=(%d,%d,%d) stars %d key %d shift %d\n", kind, a, idx, pos[0] >> 12, pos[1] >> 12,
            pos[2] >> 12, stars, key, sh);
}
#endif

int door_hook(void *door, void *player)
{
#ifndef PORT_ROM_CLEAN
    {
        const unsigned i = *(unsigned *)((char *)door + 8);
        if (i < (unsigned)kDoorEntries) {
            const signed char *e = (const signed char *)(data_ov100_02148204 + i * 16);
            castle_log_once("door", door, i, e[8], e[9], e[10]);
        }
    }
#endif
    if (!port_event_castle_live()) return 0;
    const unsigned bit = door_bit(door);
    if (!bit) return 0;
    unsigned *flags1 = (unsigned *)(data_0209caa0 + 4);
    const unsigned before = *flags1;
    *flags1 = before | bit;
    const int r = _ZN8daDoor_c19func_ov100_02144cf8EP6Player(door, player);
    *flags1 = (*flags1 & ~bit) | (before & bit);
    if (!(before & bit) && (++g_door_calls & 4095u) == 1u)
        fprintf(stderr, "[event] castle: door %u held open for the player (calls %u)\n", *(unsigned *)((char *)door + 8), g_door_calls);
    return 1;
}

int gate_hook(void *gate, void *player)
{
#ifndef PORT_ROM_CLEAN
    {
        const unsigned i = *(unsigned *)((char *)gate + 8);
        if (i < (unsigned)kGateEntries) {
            const signed char *e = (const signed char *)(data_ov100_02148390 + i * 6);
            castle_log_once("gate", gate, i, e[0], -1, e[1]);
        }
    }
#endif
    if (!port_event_castle_live()) return 0;
    const unsigned idx = *(unsigned *)((char *)gate + 8);
    if (idx >= (unsigned)kGateEntries) return 0;
    const int sh = ((const signed char *)(data_ov100_02148390 + idx * 6))[1];
    if (sh < 0 || sh > 8) return 0;
    const unsigned bit = kStarUnlocked << sh;
    unsigned *flags1 = (unsigned *)(data_0209caa0 + 4);
    const unsigned before = *flags1;
    *flags1 = before | bit;
    _ZN12daStarGate_c12St_Wait_MainEP6Player(gate, player);
    *flags1 = (*flags1 & ~bit) | (before & bit);
    if (!(before & bit) && (++g_gate_calls & 4095u) == 1u)
        fprintf(stderr, "[event] castle: star gate %u held open for the player (calls %u)\n", idx, g_gate_calls);
    return 1;
}

// EVENTS3: the star-count view. The ROM sites that gate on the star total (a Toad's line, the rabbit tiers, the 14-star light, the endless stairs)
// call port_event_numstars (src/engine/save/SaveData.cpp _MSC_VER arm), which asks this view; while a castle event is live every total reads
// as all 150 stars. Nothing is written: the real total, the HUD, the presence and the save keep the true count.
extern "C" unsigned char (*port_event_star_view)(unsigned char real);
unsigned char star_view(unsigned char real)
{
    if (!port_event_castle_live()) return real;
#ifndef PORT_ROM_CLEAN
    {   // proof aid (dev builds only): each distinct caller of the view once, as return addresses to read against the map
        static void *seen[32];
        static int nseen = 0;
        void *bt[4] = {};
        RtlCaptureStackBackTrace(1, 3, bt, 0);
        bool dup = false;
        for (int i = 0; i < nseen; ++i) dup = dup || seen[i] == bt[1];
        if (!dup && nseen < 32) {
            seen[nseen++] = bt[1];
            fprintf(stderr, "[event] castle: star view asked from %p %p %p (real %u)\n", bt[0], bt[1], bt[2], real);
        }
    }
#endif
    static unsigned n;
    if (real < 150 && (++n & 1023u) == 1u) fprintf(stderr, "[event] castle: a star check sees 150 stars (real %u, calls %u)\n", real, n);
    return 150;
}

int seat(void)
{
    port_event_star_view = star_view;
    port_event_door_idle_hook = door_hook;
    port_event_gate_wait_hook = gate_hook;
    return 1;
}
const int g_seated = seat();

}  // namespace
