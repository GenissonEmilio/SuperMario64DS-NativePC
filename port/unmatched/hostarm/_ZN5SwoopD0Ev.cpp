/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN5SwoopD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_basabasa.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daBasabasa_c.h"

extern "C" daBasabasa_c *_ZN12daBasabasa_cD0Ev(daBasabasa_c *thiz)
{
    thiz->daBasabasa_c::~daBasabasa_c();    /* the D1 body, through MSVC's one destructor */
    daBasabasa_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
