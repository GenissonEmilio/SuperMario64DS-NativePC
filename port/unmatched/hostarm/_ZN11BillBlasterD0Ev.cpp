/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN11BillBlasterD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_bk_killerdai.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjBkKillerdai_c.h"

extern "C" daObjBkKillerdai_c *_ZN18daObjBkKillerdai_cD0Ev(daObjBkKillerdai_c *thiz)
{
    thiz->daObjBkKillerdai_c::~daObjBkKillerdai_c();    /* the D1 body, through MSVC's one destructor */
    daObjBkKillerdai_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
