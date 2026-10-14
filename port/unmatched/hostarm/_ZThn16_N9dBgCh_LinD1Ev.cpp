/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZThn16_N9dBgCh_LinD1Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/collision/dBgCh_Lin.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 * Main's TU names this thunk only in a comment: mwccarm emits it from the
 * written destructor, MSVC never does, so the flat name is defined only here. */
// @symbol _ZThn16_N9dBgCh_LinD1Ev
/* Compiler-owned complete-destructor adjustment thunk for the dBgPi base. */
#include "dBgCh_Lin.h"

/* THE HOST NEEDS THE ROM'S FLAT THUNK NAME, AND MSVC NEVER EMITS IT. mwccarm
 * emits this thunk beside the destructor: `ldr ip,=-16; add r0,r0,ip;
 * b _ZN9dBgCh_LinD1Ev`, which moves `this` from the dBgPi base at +0x10 back to
 * the whole dBgCh_Lin and runs its complete-object destructor. MSVC makes
 * adjustor thunks only for its own vftables, and those are not this one. This
 * arm is the same two steps, reached from slot 0 of the port's copy of the
 * ROM's secondary table VTable_dBgPi_dBgCh_LinThunk (0x020992b4, filled in
 * port/hal/w2_dtor_heads.cpp). The destructor itself is defined by the class's D1
 * file (now main's TU); defining it here as well would define it twice under MSVC. Nothing
 * here reaches mwccarm. */
extern "C" void _ZN9dBgCh_LinD1Ev(void *self);

extern "C" void _ZThn16_N9dBgCh_LinD1Ev(dBgPi *thiz)
{
    _ZN9dBgCh_LinD1Ev((void *)((char *)thiz - 16));
}
