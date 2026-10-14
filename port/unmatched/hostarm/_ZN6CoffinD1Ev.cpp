/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN6CoffinD1Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjCasket_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. Renamed by address to main's
 * _ZN13daObjCasket_cD1Ev / class daObjCasket_c.
 * Called by port/hal/actor_classes_ov063.cpp's slot 16 and by the D0 arm port/unmatched/hostarm/_ZN6CoffinD0Ev.cpp (cartridge D1 0x02121fe4, ov071). */
#include "daObjCasket_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D1 NAME, AND MSVC NEVER EMITS IT. ~daObjCasket_c() is
 * defined in the class body, and MSVC emits an inline destructor only for a
 * caller that odr-uses it, so this arm defines the flat name as the qualified,
 * direct call that makes MSVC emit the body (the shape port/tools/dtorfwd.py
 * generates). QUALIFIED on purpose: `self->~daObjCasket_c()` would be a virtual call
 * through the port's ROM-shaped table at slot 16, straight back into this
 * symbol. The inline chain MSVC emits stores an MSVC vftable where the
 * cartridge's D1 stores _ZTV13daObjCasket_c; port/tools/dtor_store_guard.py proves
 * nothing dispatches before the base destructor restores a ROM-shaped table. */
extern "C" void _ZN13daObjCasket_cD1Ev(void *self)
{ ((daObjCasket_c *)self)->daObjCasket_c::~daObjCasket_c(); }
