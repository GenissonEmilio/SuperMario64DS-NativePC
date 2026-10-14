/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN17ExtendingPlatformD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjKm2_Nobiru_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjKm2_Nobiru_c.h"

extern "C" daObjKm2_Nobiru_c *_ZN17daObjKm2_Nobiru_cD0Ev(daObjKm2_Nobiru_c *thiz)
{
    thiz->daObjKm2_Nobiru_c::~daObjKm2_Nobiru_c();    /* the D1 body, through MSVC's one destructor */
    daObjKm2_Nobiru_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
