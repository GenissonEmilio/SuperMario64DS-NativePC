/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN13RollingLogTtmD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjHmMaruta_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjHmMaruta_c.h"

extern "C" daObjHmMaruta_c *_ZN15daObjHmMaruta_cD0Ev(daObjHmMaruta_c *thiz)
{
    thiz->daObjHmMaruta_c::~daObjHmMaruta_c();    /* the D1 body, through MSVC's one destructor */
    daObjHmMaruta_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
