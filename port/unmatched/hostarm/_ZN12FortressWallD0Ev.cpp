/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN12FortressWallD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjBk_Kabe_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjBk_Kabe_c.h"

extern "C" daObjBk_Kabe_c *_ZN14daObjBk_Kabe_cD0Ev(daObjBk_Kabe_c *thiz)
{
    thiz->daObjBk_Kabe_c::~daObjBk_Kabe_c();    /* the D1 body, through MSVC's one destructor */
    daObjBk_Kabe_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
