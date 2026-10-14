/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/_ZN11MirrorLuigiD1Ev.cpp, which carried this under #ifdef _MSC_VER. The
 * 2026-10 main merge took main's src/, which folded that code into
 * src/actors/daLuigi_c.cpp without the arm. MSVC emits one destructor per class and never the ROM's flat Itanium variant name, and port/hal calls that flat name from the class's vtable slot.
 * Only the host arm is here, retargeted to main's names; mwccarm never
 * compiles this file, so nothing here moves a ROM byte. Comments below that
 * name old per-function src/ files describe the pre-merge tree. */
// host arm for ROM symbol _ZN9daLuigi_cD1Ev
/* Real compiler-spelled complete destructor. The empty body is intentional:
 * mwccarm synthesises the reverse member teardown and dActor_c base chain from
 * daLuigi_c's declared object layout. objisolate retains this D1 contribution
 * and discards D0/D2 plus the compatibility-name vtable and RTTI passengers. */
#include "daLuigi_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D1 NAME, AND MSVC ONLY MAKES ONE DESTRUCTOR.
 * It folds the Itanium D1/D0 pair into the single ??1MirrorLuigi that
 * src/_ZN9daLuigi_cD0Ev.cpp compiles for the port, so this file cannot carry
 * the same definition (LNK2005). This arm spells the ROM's own D1 instead
 * (overlay 55, 0x021111a0, 0x58 bytes), step for step: the store of this
 * class's own table, __cxa_vec_cleanup over the two TextureSequences at +0x1b0
 * with their D1, then the dExtShadowModel_c at +0x188, the Model at +0x138 and the
 * ModelAnim at +0xd4, each by its flat ROM name, then the dActor_c base D2, and
 * no deallocation (slot 16's caller frees). Those are the calls the slot-16
 * thunk ml_d1 in hal/actor_classes_ov055.cpp made by hand, which forwards here
 * now. Nothing here reaches mwccarm: it builds the `#else` arm and emits the
 * ROM bytes it always emitted, and the object is byte-identical either way. */
extern "C" {
int __cxa_vec_cleanup(void *base, int n, int stride, void *dtor);
void _ZN15TextureSequenceD1Ev(void *p);
void _ZN17dExtShadowModel_cD1Ev(void *p);
void _ZN5ModelD1Ev(void *p);
void _ZN9ModelAnimD1Ev(void *p);
void _ZN8dActor_cD2Ev(void *p);
extern void *_ZTV9daLuigi_c[];
}
extern "C" daLuigi_c *_ZN9daLuigi_cD1Ev(daLuigi_c *thiz)
{
    char *t = (char *)thiz;
    *(void **)t = (void *)_ZTV9daLuigi_c;  /* this class's own table */
    __cxa_vec_cleanup(t + 0x1b0, 2, 0x14, (void *)&_ZN15TextureSequenceD1Ev);
    _ZN17dExtShadowModel_cD1Ev(t + 0x188);
    _ZN5ModelD1Ev(t + 0x138);
    _ZN9ModelAnimD1Ev(t + 0xd4);
    _ZN8dActor_cD2Ev(t);                      /* the base, no deallocation */
    return thiz;
}
