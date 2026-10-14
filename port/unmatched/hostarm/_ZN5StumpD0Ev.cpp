/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN5StumpD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjPile_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjPile_c.h"

extern "C" daObjPile_c *_ZN11daObjPile_cD0Ev(daObjPile_c *thiz)
{
    thiz->daObjPile_c::~daObjPile_c();    /* the D1 body, through MSVC's one destructor */
    daObjPile_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
