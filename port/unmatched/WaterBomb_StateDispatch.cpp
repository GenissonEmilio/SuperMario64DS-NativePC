/* The WATER_BOMB's (WaterBomb / daWbm_c, actor 208, ov098) three-state table
 * seat, the Cap_StateDispatch / Cannon_Behavior treatment.
 *
 * WaterBomb::Behavior reads a {function, adjustment} pair out of
 * data_ov098_0213c930[unk_3c4] and calls it -- an mwcc pointer-to-member table
 * read as a plain function pointer. The three entries are seeded by
 * __sinit_ov098_0213c2b4 (linked in hal/actor_overlays.cpp), which copies three
 * SOURCE statics into the runtime table:
 *
 *     data_ov098_0213c930[0] = data_ov098_0213c724   -> func_ov098_0213b9d8
 *     data_ov098_0213c930[1] = data_ov098_0213c72c   -> func_ov098_0213bb1c
 *     data_ov098_0213c930[2] = data_ov098_0213c71c   -> func_ov098_0213b7e8
 *
 * The .fn words the mount ships for those three statics are the overlay image's
 * own -- DS CODE ADDRESSES (0213b9d8 / 0213bb1c / 0213b7e8), the ovdata
 * contract, matched by config/arm9/overlays/ov098/relocs.txt. Calling one jumps
 * into unbuilt DS memory, which is the fault the census caught the first frame a
 * WATER_BOMB reached WaterBomb::Behavior (FAULT accessing 0213b9d8).
 *
 * The seat rewrites the three SOURCE statics before the sinit copies them (the
 * LakituBro / Cap reading: one less mapping to get wrong than reseating the
 * runtime table after), each checked against the ROM address its host body was
 * compiled from -- a mount pointing at the wrong bytes says so instead of
 * calling into the overlay image. All three .delta halves are 0 (the
 * non-virtual complete-class form, confirmed against the overlay bytes), so the
 * pair is just the function pointer, and all three functions are matched src
 * (func_ov098_0213b7e8/9d8/bb1c), so there is no hole here: no state traps.
 */
#include <cstdio>
#include <cstdlib>
#include "hal/pmf_tc.h"

extern "C" {
void _ZN7daWbm_c9StateDropEv(void *c);   /* state 0, the fire */
void _ZN7daWbm_c9StateFallEv(void *c);   /* state 1, the landing */
void _ZN7daWbm_c11StateBounceEv(void *c);   /* state 2, the fall / Cannon_Kill share */

struct PortPmf { unsigned fn; int delta; };
/* the three source statics __sinit_ov098_0213c2b4 copies into
   data_ov098_0213c930 */
extern PortPmf data_ov098_0213c724[], data_ov098_0213c72c[],
    data_ov098_0213c71c[];
}  /* extern "C" */

static const struct { PortPmf *slot; unsigned rom; void (*host)(void *); }
g_water_bomb_states[] = {
    {data_ov098_0213c724, 0x0213b9d8, _ZN7daWbm_c9StateDropEv},
    {data_ov098_0213c72c, 0x0213bb1c, _ZN7daWbm_c9StateFallEv},
    {data_ov098_0213c71c, 0x0213b7e8, _ZN7daWbm_c11StateBounceEv},
};

extern "C" void port_water_bomb_states_seat(void)
{
    static int done;
    if (done)
        return;
    done = 1;
    for (unsigned i = 0; i < sizeof g_water_bomb_states /
                             sizeof g_water_bomb_states[0]; ++i) {
        PortPmf *p = g_water_bomb_states[i].slot;
        if ((unsigned)(p->fn) < 0x02000000u || (unsigned)(p->fn) >= 0x02400000u) continue;  /* PORTSYNC3: a mount word that is not a DS code address: the class TU owns the live table */
        if (p->fn != g_water_bomb_states[i].rom || p->delta != 0) {
            std::fprintf(stderr, "FATAL: WaterBomb state %u: the mount holds "
                         "%08x/%d, the ROM's own table says %08x/0 -- WRONG "
                         "BYTES\n", i, p->fn, p->delta,
                         g_water_bomb_states[i].rom);
            std::abort();
        }
        p->fn = PORT_PMF_TC(g_water_bomb_states[i].host);
    }
}
