/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/game/actors/daTrsIcon_c/_ZN11daTrsIcon_cD1Ev.cpp (port line). The
 * 2026-10 main merge (749f5799a) took main's src/, which defines
 * daTrsIcon_c::~daTrsIcon_c as a real member in src/game/actors/d_a_trs.cpp; under
 * MSVC that is the one decorated ??1daTrsIcon_c, never the ROM's flat D1 name,
 * which the slot-16 thunk bbi_d1 in hal/actor_classes_ov063.cpp calls. mwccarm
 * never sees this file, so nothing here moves a ROM byte. */
// @symbol _ZN11daTrsIcon_cD1Ev
#include "daTrsIcon_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D1 NAME, AND MSVC ONLY MAKES ONE DESTRUCTOR.
 * It folds the Itanium D1/D0 pair into the single ??1daTrsIcon_c that main's
 * TU compiles for the port, so this file cannot carry the same definition
 * (LNK2005). This arm spells the ROM's own D1 instead, step for step: the store
 * of this class's own table (_ZTV10BigBooIcon, the name the port's table copy
 * in hal/actor_classes_ov063.cpp defines), then the dActor_c base D2, and no
 * deallocation (slot 16's caller frees). */
extern "C" {
void *_ZN8dActor_cD2Ev(void *self);
extern void *_ZTV10BigBooIcon[];
}
extern "C" daTrsIcon_c *_ZN11daTrsIcon_cD1Ev(daTrsIcon_c *thiz)
{
    *(void **)thiz = (void *)_ZTV10BigBooIcon;  /* this class's own table */
    _ZN8dActor_cD2Ev(thiz);                     /* the base, no deallocation */
    return thiz;
}
