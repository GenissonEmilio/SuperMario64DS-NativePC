/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN6NumberD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_number.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjNumber_c.h"

extern "C" daObjNumber_c *_ZN13daObjNumber_cD0Ev(daObjNumber_c *thiz)
{
    thiz->daObjNumber_c::~daObjNumber_c();    /* the D1 body, through MSVC's one destructor */
    daObjNumber_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
