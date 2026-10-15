/* HOST ARM, recovered by PORTSYNC2.
 * Old path: src/_ZN8daDemo_c13simpleModel_cD0Ev.cpp (its #ifdef _MSC_VER arm). The
 * 2026-10 main merge folded that file into src/game/actors/d_a_demo.cpp, which
 * carries no host arm for this D0 (only for _ZN8daDemo_cD0Ev). mwccarm never
 * saw this arm, so nothing here moves a ROM byte.
 *
 * WHY IT EXISTS. The host needs the ROM's flat D0 name (0x020f69a8, ov002;
 * port/hal/actor_classes_intro.cpp seats it) and MSVC never emits it: MSVC
 * folds the Itanium destructor variants into the one ~simpleModel_c() main's TU
 * defines. A qualified call to that destructor would not be the face's call:
 * simpleModel_c has a virtual base, so MSVC treats an explicit destructor call as
 * the complete-object (vbase) destructor and adds the virtual base's teardown
 * and an EH frame. The retired port/faces_sync.txt face for this D0 called the
 * flat D1 name, so this makes the same two calls that face made: the D1 body
 * through its flat ROM name (a live port/faces_sync.txt face onto
 * ??1simpleModel_c@daDemo_c@@UAE@XZ), then the class-specific operator delete. */
#include "daDemo_c.h"

// @symbol _ZN8daDemo_c13simpleModel_cD0Ev
extern "C" void _ZN8daDemo_c13simpleModel_cD1Ev(void *self);
extern "C" daDemo_c::simpleModel_c *_ZN8daDemo_c13simpleModel_cD0Ev(daDemo_c::simpleModel_c *thiz)
{
    _ZN8daDemo_c13simpleModel_cD1Ev(thiz);           /* the D1 body, through its flat ROM name */
    daDemo_c::simpleModel_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
