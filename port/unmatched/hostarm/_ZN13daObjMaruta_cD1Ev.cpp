/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/_ZN13daObjMaruta_cD1Ev.cpp, which carried this under #ifdef _MSC_VER. The
 * 2026-10 main merge took main's src/, which folded that code into
 * src/game/actors/d_a_obj_maruta.cpp without the arm. MSVC emits one destructor per class and never the ROM's flat Itanium variant name, and port/hal calls that flat name from the class's vtable slot.
 * Only the host arm is here, retargeted to main's names; mwccarm never
 * compiles this file, so nothing here moves a ROM byte. Comments below that
 * name old per-function src/ files describe the pre-merge tree. */
// host arm for ROM symbol _ZN13daObjMaruta_cD1Ev
/* recovered: real C++ destructor -- inline in the class, forced out of line here
 *
 * daObjMaruta_c.h defines `~daObjMaruta_c() {}` in the class body on purpose,
 * so every descendant's destructor inlines it; a TU that merely includes the
 * header emits no _ZN13daObjMaruta_cD1Ev, but the ROM carries one at ov080
 * 0x02127014. The explicit call below forces mwcc to emit the out-of-line
 * variant, and objisolate drops the forcing function's own .text
 * (notes/dtor-migration.md section 3).
 */
#include "daObjMaruta_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D1 NAME, AND MSVC NEVER EMITS IT. The
 * destructor is inline in the class and MSVC folds the Itanium D1/D0 pair into
 * one, so the forcing call below would give the host no body under this name.
 * The name is what slot 16 of _ZTV13daObjMaruta_c holds (ov080 0x02128378),
 * and the port hosts that table as an array (hal/actor_classes_ov080.cpp).
 * This arm spells the ROM's own D1 (ov080 0x02127014, 0x44 bytes) step for
 * step: the store of this class's own table, the store of dBgActor_c's that
 * its inlined base destructor makes, the MovingMeshCollider at +0x124 and the
 * Model at +0xd4 by their flat ROM names, then the dActor_c base D2, and no
 * deallocation (slot 16's caller frees). The class is abstract (slots 0, 3
 * and 6 are null), so on the cartridge this body is reached only through that
 * table. Nothing here reaches mwccarm: it builds the `#else` arm and emits the
 * ROM bytes it always emitted, and the object is byte-identical either way. */
extern "C" {
void _ZN10dBgW_KcMbgD1Ev(void *self);
void _ZN5ModelD1Ev(void *self);
void *_ZN8dActor_cD2Ev(void *self);
extern void *_ZTV13daObjMaruta_c[];
extern void *_ZTV10dBgActor_c[];
}
extern "C" daObjMaruta_c *_ZN13daObjMaruta_cD1Ev(daObjMaruta_c *thiz)
{
    char *t = (char *)thiz;
    /* THE WORD IS VOLATILE ON PURPOSE. The two table stores are back to
       back, and MSVC deletes the first as a dead store (then folds this
       body with the other class's identical one); the ROM makes both, so
       both are made here. */
    void *volatile *vptr = (void *volatile *)t;
    *vptr = (void *)_ZTV13daObjMaruta_c;        /* this class's own table */
    *vptr = (void *)_ZTV10dBgActor_c;           /* the inlined ~dBgActor_c's */
    _ZN10dBgW_KcMbgD1Ev(t + 0x124);
    _ZN5ModelD1Ev(t + 0xd4);
    _ZN8dActor_cD2Ev(t);                        /* the base, no deallocation */
    return thiz;
}
