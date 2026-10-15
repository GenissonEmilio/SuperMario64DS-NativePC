// PORTDROP2. Main's three minigame factories (Cup, Roulette, Trampoline2)
// run on the host as written. Two of them size the new scene with
// sizeof(class) rather than the ROM's literal, and a host copy that spelled
// the literal used to hide any difference between the two. The headers state
// the ROM sizes only for the DS compiler (SM64DS_PLATFORM_PC skips them), so
// this file states them for the host compiler: if MSVC lays a class out at a
// different size than the ROM allocates, the build stops here instead of the
// factory writing past the end of its scene.
#include "dScMgCup_c.h"
#include "dScMgRoulette_c.h"
#include "dScMgTrampoline2_c.h"

static_assert(sizeof(dScMgCup_c) == 0x5470, "dScMgCup_c host size != ROM scene size");
static_assert(sizeof(dScMgRoulette_c) == 0x5400, "dScMgRoulette_c host size != ROM scene size");
static_assert(sizeof(dScMgTrampoline2_c) == 0x7bac, "dScMgTrampoline2_c host size != ROM scene size");
