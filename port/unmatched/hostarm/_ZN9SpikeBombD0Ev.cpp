/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9SpikeBombD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_kirai.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daKirai_c.h"

extern "C" daKirai_c *_ZN9daKirai_cD0Ev(daKirai_c *thiz)
{
    thiz->daKirai_c::~daKirai_c();    /* the D1 body, through MSVC's one destructor */
    daKirai_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
