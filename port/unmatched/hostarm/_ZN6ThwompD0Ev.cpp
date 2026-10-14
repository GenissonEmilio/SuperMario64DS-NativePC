/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN6ThwompD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daDsn_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daDsn_c.h"

extern "C" daDsn_c *_ZN7daDsn_cD0Ev(daDsn_c *thiz)
{
    thiz->daDsn_c::~daDsn_c();    /* the D1 body, through MSVC's one destructor */
    daDsn_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
