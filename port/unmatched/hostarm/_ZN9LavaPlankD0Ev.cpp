/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9LavaPlankD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_fl_uki_ki.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjFl_UkiKi_c.h"

extern "C" daObjFl_UkiKi_c *_ZN15daObjFl_UkiKi_cD0Ev(daObjFl_UkiKi_c *thiz)
{
    thiz->daObjFl_UkiKi_c::~daObjFl_UkiKi_c();    /* the D1 body, through MSVC's one destructor */
    daObjFl_UkiKi_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
