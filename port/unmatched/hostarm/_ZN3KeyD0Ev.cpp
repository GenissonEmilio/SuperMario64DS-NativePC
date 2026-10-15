/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN3KeyD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_key.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjKey_c.h"

extern "C" daObjKey_c *_ZN10daObjKey_cD0Ev(daObjKey_c *thiz)
{
    thiz->daObjKey_c::~daObjKey_c();    /* the D1 body, through MSVC's one destructor */
    daObjKey_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
