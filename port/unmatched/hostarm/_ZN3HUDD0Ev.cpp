/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN3HUDD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/dMeter_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dMeter_c.h"

extern "C" dMeter_c *_ZN8dMeter_cD0Ev(dMeter_c *thiz)
{
    thiz->dMeter_c::~dMeter_c();    /* the D1 body, through MSVC's one destructor */
    dMeter_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
