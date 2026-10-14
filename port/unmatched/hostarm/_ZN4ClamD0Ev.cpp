/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN4ClamD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjShell_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjShell_c.h"

extern "C" daObjShell_c *_ZN12daObjShell_cD0Ev(daObjShell_c *thiz)
{
    thiz->daObjShell_c::~daObjShell_c();    /* the D1 body, through MSVC's one destructor */
    daObjShell_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
