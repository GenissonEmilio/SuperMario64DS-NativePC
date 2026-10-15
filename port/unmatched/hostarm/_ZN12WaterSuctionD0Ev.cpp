/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN12WaterSuctionD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_water_suikomi.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daWater_Suikomi_c.h"

extern "C" daWater_Suikomi_c *_ZN17daWater_Suikomi_cD0Ev(daWater_Suikomi_c *thiz)
{
    thiz->daWater_Suikomi_c::~daWater_Suikomi_c();    /* the D1 body, through MSVC's one destructor */
    daWater_Suikomi_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
