/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN9LightBeamD1Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_c1_hikari.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. Renamed by address to main's
 * _ZN15daObjC1Hikari_cD1Ev / class daObjC1Hikari_c.
 * Called by port/hal/actor_classes.cpp's slot 16 for LIGHT_BEAM (cartridge D1 0x021119d0, ov010). */
#include "daObjC1Hikari_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D1 NAME, AND MSVC NEVER EMITS IT. ~daObjC1Hikari_c() is
 * defined in the class body, and MSVC emits an inline destructor only for a
 * caller that odr-uses it, so this arm defines the flat name as the qualified,
 * direct call that makes MSVC emit the body (the shape port/tools/dtorfwd.py
 * generates). QUALIFIED on purpose: `self->~daObjC1Hikari_c()` would be a virtual call
 * through the port's ROM-shaped table at slot 16, straight back into this
 * symbol. The inline chain MSVC emits stores an MSVC vftable where the
 * cartridge's D1 stores _ZTV15daObjC1Hikari_c; port/tools/dtor_store_guard.py proves
 * nothing dispatches before the base destructor restores a ROM-shaped table. */
extern "C" void _ZN15daObjC1Hikari_cD1Ev(void *self)
{ ((daObjC1Hikari_c *)self)->daObjC1Hikari_c::~daObjC1Hikari_c(); }
