/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN22RotatingUpDownPlatformD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_linelift2.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daLinelift2_c.h"

extern "C" daLinelift2_c *_ZN13daLinelift2_cD0Ev(daLinelift2_c *thiz)
{
    thiz->daLinelift2_c::~daLinelift2_c();    /* the D1 body, through MSVC's one destructor */
    daLinelift2_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
