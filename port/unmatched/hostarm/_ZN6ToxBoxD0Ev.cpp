/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN6ToxBoxD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_onms.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daOnms_c.h"

extern "C" daOnms_c *_ZN8daOnms_cD0Ev(daOnms_c *thiz)
{
    thiz->daOnms_c::~daOnms_c();    /* the D1 body, through MSVC's one destructor */
    daOnms_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
