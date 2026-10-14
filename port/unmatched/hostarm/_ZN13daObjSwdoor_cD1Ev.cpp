/* HOST ARM, recovered by PORTSYNC2.
 * Old path: src/_ZN13daObjSwdoor_cD1Ev.cpp (its #ifdef _MSC_VER arm). The
 * 2026-10 main merge folded that file into src/actors/daObjSwdoor_c.cpp, whose
 * own _MSC_VER arm defines only the flat D0 (_ZN13daObjSwdoor_cD0Ev) and says
 * no flat D1 provider is supplied there. mwccarm never saw this arm, so nothing
 * here moves a ROM byte.
 *
 * WHY IT EXISTS. The host needs the ROM's flat D1 name and MSVC never emits it:
 * ~daObjSwdoor_c() is defined in the class body, and the port's slot 16 for
 * this class (port/hal/actor_base_tables_ov002.cpp) calls the flat name. This
 * defines it as the qualified, direct call that makes MSVC emit the body.
 * QUALIFIED as in the old arm, which guarded against a virtual call through
 * the port's ROM-shaped table at slot 16 (straight back into this symbol);
 * main's header declares ~daObjSwdoor_c non-virtual (MSVC emits
 * ??1daObjSwdoor_c@@QAE@XZ), so the qualifier is now belt-and-braces and the
 * object code is the same. The inline chain MSVC emits stores an MSVC vftable
 * (??_7dBgActor_c@@6B@, measured) where the cartridge's D1 (0x020bab64, ov002)
 * stores _ZTV13daObjSwdoor_c; port/tools/dtor_store_guard.py re-proves from the
 * ROM body that nothing dispatches before the base destructor restores a
 * ROM-shaped table. */
#include "daObjSwdoor_c.h"

// @symbol _ZN13daObjSwdoor_cD1Ev
extern "C" void _ZN13daObjSwdoor_cD1Ev(void *self)
{ ((daObjSwdoor_c *)self)->daObjSwdoor_c::~daObjSwdoor_c(); }
