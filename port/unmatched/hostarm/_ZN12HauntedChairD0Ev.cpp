/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN12HauntedChairD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daChair_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daChair_c.h"

extern "C" daChair_c *_ZN9daChair_cD0Ev(daChair_c *thiz)
{
    thiz->daChair_c::~daChair_c();    /* the D1 body, through MSVC's one destructor */
    daChair_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
