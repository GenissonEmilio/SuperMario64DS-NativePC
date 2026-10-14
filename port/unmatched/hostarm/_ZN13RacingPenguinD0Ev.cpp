/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN13RacingPenguinD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_pg_rcer.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daPgRcer_c.h"

extern "C" daPgRcer_c *_ZN10daPgRcer_cD0Ev(daPgRcer_c *thiz)
{
    thiz->daPgRcer_c::~daPgRcer_c();    /* the D1 body, through MSVC's one destructor */
    daPgRcer_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
