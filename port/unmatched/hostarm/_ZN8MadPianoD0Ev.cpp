/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/game/actors/daPiano_c/_ZN8MadPianoD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/daPiano_c/d_a_piano.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daPiano_c.h"

extern "C" daPiano_c *_ZN9daPiano_cD0Ev(daPiano_c *thiz)
{
    thiz->daPiano_c::~daPiano_c();    /* the D1 body, through MSVC's one destructor */
    daPiano_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
