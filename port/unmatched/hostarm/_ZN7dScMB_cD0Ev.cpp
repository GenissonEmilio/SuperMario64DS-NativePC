/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN7dScMB_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/stages/d_s_mb.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dScMB_c.h"

extern "C" dScMB_c *_ZN7dScMB_cD0Ev(dScMB_c *thiz)
{
    thiz->dScMB_c::~dScMB_c();    /* the D1 body, through MSVC's one destructor */
    dScMB_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
