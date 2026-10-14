/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN13PeachPaintingD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjC1Peach_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjC1Peach_c.h"

extern "C" daObjC1Peach_c *_ZN14daObjC1Peach_cD0Ev(daObjC1Peach_c *thiz)
{
    thiz->daObjC1Peach_c::~daObjC1Peach_c();    /* the D1 body, through MSVC's one destructor */
    daObjC1Peach_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
