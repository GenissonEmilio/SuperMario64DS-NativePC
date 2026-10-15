/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9MontyMoleD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_choropu.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daChoropu_c.h"

extern "C" daChoropu_c *_ZN11daChoropu_cD0Ev(daChoropu_c *thiz)
{
    thiz->daChoropu_c::~daChoropu_c();    /* the D1 body, through MSVC's one destructor */
    daChoropu_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
