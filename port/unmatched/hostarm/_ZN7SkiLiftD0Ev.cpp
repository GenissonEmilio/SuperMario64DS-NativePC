/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN7SkiLiftD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjSm_Lift_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjSm_Lift_c.h"

extern "C" daObjSm_Lift_c *_ZN14daObjSm_Lift_cD0Ev(daObjSm_Lift_c *thiz)
{
    thiz->daObjSm_Lift_c::~daObjSm_Lift_c();    /* the D1 body, through MSVC's one destructor */
    daObjSm_Lift_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
