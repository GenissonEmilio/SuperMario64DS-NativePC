/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN3MrID0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_eykn.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daEykn_c.h"

extern "C" daEykn_c *_ZN8daEykn_cD0Ev(daEykn_c *thiz)
{
    thiz->daEykn_c::~daEykn_c();    /* the D1 body, through MSVC's one destructor */
    daEykn_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
