/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN14dScMgCurling_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/minigames/d_s_mg_curling.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dScMgCurling_c.h"

extern "C" dScMgCurling_c *_ZN14dScMgCurling_cD0Ev(dScMgCurling_c *thiz)
{
    thiz->dScMgCurling_c::~dScMgCurling_c();    /* the D1 body, through MSVC's one destructor */
    dScMgCurling_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
