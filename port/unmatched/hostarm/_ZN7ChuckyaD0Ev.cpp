/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN7ChuckyaD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_holhei.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daHolhei_c.h"

extern "C" daHolhei_c *_ZN10daHolhei_cD0Ev(daHolhei_c *thiz)
{
    thiz->daHolhei_c::~daHolhei_c();    /* the D1 body, through MSVC's one destructor */
    daHolhei_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
