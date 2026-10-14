/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN8MantaRayD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daManta_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daManta_c.h"

extern "C" daManta_c *_ZN9daManta_cD0Ev(daManta_c *thiz)
{
    thiz->daManta_c::~daManta_c();    /* the D1 body, through MSVC's one destructor */
    daManta_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
