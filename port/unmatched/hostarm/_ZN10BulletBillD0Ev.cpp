/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10BulletBillD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_klr.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daKlr_c.h"

extern "C" daKlr_c *_ZN7daKlr_cD0Ev(daKlr_c *thiz)
{
    thiz->daKlr_c::~daKlr_c();    /* the D1 body, through MSVC's one destructor */
    daKlr_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
