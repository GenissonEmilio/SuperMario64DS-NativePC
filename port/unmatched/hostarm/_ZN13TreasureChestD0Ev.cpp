/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN13TreasureChestD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_tbox.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjTbox_c.h"

extern "C" daObjTbox_c *_ZN11daObjTbox_cD0Ev(daObjTbox_c *thiz)
{
    thiz->daObjTbox_c::~daObjTbox_c();    /* the D1 body, through MSVC's one destructor */
    daObjTbox_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
