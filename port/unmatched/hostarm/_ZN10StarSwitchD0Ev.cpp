/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10StarSwitchD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_switch.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjSwitch_c.h"

extern "C" daObjSwitch_c *_ZN13daObjSwitch_cD0Ev(daObjSwitch_c *thiz)
{
    thiz->daObjSwitch_c::~daObjSwitch_c();    /* the D1 body, through MSVC's one destructor */
    daObjSwitch_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
