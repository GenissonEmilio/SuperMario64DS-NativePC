/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN8SnowballD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_snowball.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daSnowball_c.h"

extern "C" daSnowball_c *_ZN12daSnowball_cD0Ev(daSnowball_c *thiz)
{
    thiz->daSnowball_c::~daSnowball_c();    /* the D1 body, through MSVC's one destructor */
    daSnowball_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
