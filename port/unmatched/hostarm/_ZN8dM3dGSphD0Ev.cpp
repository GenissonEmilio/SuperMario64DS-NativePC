/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN8dM3dGSphD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/collision/d_m3d_g_sph.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dM3dGSph.h"

extern "C" dM3dGSph *_ZN8dM3dGSphD0Ev(dM3dGSph *thiz)
{
    thiz->dM3dGSph::~dM3dGSph();    /* the D1 body, through MSVC's one destructor */
    dM3dGSph::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
