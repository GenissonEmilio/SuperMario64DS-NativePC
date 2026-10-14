/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN8CccArenaD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjEwbIce_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjEwbIce_c.h"

extern "C" daObjEwbIce_c *_ZN13daObjEwbIce_cD0Ev(daObjEwbIce_c *thiz)
{
    thiz->daObjEwbIce_c::~daObjEwbIce_c();    /* the D1 body, through MSVC's one destructor */
    daObjEwbIce_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
