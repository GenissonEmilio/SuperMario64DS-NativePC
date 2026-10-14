/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10LavaBubbleD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daBbl_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daBbl_c.h"

extern "C" daBbl_c *_ZN7daBbl_cD0Ev(daBbl_c *thiz)
{
    thiz->daBbl_c::~daBbl_c();    /* the D1 body, through MSVC's one destructor */
    daBbl_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
