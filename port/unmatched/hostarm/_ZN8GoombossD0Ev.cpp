/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN8GoombossD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daKuriKing_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daKuriKing_c.h"

extern "C" daKuriKing_c *_ZN12daKuriKing_cD0Ev(daKuriKing_c *thiz)
{
    thiz->daKuriKing_c::~daKuriKing_c();    /* the D1 body, through MSVC's one destructor */
    daKuriKing_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
