/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN4ViewD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/view/dView_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 *
 * WHY IT EXISTS. The host needs the ROM's flat D0 name (port/hal/dtor_seats_base.cpp holds it as a
 * vtable slot), and MSVC never emits it: ~dView_c() is defined in the class
 * body, so a qualified call from here would inline a second copy of the
 * whole destructor chain. The port's D1 for this class is its flat ROM name
 * (a port/faces_sync.txt face onto ??1dView_c), so this makes the D0's two
 * calls: the D1 body through that flat name, then the class-specific
 * operator delete. Logic unchanged from the arm; only the names moved
 * (class View -> dView_c, _ZN4ViewD0Ev/_ZN4ViewD1Ev -> _ZN7dView_cD0Ev/_ZN7dView_cD1Ev). */
#include "dView_c.h"

extern "C" void _ZN7dView_cD1Ev(void *self);
extern "C" dView_c *_ZN7dView_cD0Ev(dView_c *thiz)
{
    _ZN7dView_cD1Ev(thiz);          /* the D1 body, through its flat ROM name */
    dView_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
