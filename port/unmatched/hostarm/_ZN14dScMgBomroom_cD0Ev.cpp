/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN14dScMgBomroom_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/minigames/d_s_mg_bomroom.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dScMgBomroom_c.h"

extern "C" dScMgBomroom_c *_ZN14dScMgBomroom_cD0Ev(dScMgBomroom_c *thiz)
{
    thiz->dScMgBomroom_c::~dScMgBomroom_c();    /* the D1 body, through MSVC's one destructor */
    dScMgBomroom_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
