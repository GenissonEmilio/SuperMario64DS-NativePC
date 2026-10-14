/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9WaterRingD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daWater_Ring_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daWater_Ring_c.h"

extern "C" daWater_Ring_c *_ZN14daWater_Ring_cD0Ev(daWater_Ring_c *thiz)
{
    thiz->daWater_Ring_c::~daWater_Ring_c();    /* the D1 body, through MSVC's one destructor */
    daWater_Ring_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
