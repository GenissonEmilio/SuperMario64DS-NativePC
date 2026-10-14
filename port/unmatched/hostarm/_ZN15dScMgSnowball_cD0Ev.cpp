/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN15dScMgSnowball_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/dScMgSnowball_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dScMgSnowball_c.h"

extern "C" dScMgSnowball_c *_ZN15dScMgSnowball_cD0Ev(dScMgSnowball_c *thiz)
{
    thiz->dScMgSnowball_c::~dScMgSnowball_c();    /* the D1 body, through MSVC's one destructor */
    dScMgSnowball_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
