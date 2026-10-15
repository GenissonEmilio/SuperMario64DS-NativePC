/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/_ZN13daObjDorifu_cD0Ev.cpp, which carried this under #ifdef _MSC_VER. The
 * 2026-10 main merge took main's src/, which folded that code into
 * src/game/actors/d_a_obj_dorifu.cpp without the arm. MSVC emits one destructor per class and never the ROM's flat Itanium variant name, and port/hal calls that flat name from the class's vtable slot.
 * Only the host arm is here, retargeted to main's names; mwccarm never
 * compiles this file, so nothing here moves a ROM byte. Comments below that
 * name old per-function src/ files describe the pre-merge tree. */
// host arm for ROM symbol _ZN13daObjDorifu_cD0Ev
/* recovered: real C++ deleting destructor, defined inline in the header
 *
 * ~daObjDorifu_c is defined in the class body -- this class's three
 * descendants inline its vptr store rather than calling it, which the
 * compiler can only do from a visible body. So this file cannot define it,
 * and a TU that merely includes the header emits nothing. The delete-
 * expression below forces the deleting destructor's own out-of-line copy
 * into existence; objisolate keeps the one this file is bound to and drops
 * the rest.
 *
 * A DELETE-EXPRESSION, NOT `p->~daObjDorifu_c()`, for the same reason
 * src/_ZN10dBgActor_cD0Ev.cpp gives: dBgActor_c declares Kill, a key
 * function reachable from this class, so a plain destructor call would not
 * reproduce D0 out of line. `delete p` asks for the deleting half by name
 * and brings it back.
 *
 * The array cleanup is this class's own, same as its D1 sibling: a
 * dBgW_KcMbg[5] at 0x4b0 and a Model[5] at 0x320 are destroyed
 * before the vptr is restored to dBgActor_c's, then the inherited
 * dBgW_KcMbg (0x124) and Model (0xd4), then dActor_c's base
 * destructor, then the object is handed back through dActor_c's inline
 * operator delete.
 */
#include "daObjDorifu_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D0 NAME, AND MSVC NEVER EMITS IT. ~daObjDorifu_c()
 * is defined in the class body, so a qualified call from here would inline
 * a second copy of the whole destructor chain into this arm. The port's D1
 * for this class is its flat ROM name, the name the class's vtable slot 16
 * holds, so this arm makes the D0's two calls: the D1 body through that
 * flat name, then the class-specific operator delete. Nothing here reaches
 * mwccarm: it builds the `#else` arm and emits the ROM bytes it always
 * emitted, and the object is byte-identical either way. */
extern "C" void _ZN13daObjDorifu_cD1Ev(void *self);
extern "C" daObjDorifu_c *_ZN13daObjDorifu_cD0Ev(daObjDorifu_c *thiz)
{
    _ZN13daObjDorifu_cD1Ev(thiz);          /* the D1 body, through its flat ROM name */
    daObjDorifu_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
