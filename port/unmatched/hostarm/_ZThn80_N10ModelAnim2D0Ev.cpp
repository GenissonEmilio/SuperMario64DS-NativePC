/* HOST ARM recovered by PORTSYNC2 (copied from the arm src/_ZThn80_N10ModelAnim2D0Ev.cpp still carries).
 * The 2026-10 main merge (749f5799a) kept src/_ZThn80_N10ModelAnim2D0Ev.cpp and its
 * #ifdef _MSC_VER arm, but main renamed the thunk's base class Animation to
 * dExtFrameCtrl_c, so the arm no longer compiles on the host. This copy is the
 * same arm with the parameter type renamed; the src row is off the slice.
 * mwccarm never saw the arm, so nothing here moves a ROM byte. */
// @symbol _ZThn80_N10ModelAnim2D0Ev
#include "ModelAnim2.h"

/* THE HOST NEEDS THE ROM'S FLAT THUNK NAME, AND MSVC NEVER EMITS IT. mwccarm
 * emits this thunk beside the destructor: `ldr ip,=-80; add r0,r0,ip;
 * b _ZN10ModelAnim2D0Ev`, which moves `this` from the dExtFrameCtrl_c (was Animation) base at +0x50 back to
 * the whole ModelAnim2 and runs its deleting destructor. MSVC makes
 * adjustor thunks only for its own vftables, and those are not this one. This
 * arm is the same two steps, reached from slot 1 of the port's copy of the
 * ROM's secondary table VTable_Animation_ModelAnim2Thunk (0x0208e9dc, filled in
 * port/hal/cxxname_bridge.cpp). The destructor itself is defined by the class's D1
 * file; defining it here as well would define it twice under MSVC. */
extern "C" ModelAnim2 *_ZN10ModelAnim2D0Ev(ModelAnim2 *thiz);

extern "C" ModelAnim2 *_ZThn80_N10ModelAnim2D0Ev(dExtFrameCtrl_c *thiz)
{
    return _ZN10ModelAnim2D0Ev((ModelAnim2 *)((char *)thiz - 80));
}
