/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZThn80_N8daDemo_c10anmModel_cD1Ev.cpp under
 * #ifdef _MSC_VER; the 2026-10 main merge (749f5799a) took main's src/, which keeps
 * that file but rewrites it (nested daDemo_c classes declared locally, the
 * anmModel_c and simpleModel_c destructors as real members) without the arm.
 * Main names this thunk only through the file's // @symbol marker: mwccarm emits
 * it from the written destructor, MSVC never does, so the flat name is defined
 * only here. mwccarm never saw the arm, so nothing here moves a ROM byte. */
// @symbol _ZThn80_N8daDemo_c10anmModel_cD1Ev
/* Compiler-owned complete-destructor adjustment thunk. */
#include "ModelAnim.h" /* dExtFrameCtrl_c, the old Animation; param type renamed per the map */

/* THE HOST NEEDS THE ROM'S FLAT THUNK NAME, AND MSVC NEVER EMITS IT. mwccarm
 * emits this thunk beside the destructor: `ldr ip,=-80; add r0,r0,ip;
 * b _ZN8daDemo_c10anmModel_cD1Ev`, which moves `this` from the dExtFrameCtrl_c (was Animation) base at +0x50 back to
 * the whole daDemo_c::anmModel_c and runs its complete-object destructor. MSVC makes
 * adjustor thunks only for its own vftables, and those are not this one. This
 * arm is the same two steps, reached from slot 0 of the port's copy of the
 * ROM's secondary table data_ov002_0210bce8 (0x0210bce8, filled in
 * port/hal/w2_dtor_heads.cpp). The destructor itself is defined elsewhere;
 * defining it here as well would define it twice under MSVC. */
extern "C" void _ZN8daDemo_c10anmModel_cD1Ev(void *self);

extern "C" void _ZThn80_N8daDemo_c10anmModel_cD1Ev(dExtFrameCtrl_c *thiz)
{
    _ZN8daDemo_c10anmModel_cD1Ev((void *)((char *)thiz - 80));
}
