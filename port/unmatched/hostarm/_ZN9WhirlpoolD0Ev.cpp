/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9WhirlpoolD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daWater_Tatumaki_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daWater_Tatumaki_c.h"

extern "C" daWater_Tatumaki_c *_ZN18daWater_Tatumaki_cD0Ev(daWater_Tatumaki_c *thiz)
{
    thiz->daWater_Tatumaki_c::~daWater_Tatumaki_c();    /* the D1 body, through MSVC's one destructor */
    daWater_Tatumaki_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
