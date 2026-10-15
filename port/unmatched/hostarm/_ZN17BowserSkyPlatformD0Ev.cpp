/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN17BowserSkyPlatformD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_kpa3_bg.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daKpa3Bg_c.h"

extern "C" daKpa3Bg_c *_ZN10daKpa3Bg_cD0Ev(daKpa3Bg_c *thiz)
{
    thiz->daKpa3Bg_c::~daKpa3Bg_c();    /* the D1 body, through MSVC's one destructor */
    daKpa3Bg_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
