/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN16daObjFallBlock_cD1Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjFallBlock_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 *
 * WHY IT EXISTS. The host needs the ROM's flat D1 name, and MSVC never emits
 * it: the destructor is inline in the class (on _MSC_VER the header spells
 * the pair as Destructor1/Destructor0 plus a slotless inline ~daObjFallBlock_c).
 * The name is what slot 16 of _ZTV16daObjFallBlock_c holds, and the port
 * hosts that table as an array (port/hal/actor_classes_ov098.cpp). This
 * spells the ROM's own D1 (ov098 0x02139fc8, 0x44 bytes) step for step: the store of this
 * class's own table, the store of dBgActor_c's that its inlined base
 * destructor makes, the dBgW_KcMbg (was MovingMeshCollider) at +0x124 and the Model at +0xd4
 * by their flat ROM names, then the dActor_c base D2, and no deallocation (slot 16's caller frees).
 * Logic unchanged from the arm. */
#include "daObjFallBlock_c.h"

extern "C" {
void _ZN10dBgW_KcMbgD1Ev(void *self);
void _ZN5ModelD1Ev(void *self);
void *_ZN8dActor_cD2Ev(void *self);
extern void *_ZTV16daObjFallBlock_c[];
extern void *_ZTV10dBgActor_c[];
}
extern "C" daObjFallBlock_c *_ZN16daObjFallBlock_cD1Ev(daObjFallBlock_c *thiz)
{
    char *t = (char *)thiz;
    /* THE WORD IS VOLATILE ON PURPOSE. The two table stores are back to
       back, and MSVC deletes the first as a dead store (then folds this
       body with the other class's identical one); the ROM makes both, so
       both are made here. */
    void *volatile *vptr = (void *volatile *)t;
    *vptr = (void *)_ZTV16daObjFallBlock_c;        /* this class's own table */
    *vptr = (void *)_ZTV10dBgActor_c;              /* the inlined ~dBgActor_c's */
    _ZN10dBgW_KcMbgD1Ev(t + 0x124);
    _ZN5ModelD1Ev(t + 0xd4);
    _ZN8dActor_cD2Ev(t);                           /* the base, no deallocation */
    return thiz;
}
