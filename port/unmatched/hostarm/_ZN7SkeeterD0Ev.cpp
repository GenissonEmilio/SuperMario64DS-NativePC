/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN7SkeeterD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_menbo.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daMenbo_c.h"

extern "C" daMenbo_c *_ZN9daMenbo_cD0Ev(daMenbo_c *thiz)
{
    thiz->daMenbo_c::~daMenbo_c();    /* the D1 body, through MSVC's one destructor */
    daMenbo_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
