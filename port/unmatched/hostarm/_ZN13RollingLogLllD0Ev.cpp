/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN13RollingLogLllD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_fl_maruta.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjFlMaruta_c.h"

extern "C" daObjFlMaruta_c *_ZN15daObjFlMaruta_cD0Ev(daObjFlMaruta_c *thiz)
{
    thiz->daObjFlMaruta_c::~daObjFlMaruta_c();    /* the D1 body, through MSVC's one destructor */
    daObjFlMaruta_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
