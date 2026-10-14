/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/_ZN11daDsnBase_cD1Ev.cpp, which carried this under #ifdef _MSC_VER. The
 * 2026-10 main merge took main's src/, which folded that code into
 * src/actors/daDsnBase_c.cpp without the arm. MSVC emits one destructor per class and never the ROM's flat Itanium variant name, and port/hal calls that flat name from the class's vtable slot.
 * Only the host arm is here, retargeted to main's names; mwccarm never
 * compiles this file, so nothing here moves a ROM byte. Comments below that
 * name old per-function src/ files describe the pre-merge tree. */
// host arm for ROM symbol _ZN11daDsnBase_cD1Ev
/* recovered: real C++ destructor -- inline in the class, forced out of line here
 *
 * daDsnBase_c.h defines `~daDsnBase_c() {}` in the class body on purpose, so
 * every descendant's destructor inlines it; a TU that merely includes the
 * header emits no _ZN11daDsnBase_cD1Ev, but the ROM carries one at ov091
 * 0x02132d6c. The explicit call below cannot be satisfied by the inline copy
 * alone, so mwcc emits the out-of-line variant and objisolate drops the
 * forcing function's own .text (notes/dtor-migration.md section 3). The body
 * it emits is the class's: the dExtShadowModel_c and TextureSequence members torn
 * down in reverse declaration order, the dBgActor_c base step last.
 */
#include "daDsnBase_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D1 NAME, AND MSVC NEVER EMITS IT.
 * ~daDsnBase_c() is defined in the class body, and MSVC emits an inline
 * destructor only for a caller that odr-uses it. The port's slot 16 for this
 * class (actorport_d16t_daDsnBase_c in hal/actorport_dtor_bridge.cpp, the
 * compiler table's Destructor1) calls the flat name, so this arm defines it as
 * the qualified, direct call that makes MSVC emit the body, the shape
 * port/tools/dtorfwd.py generates for every inline-destructor class. QUALIFIED
 * on purpose: `self->~daDsnBase_c()` would be a virtual call through the
 * port's table at slot 16, straight back into this symbol. The inline chain
 * MSVC emits stores an MSVC vftable where the cartridge's D1 (0x02132d6c,
 * ov091) stores _ZTV11daDsnBase_c; port/tools/dtor_store_guard.py reads this
 * arm (SRC_ARMS) and re-proves from the ROM body on every build that nothing
 * dispatches before the base destructor restores a ROM-shaped table. Nothing
 * here reaches mwccarm: it builds the `#else` arm and emits the ROM bytes it
 * always emitted. */
extern "C" void _ZN11daDsnBase_cD1Ev(void *self)
{ ((daDsnBase_c *)self)->daDsnBase_c::~daDsnBase_c(); }
