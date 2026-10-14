//cpp
// @symbol _ZN15dExtFrameCtrl_cD1Ev
/* D1, the complete-object destructor. One `dExtFrameCtrl_c::~dExtFrameCtrl_c()` definition makes
 * mwcc emit D0, D1 and D2 together; objisolate keeps the one this file is
 * bound to by its delinks entry, so the D0/D2 siblings carry the same
 * definition in their own files. */
#include "dExtFrameCtrl_c.h"

dExtFrameCtrl_c::~dExtFrameCtrl_c()
{
}
