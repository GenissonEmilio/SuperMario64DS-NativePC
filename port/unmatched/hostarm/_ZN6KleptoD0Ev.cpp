/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN6KleptoD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daJango_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daJango_c.h"

extern "C" daJango_c *_ZN9daJango_cD0Ev(daJango_c *thiz)
{
    thiz->daJango_c::~daJango_c();    /* the D1 body, through MSVC's one destructor */
    daJango_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
