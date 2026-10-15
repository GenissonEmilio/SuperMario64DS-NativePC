/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9ButterflyD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_btfly.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daBtfly_c.h"

extern "C" daBtfly_c *_ZN9daBtfly_cD0Ev(daBtfly_c *thiz)
{
    thiz->daBtfly_c::~daBtfly_c();    /* the D1 body, through MSVC's one destructor */
    daBtfly_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
