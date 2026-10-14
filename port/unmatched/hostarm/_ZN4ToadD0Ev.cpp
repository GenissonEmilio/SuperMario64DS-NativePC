/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN4ToadD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_kinopio.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daKinopio_c.h"

extern "C" daKinopio_c *_ZN11daKinopio_cD0Ev(daKinopio_c *thiz)
{
    thiz->daKinopio_c::~daKinopio_c();    /* the D1 body, through MSVC's one destructor */
    daKinopio_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
