/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN11ShadowModelD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/model/dExtShadowModel_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dExtShadowModel_c.h"

extern "C" dExtShadowModel_c *_ZN17dExtShadowModel_cD0Ev(dExtShadowModel_c *thiz)
{
    thiz->dExtShadowModel_c::~dExtShadowModel_c();    /* the D1 body, through MSVC's one destructor */
    dExtShadowModel_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
