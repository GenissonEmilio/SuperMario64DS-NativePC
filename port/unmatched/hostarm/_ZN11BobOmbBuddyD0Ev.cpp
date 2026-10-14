/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN11BobOmbBuddyD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_red_bombhei.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daRedBombhei_c.h"

extern "C" daRedBombhei_c *_ZN14daRedBombhei_cD0Ev(daRedBombhei_c *thiz)
{
    thiz->daRedBombhei_c::~daRedBombhei_c();    /* the D1 body, through MSVC's one destructor */
    daRedBombhei_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
