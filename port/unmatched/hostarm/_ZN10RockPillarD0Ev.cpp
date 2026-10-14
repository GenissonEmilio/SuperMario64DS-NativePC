/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10RockPillarD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjKi_Hasira_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjKi_Hasira_c.h"

extern "C" daObjKi_Hasira_c *_ZN16daObjKi_Hasira_cD0Ev(daObjKi_Hasira_c *thiz)
{
    thiz->daObjKi_Hasira_c::~daObjKi_Hasira_c();    /* the D1 body, through MSVC's one destructor */
    daObjKi_Hasira_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
