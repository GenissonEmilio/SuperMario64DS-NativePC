//cpp
// @symbol _ZN15dExtFrameCtrl_cC1Ev
/* recovered: real C++ constructor -- base step (none: root class) and the
 * vptr store are synthesized; only the two body statements are written.
 *
 * The ROM stores _ZTV15dExtFrameCtrl_c, zeroes currFrame (+0x8) and sets speed
 * (+0xc) to 0x1000 = 1.0 -- in that order, which is vptr-then-body per the
 * measured emission order (notes/ctor-migration.md section 6). The C2
 * sibling this TU also emits is stripped by objisolate; that variant's
 * enrolled home is src/_ZN15dExtFrameCtrl_cC2Ev.cpp.
 */
#include "dExtFrameCtrl_c.h"

dExtFrameCtrl_c::dExtFrameCtrl_c()
{
    currFrame = 0;
    speed = 0x1000;
}
