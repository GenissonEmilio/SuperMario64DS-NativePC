/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN5KoopaD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_nknk.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "Koopa.h"

extern "C" daNknk_c *_ZN8daNknk_cD0Ev(daNknk_c *thiz)
{
    thiz->daNknk_c::~daNknk_c();    /* the D1 body, through MSVC's one destructor */
    daNknk_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
