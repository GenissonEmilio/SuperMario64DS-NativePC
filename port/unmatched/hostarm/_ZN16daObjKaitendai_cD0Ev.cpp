/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN16daObjKaitendai_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_kaitendai.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 *
 * WHY IT EXISTS. The host needs the ROM's flat D0 name (port/hal/actor_classes_ov036.cpp holds it as a
 * vtable slot), and MSVC never emits it: ~daObjKaitendai_c() is defined in the class
 * body, so a qualified call from here would inline a second copy of the
 * whole destructor chain. The port's D1 for this class is its flat ROM name
 * (a port/faces_sync.txt face onto ??1daObjKaitendai_c), so this makes the D0's two
 * calls: the D1 body through that flat name, then the class-specific
 * operator delete. Logic unchanged from the arm. */
#include "daObjKaitendai_c.h"

extern "C" void _ZN16daObjKaitendai_cD1Ev(void *self);
extern "C" daObjKaitendai_c *_ZN16daObjKaitendai_cD0Ev(daObjKaitendai_c *thiz)
{
    _ZN16daObjKaitendai_cD1Ev(thiz);          /* the D1 body, through its flat ROM name */
    daObjKaitendai_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
