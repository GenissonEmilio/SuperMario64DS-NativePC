/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10DonutBlockD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_rc_tikuwa.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjRc_Tikuwa_c.h"

extern "C" daObjRc_Tikuwa_c *_ZN16daObjRc_Tikuwa_cD0Ev(daObjRc_Tikuwa_c *thiz)
{
    thiz->daObjRc_Tikuwa_c::~daObjRc_Tikuwa_c();    /* the D1 body, through MSVC's one destructor */
    daObjRc_Tikuwa_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
