/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9JetStreamD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daWater_Hakidasi_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daWater_Hakidasi_c.h"

extern "C" daWater_Hakidasi_c *_ZN18daWater_Hakidasi_cD0Ev(daWater_Hakidasi_c *thiz)
{
    thiz->daWater_Hakidasi_c::~daWater_Hakidasi_c();    /* the D1 body, through MSVC's one destructor */
    daWater_Hakidasi_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
