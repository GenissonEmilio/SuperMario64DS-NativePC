/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10dFdDummy_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/runtime/graphics/fader/d_fader.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dFdDummy_c.h"

extern "C" dFdDummy_c *_ZN10dFdDummy_cD0Ev(dFdDummy_c *thiz)
{
    thiz->dFdDummy_c::~dFdDummy_c();    /* the D1 body, through MSVC's one destructor */
    dFdDummy_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
