/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN11dScMiniGm_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/dScMiniGm_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dScMiniGm_c.h"

extern "C" dScMiniGm_c *_ZN11dScMiniGm_cD0Ev(dScMiniGm_c *thiz)
{
    thiz->dScMiniGm_c::~dScMiniGm_c();    /* the D1 body, through MSVC's one destructor */
    dScMiniGm_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
