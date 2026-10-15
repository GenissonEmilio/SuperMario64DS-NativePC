/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZThn56_N12dBgCh_SphCrrD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/collision/d_bg_ch_sph_crr.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 * Main's TU names this thunk only in a comment: mwccarm emits it from the
 * written destructor, MSVC never does, so the flat name is defined only here. */
// @symbol _ZThn56_N12dBgCh_SphCrrD0Ev
/* Compiler-owned deleting-destructor adjustment thunk for the dM3dGSph base. */
#include "dBgCh_SphCrr.h"

/* THE HOST NEEDS THE ROM'S FLAT THUNK NAME, AND MSVC NEVER EMITS IT. mwccarm
 * emits this thunk beside the destructor: `ldr ip,=-56; add r0,r0,ip;
 * b _ZN12dBgCh_SphCrrD0Ev`, which moves `this` from the dM3dGSph base at +0x38 back to
 * the whole dBgCh_SphCrr and runs its deleting destructor. MSVC makes
 * adjustor thunks only for its own vftables, and those are not this one. This
 * arm is the same two steps, reached from slot 1 of the port's copy of the
 * ROM's secondary table VTable_dM3dGSph_dBgCh_SphCrrThunk (0x0209935c, filled in
 * port/hal/w2_dtor_heads.cpp). The destructor itself is defined by the class's D1
 * file (now main's TU); defining it here as well would define it twice under MSVC. Nothing
 * here reaches mwccarm. */
extern "C" dBgCh_SphCrr *_ZN12dBgCh_SphCrrD0Ev(dBgCh_SphCrr *thiz);

extern "C" dBgCh_SphCrr *_ZThn56_N12dBgCh_SphCrrD0Ev(dM3dGSph *thiz)
{
    return _ZN12dBgCh_SphCrrD0Ev((dBgCh_SphCrr *)((char *)thiz - 56));
}
