/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9TinyWaterD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjTtWater_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjTtWater_c.h"

extern "C" daObjTtWater_c *_ZN14daObjTtWater_cD0Ev(daObjTtWater_c *thiz)
{
    thiz->daObjTtWater_c::~daObjTtWater_c();    /* the D1 body, through MSVC's one destructor */
    daObjTtWater_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
