/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/_ZN14daObjUkiyuka_cD0Ev.cpp, which carried this under #ifdef _MSC_VER. The
 * 2026-10 main merge took main's src/, which folded that code into
 * src/actors/daObjUkiyuka_c.cpp without the arm. MSVC emits one destructor per class and never the ROM's flat Itanium variant name, and port/hal calls that flat name from the class's vtable slot.
 * Only the host arm is here, retargeted to main's names; mwccarm never
 * compiles this file, so nothing here moves a ROM byte. Comments below that
 * name old per-function src/ files describe the pre-merge tree. */
// host arm for ROM symbol _ZN14daObjUkiyuka_cD0Ev
/* recovered: real C++ deleting destructor, defined inline in the header
 *
 * ~daObjUkiyuka_c is defined in the class body -- this class's descendants
 * inline its vptr store rather than calling it, which the compiler can only
 * do from a visible body. So this file cannot define it, and a TU that
 * merely includes the header emits nothing. The delete-expression below
 * forces the deleting destructor's own out-of-line copy into existence;
 * objisolate keeps the one this file is bound to and drops the rest.
 *
 * A DELETE-EXPRESSION, NOT `p->~daObjUkiyuka_c()`, for the same reason
 * src/_ZN10dBgActor_cD0Ev.cpp gives: dBgActor_c declares Kill, a key
 * function reachable from this class, so a plain destructor call would not
 * reproduce D0 out of line. `delete p` asks for the deleting half by name
 * and brings it back.
 *
 * D0 is the deleting half: destroy through daObjUkiyuka_c and dBgActor_c,
 * then hand the object back through dActor_c's inline operator delete, which
 * is why nothing here mentions a heap.
 */
#include "daObjUkiyuka_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D0 NAME, AND MSVC NEVER EMITS IT. ~daObjUkiyuka_c()
 * is defined in the class body, so there is no out-of-line host body for
 * a qualified call to reach: the call would inline a second copy of the
 * destructor. The D1 is reachable only by its flat ROM name, and that is
 * the call port/faces_sync.txt's face for this D0 made, so this arm makes
 * the same two calls that face made: the D1 body through its flat name,
 * then the class-specific operator delete. Nothing here reaches mwccarm:
 * it builds the `#else` arm and emits the ROM bytes it always emitted, and
 * the object is byte-identical either way. */
extern "C" void _ZN14daObjUkiyuka_cD1Ev(void *self);
extern "C" daObjUkiyuka_c *_ZN14daObjUkiyuka_cD0Ev(daObjUkiyuka_c *thiz)
{
    _ZN14daObjUkiyuka_cD1Ev(thiz);          /* the D1 body, through its flat ROM name */
    daObjUkiyuka_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
