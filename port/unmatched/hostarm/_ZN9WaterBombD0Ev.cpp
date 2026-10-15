/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9WaterBombD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_wbm.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daWbm_c.h"

extern "C" daWbm_c *_ZN7daWbm_cD0Ev(daWbm_c *thiz)
{
    thiz->daWbm_c::~daWbm_c();    /* the D1 body, through MSVC's one destructor */
    daWbm_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
