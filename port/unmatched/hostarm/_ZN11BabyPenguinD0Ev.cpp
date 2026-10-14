/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN11BabyPenguinD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_pg_bby.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daPgBby_c.h"

extern "C" daPgBby_c *_ZN9daPgBby_cD0Ev(daPgBby_c *thiz)
{
    thiz->daPgBby_c::~daPgBby_c();    /* the D1 body, through MSVC's one destructor */
    daPgBby_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
