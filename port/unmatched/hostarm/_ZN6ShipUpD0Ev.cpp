/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN6ShipUpD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_ki_fune.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjKi_Fune_c.h"

extern "C" daObjKi_Fune_c *_ZN14daObjKi_Fune_cD0Ev(daObjKi_Fune_c *thiz)
{
    thiz->daObjKi_Fune_c::~daObjKi_Fune_c();    /* the D1 body, through MSVC's one destructor */
    daObjKi_Fune_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
