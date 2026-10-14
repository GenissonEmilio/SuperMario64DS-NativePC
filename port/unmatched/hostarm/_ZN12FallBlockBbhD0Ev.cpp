/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/game/actors/daObjTh_Fall_Block_c/_ZN12FallBlockBbhD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_th_fall_block.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjTh_Fall_Block_c.h"

extern "C" daObjTh_Fall_Block_c *_ZN20daObjTh_Fall_Block_cD0Ev(daObjTh_Fall_Block_c *thiz)
{
    thiz->daObjTh_Fall_Block_c::~daObjTh_Fall_Block_c();    /* the D1 body, through MSVC's one destructor */
    daObjTh_Fall_Block_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
