/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/_ZN11PyramidLiftD0Ev.cpp, which carried this under #ifdef _MSC_VER. The
 * 2026-10 main merge took main's src/, which folded that code into
 * src/game/actors/d_a_dp_lift.cpp without the arm. MSVC emits one destructor per class and never the ROM's flat Itanium variant name, and port/hal calls that flat name from the class's vtable slot.
 * Only the host arm is here, retargeted to main's names; mwccarm never
 * compiles this file, so nothing here moves a ROM byte. Comments below that
 * name old per-function src/ files describe the pre-merge tree. */
// host arm for ROM symbol _ZN10daDpLift_cD0Ev
/* Real compiler-spelled deleting destructor. The delete-expression selects
 * D0 and reaches dActor_c's inline actor-heap deallocator after the complete
 * daDpLift_c destruction sequence. */
#include "daDpLift_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D0 NAME, AND MSVC NEVER EMITS IT. ~daDpLift_c()
 * is defined in the class body, so a qualified call from here would inline
 * a second copy of the whole destructor chain into this arm. The port's D1
 * for this class is its flat ROM name, the name the class's vtable slot 16
 * holds, so this arm makes the D0's two calls: the D1 body through that
 * flat name, then the class-specific operator delete. Nothing here reaches
 * mwccarm: it builds the `#else` arm and emits the ROM bytes it always
 * emitted, and the object is byte-identical either way. */
extern "C" void _ZN10daDpLift_cD1Ev(void *self);
extern "C" daDpLift_c *_ZN10daDpLift_cD0Ev(daDpLift_c *thiz)
{
    _ZN10daDpLift_cD1Ev(thiz);          /* the D1 body, through its flat ROM name */
    daDpLift_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
