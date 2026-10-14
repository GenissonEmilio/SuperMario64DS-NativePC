/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN8BigBullyD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daBDonketu_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daBDonketu_c.h"

extern "C" daBDonketu_c *_ZN12daBDonketu_cD0Ev(daBDonketu_c *thiz)
{
    thiz->daBDonketu_c::~daBDonketu_c();    /* the D1 body, through MSVC's one destructor */
    daBDonketu_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
