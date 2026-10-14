/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9TinyCoverD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjTtFuta_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjTtFuta_c.h"

extern "C" daObjTtFuta_c *_ZN13daObjTtFuta_cD0Ev(daObjTtFuta_c *thiz)
{
    thiz->daObjTtFuta_c::~daObjTtFuta_c();    /* the D1 body, through MSVC's one destructor */
    daObjTtFuta_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
