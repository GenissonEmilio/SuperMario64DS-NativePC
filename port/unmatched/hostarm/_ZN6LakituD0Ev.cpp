/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN6LakituD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_jgm.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daJgm_c.h"

extern "C" daJgm_c *_ZN7daJgm_cD0Ev(daJgm_c *thiz)
{
    thiz->daJgm_c::~daJgm_c();    /* the D1 body, through MSVC's one destructor */
    daJgm_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
