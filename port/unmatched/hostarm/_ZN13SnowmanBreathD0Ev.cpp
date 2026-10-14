/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN13SnowmanBreathD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daSnmBth_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daSnmBth_c.h"

extern "C" daSnmBth_c *_ZN10daSnmBth_cD0Ev(daSnmBth_c *thiz)
{
    thiz->daSnmBth_c::~daSnmBth_c();    /* the D1 body, through MSVC's one destructor */
    daSnmBth_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
