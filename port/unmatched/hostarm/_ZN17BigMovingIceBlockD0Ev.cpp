/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN17BigMovingIceBlockD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjEwmIceBlock_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 *
 * WHY IT EXISTS. The host needs the ROM's flat D0 name (slot 17 of the
 * class's table, port/hal/actor_classes_ov056.cpp), and MSVC never emits it:
 * it folds the Itanium D1/D0 pair into one ??1daObjEwmIceBlock_c. The port's
 * D1 for this class is its flat ROM name (hostarm/_ZN17BigMovingIceBlockD1Ev.cpp),
 * so this makes the D0's two calls: the D1 body through that flat name, then
 * the class-specific operator delete. Logic unchanged from the arm; only the
 * names moved (BigMovingIceBlock -> daObjEwmIceBlock_c,
 * _ZN17BigMovingIceBlockD0Ev/D1Ev -> _ZN18daObjEwmIceBlock_cD0Ev/D1Ev). */
#include "daObjEwmIceBlock_c.h"

extern "C" void _ZN18daObjEwmIceBlock_cD1Ev(void *self);
extern "C" daObjEwmIceBlock_c *_ZN18daObjEwmIceBlock_cD0Ev(daObjEwmIceBlock_c *thiz)
{
    _ZN18daObjEwmIceBlock_cD1Ev(thiz);          /* the D1 body, through its flat ROM name */
    daObjEwmIceBlock_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
