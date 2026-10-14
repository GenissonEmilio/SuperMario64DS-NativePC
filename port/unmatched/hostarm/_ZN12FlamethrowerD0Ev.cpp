/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN12FlamethrowerD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjFlamethrower_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjFlamethrower_c.h"

extern "C" daObjFlamethrower_c *_ZN19daObjFlamethrower_cD0Ev(daObjFlamethrower_c *thiz)
{
    thiz->daObjFlamethrower_c::~daObjFlamethrower_c();    /* the D1 body, through MSVC's one destructor */
    daObjFlamethrower_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
