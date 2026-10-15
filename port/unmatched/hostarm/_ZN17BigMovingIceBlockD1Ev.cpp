/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN17BigMovingIceBlockD1Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_ewm_ice_block.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 *
 * WHY IT EXISTS. The host needs the ROM's flat D1 name: slot 16 of the class's
 * table (port/hal/actor_classes_ov056.cpp) and the D0 carry beside this file
 * call it, and MSVC only emits ??1daObjEwmIceBlock_c. Main now defines
 * ~daObjEwmIceBlock_c() out of line in its TU (it is the key function there),
 * where the old port line had it inline in the class; either way the
 * qualified, direct call below reaches MSVC's one destructor body. QUALIFIED
 * on purpose: `self->~daObjEwmIceBlock_c()` would be a virtual call through
 * the port's ROM-shaped table at slot 16, straight back into this symbol.
 * The inline chain MSVC emits stores an MSVC vftable where the cartridge's D1
 * (0x021111a0, ov056) stores _ZTV18daObjEwmIceBlock_c;
 * port/tools/dtor_store_guard.py re-proves from the ROM body that nothing
 * dispatches before the base destructor restores a ROM-shaped table (that
 * tool read this arm in its old src/ location). Logic unchanged from the arm;
 * only the names moved (BigMovingIceBlock -> daObjEwmIceBlock_c). */
#include "daObjEwmIceBlock_c.h"

extern "C" void _ZN18daObjEwmIceBlock_cD1Ev(void *self)
{ ((daObjEwmIceBlock_c *)self)->daObjEwmIceBlock_c::~daObjEwmIceBlock_c(); }
