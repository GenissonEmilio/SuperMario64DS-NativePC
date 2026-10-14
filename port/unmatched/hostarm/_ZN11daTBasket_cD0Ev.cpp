/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/game/actors/daTBasket_c/_ZN11daTBasket_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daTrs_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daTBasket_c.h"

extern "C" daTBasket_c *_ZN11daTBasket_cD0Ev(daTBasket_c *thiz)
{
    thiz->daTBasket_c::~daTBasket_c();    /* the D1 body, through MSVC's one destructor */
    daTBasket_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
