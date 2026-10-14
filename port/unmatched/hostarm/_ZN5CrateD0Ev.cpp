/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN5CrateD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjBlockS_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjBlockS_c.h"

extern "C" daObjBlockS_c *_ZN13daObjBlockS_cD0Ev(daObjBlockS_c *thiz)
{
    thiz->daObjBlockS_c::~daObjBlockS_c();    /* the D1 body, through MSVC's one destructor */
    daObjBlockS_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
