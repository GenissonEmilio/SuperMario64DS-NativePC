/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN7dCcAc_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/collision/dCcAc_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dCcAc_c.h"

extern "C" dCcAc_c *_ZN7dCcAc_cD0Ev(dCcAc_c *thiz)
{
    thiz->dCcAc_c::~dCcAc_c();    /* the D1 body, through MSVC's one destructor */
    dCcAc_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
