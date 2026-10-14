/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN15RecRoomCupboardD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjCloset_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjCloset_c.h"

extern "C" daObjCloset_c *_ZN13daObjCloset_cD0Ev(daObjCloset_c *thiz)
{
    thiz->daObjCloset_c::~daObjCloset_c();    /* the D1 body, through MSVC's one destructor */
    daObjCloset_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
