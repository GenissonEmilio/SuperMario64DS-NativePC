/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/game/actors/WingFeather/_ZN11WingFeatherD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_feather.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daFeather_c.h"

extern "C" daFeather_c *_ZN11daFeather_cD0Ev(daFeather_c *thiz)
{
    thiz->daFeather_c::~daFeather_c();    /* the D1 body, through MSVC's one destructor */
    daFeather_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
