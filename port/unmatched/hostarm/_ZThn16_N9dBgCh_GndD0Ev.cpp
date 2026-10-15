/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZThn16_N9dBgCh_GndD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/collision/d_bg_ch_gnd.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 * Main's TU names this thunk only as an @symbol marker on the written
 * destructor: mwccarm emits it from that destructor, MSVC never does, so the
 * flat name is defined only here. */
// @symbol _ZThn16_N9dBgCh_GndD0Ev
/* Compiler-owned deleting-destructor adjustment thunk for the dBgPi base. */
#include "dBgCh_Gnd.h"

/* THE HOST NEEDS THE ROM'S FLAT THUNK NAME, AND MSVC NEVER EMITS IT. mwccarm
 * emits this thunk beside the destructor: `ldr ip,=-16; add r0,r0,ip;
 * b _ZN9dBgCh_GndD0Ev`, which moves `this` from the dBgPi base at +0x10 back to
 * the whole dBgCh_Gnd and runs its deleting destructor. MSVC makes
 * adjustor thunks only for its own vftables, and those are not this one. This
 * arm is the same two steps, reached from slot 1 of the port's copy of the
 * ROM's secondary table VTable_dBgPi_dBgCh_GndThunk (0x02099278, filled in
 * port/hal/w2_dtor_heads.cpp). The destructor itself is defined by main's TU;
 * defining it here as well would define it twice under MSVC. Nothing
 * here reaches mwccarm. */
extern "C" dBgCh_Gnd *_ZN9dBgCh_GndD0Ev(dBgCh_Gnd *thiz);

extern "C" dBgCh_Gnd *_ZThn16_N9dBgCh_GndD0Ev(dBgPi *thiz)
{
    return _ZN9dBgCh_GndD0Ev((dBgCh_Gnd *)((char *)thiz - 16));
}
