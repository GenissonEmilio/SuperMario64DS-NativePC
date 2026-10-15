/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN13BowserShutterD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_wl_koopa_shutter.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjWlKoopaShutter_c.h"

extern "C" daObjWlKoopaShutter_c *_ZN21daObjWlKoopaShutter_cD0Ev(daObjWlKoopaShutter_c *thiz)
{
    thiz->daObjWlKoopaShutter_c::~daObjWlKoopaShutter_c();    /* the D1 body, through MSVC's one destructor */
    daObjWlKoopaShutter_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
