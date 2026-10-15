/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN11PowerFlowerD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_power_up_item.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjPowerUpItem_c.h"

extern "C" daObjPowerUpItem_c *_ZN18daObjPowerUpItem_cD0Ev(daObjPowerUpItem_c *thiz)
{
    thiz->daObjPowerUpItem_c::~daObjPowerUpItem_c();    /* the D1 body, through MSVC's one destructor */
    daObjPowerUpItem_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
