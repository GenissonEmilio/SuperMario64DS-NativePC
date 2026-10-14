/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9LakituBroD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daC_Jugem_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daC_Jugem_c.h"

extern "C" daC_Jugem_c *_ZN11daC_Jugem_cD0Ev(daC_Jugem_c *thiz)
{
    thiz->daC_Jugem_c::~daC_Jugem_c();    /* the D1 body, through MSVC's one destructor */
    daC_Jugem_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
