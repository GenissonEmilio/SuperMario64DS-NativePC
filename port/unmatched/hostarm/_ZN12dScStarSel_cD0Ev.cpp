/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN12dScStarSel_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/stages/d_s_star_sel_methods.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dScStarSel_c.h"

extern "C" dScStarSel_c *_ZN12dScStarSel_cD0Ev(dScStarSel_c *thiz)
{
    thiz->dScStarSel_c::~dScStarSel_c();    /* the D1 body, through MSVC's one destructor */
    dScStarSel_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
