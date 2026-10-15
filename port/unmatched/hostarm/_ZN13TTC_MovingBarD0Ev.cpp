/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN13TTC_MovingBarD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_ct_kaitendai.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjCtKaitendai_c.h"

extern "C" daObjCtKaitendai_c *_ZN18daObjCtKaitendai_cD0Ev(daObjCtKaitendai_c *thiz)
{
    thiz->daObjCtKaitendai_c::~daObjCtKaitendai_c();    /* the D1 body, through MSVC's one destructor */
    daObjCtKaitendai_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
