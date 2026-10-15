/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN16BowserShockwavesD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_f_ring.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daFRing_c.h"

extern "C" daFRing_c *_ZN9daFRing_cD0Ev(daFRing_c *thiz)
{
    thiz->daFRing_c::~daFRing_c();    /* the D1 body, through MSVC's one destructor */
    daFRing_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
