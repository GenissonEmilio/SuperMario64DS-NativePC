/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN8dActor_c10EarthquakeERK7Vector35Fix12IiE.cpp
 * under #ifdef _MSC_VER; the 2026-10 main merge (749f5799a) took main's src/, where
 * that code now lives in src/runtime/actor/d_actor.cpp without the arm. mwccarm never saw
 * the arm, so nothing here moves a ROM byte.
 *
 * Main's TU defines the same extern "C" flat name (two-argument form), so the
 * TU's hostgen copy must park _ZN8dActor_c10EarthquakeERK7Vector35Fix12IiE. */
// @symbol _ZN8dActor_c10EarthquakeERK7Vector35Fix12IiE
#include "dActor_c.h"

extern "C" {
/* THE STRENGTH IS FORWARDED, AS THE ROM FORWARDS IT. The ROM body at
   0x02010168 only replaces r0 (this) with the camera and tail-branches to
   func_0200d8c8, so the position in r1 and the strength in r2 reach it
   untouched and the two-argument call byte-matches. On x86 the third
   argument has to be pushed: without it func_0200d8c8 read the stack slot
   above its arguments (a stack address, 0x001af054) as the strength instead
   of the caller's 0x320000..0x860000, and the camera shake radius of every
   crusher, Whomp and boss landing was wrong. Declared with the strength, the
   host passes what r2 held. */
extern void func_0200d8c8(void*, int, int);
extern void* data_0209f318;

void _ZN8dActor_c10EarthquakeERK7Vector35Fix12IiE(void* v, int f, int strength) {
    func_0200d8c8(data_0209f318, f, strength);
}
}
