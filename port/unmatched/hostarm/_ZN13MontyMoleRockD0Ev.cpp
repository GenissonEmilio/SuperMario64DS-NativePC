/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN13MontyMoleRockD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daChoropu_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daChoro_Rock_c.h"

extern "C" daChoro_Rock_c *_ZN14daChoro_Rock_cD0Ev(daChoro_Rock_c *thiz)
{
    thiz->daChoro_Rock_c::~daChoro_Rock_c();    /* the D1 body, through MSVC's one destructor */
    daChoro_Rock_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
