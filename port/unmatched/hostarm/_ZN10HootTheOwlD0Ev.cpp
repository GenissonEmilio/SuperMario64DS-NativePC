/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10HootTheOwlD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_owl.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daOwl_c.h"

extern "C" daOwl_c *_ZN7daOwl_cD0Ev(daOwl_c *thiz)
{
    thiz->daOwl_c::~daOwl_c();    /* the D1 body, through MSVC's one destructor */
    daOwl_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
