/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN14SquarePathLiftD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_emm_yuka.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjEmmYuka_c.h"

extern "C" daObjEmmYuka_c *_ZN14daObjEmmYuka_cD0Ev(daObjEmmYuka_c *thiz)
{
    thiz->daObjEmmYuka_c::~daObjEmmYuka_c();    /* the D1 body, through MSVC's one destructor */
    daObjEmmYuka_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
