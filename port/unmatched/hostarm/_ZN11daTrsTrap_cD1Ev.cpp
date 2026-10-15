/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/game/actors/daTrsTrap_c/_ZN11daTrsTrap_cD1Ev.cpp (port line). The
 * 2026-10 main merge (749f5799a) took main's src/, which defines
 * daTrsTrap_c::~daTrsTrap_c as a real member in
 * src/game/actors/d_a_trs_trap.cpp; under MSVC that is the one
 * decorated ??1daTrsTrap_c, never the ROM's flat D1 name, which the slot-16
 * thunk ms_d1 in hal/actor_classes_ov063.cpp calls. mwccarm never sees this
 * file, so nothing here moves a ROM byte. */
// @symbol _ZN11daTrsTrap_cD1Ev
/* Forward-declared, not #include "daTrsTrap_c.h": main's header declares
   operator new(unsigned long), which MSVC rejects (size_t is unsigned int on
   the 32-bit host), and this body only needs the type's name. */
struct daTrsTrap_c;

/* THE HOST NEEDS THE ROM'S FLAT D1 NAME, AND MSVC ONLY MAKES ONE DESTRUCTOR.
 * It folds the Itanium D1/D0 pair into the single ??1daTrsTrap_c that main's
 * TU compiles for the port, so this file cannot carry the same definition
 * (LNK2005). This arm spells the ROM's own D1 instead, step for step: the
 * store of this class's own table (_ZTV11daTrsTrap_c, aliased to the port's
 * _ZTV12MansionSteps copy in hal/actor_classes_ov063.cpp), the dBgW_KcMbg at
 * +0x15c (mMovingMeshCollider) and the Model at +0xd4 (mModel) by their flat
 * ROM names, then the dActor_c base D2, and no deallocation (slot 16's caller
 * frees). */
extern "C" {
void _ZN10dBgW_KcMbgD1Ev(void *self);
void _ZN5ModelD1Ev(void *self);
void *_ZN8dActor_cD2Ev(void *self);
extern void *_ZTV11daTrsTrap_c[];
}
extern "C" daTrsTrap_c *_ZN11daTrsTrap_cD1Ev(daTrsTrap_c *thiz)
{
    char *t = (char *)thiz;
    *(void **)t = (void *)_ZTV11daTrsTrap_c;  /* this class's own table */
    _ZN10dBgW_KcMbgD1Ev(t + 0x15c);
    _ZN5ModelD1Ev(t + 0xd4);
    _ZN8dActor_cD2Ev(t);                      /* the base, no deallocation */
    return thiz;
}
