/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN13FortressTowerD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjSimpleBg_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjSimpleBg_c.h"

extern "C" daObjSimpleBg_c *_ZN15daObjSimpleBg_cD0Ev(daObjSimpleBg_c *thiz)
{
    thiz->daObjSimpleBg_c::~daObjSimpleBg_c();    /* the D1 body, through MSVC's one destructor */
    daObjSimpleBg_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
