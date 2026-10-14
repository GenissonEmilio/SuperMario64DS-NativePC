/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10LavaSeesawD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjFl_Seesaw_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjFl_Seesaw_c.h"

extern "C" daObjFl_Seesaw_c *_ZN16daObjFl_Seesaw_cD0Ev(daObjFl_Seesaw_c *thiz)
{
    thiz->daObjFl_Seesaw_c::~daObjFl_Seesaw_c();    /* the D1 body, through MSVC's one destructor */
    daObjFl_Seesaw_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
