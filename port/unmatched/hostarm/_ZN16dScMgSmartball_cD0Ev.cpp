/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN16dScMgSmartball_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/dScMgSmartball_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dScMgSmartball_c.h"

extern "C" dScMgSmartball_c *_ZN16dScMgSmartball_cD0Ev(dScMgSmartball_c *thiz)
{
    thiz->dScMgSmartball_c::~dScMgSmartball_c();    /* the D1 body, through MSVC's one destructor */
    dScMgSmartball_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
