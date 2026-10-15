/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN13KoopaTheQuickD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_r_nk.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daRNk_c.h"

extern "C" daRNk_c *_ZN7daRNk_cD0Ev(daRNk_c *thiz)
{
    thiz->daRNk_c::~daRNk_c();    /* the D1 body, through MSVC's one destructor */
    daRNk_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
