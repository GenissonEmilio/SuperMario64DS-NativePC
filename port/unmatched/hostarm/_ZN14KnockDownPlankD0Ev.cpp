/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN14KnockDownPlankD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjBk_Botaosi_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjBk_Botaosi_c.h"

extern "C" daObjBk_Botaosi_c *_ZN17daObjBk_Botaosi_cD0Ev(daObjBk_Botaosi_c *thiz)
{
    thiz->daObjBk_Botaosi_c::~daObjBk_Botaosi_c();    /* the D1 body, through MSVC's one destructor */
    daObjBk_Botaosi_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
