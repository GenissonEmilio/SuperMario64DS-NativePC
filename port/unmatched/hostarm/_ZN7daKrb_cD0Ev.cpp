/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN7daKrb_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daKrb_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daKrb_c.h"

extern "C" daKrb_c *_ZN7daKrb_cD0Ev(daKrb_c *thiz)
{
    thiz->daKrb_c::~daKrb_c();    /* the D1 body, through MSVC's one destructor */
    daKrb_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
