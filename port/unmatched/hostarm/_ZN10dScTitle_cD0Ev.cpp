/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10dScTitle_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/dScTitle_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dScTitle_c.h"

extern "C" dScTitle_c *_ZN10dScTitle_cD0Ev(dScTitle_c *thiz)
{
    thiz->dScTitle_c::~dScTitle_c();    /* the D1 body, through MSVC's one destructor */
    dScTitle_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
