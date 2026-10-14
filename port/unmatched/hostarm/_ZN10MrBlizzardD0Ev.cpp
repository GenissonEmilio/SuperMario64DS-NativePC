/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10MrBlizzardD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daSnowman_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daSnowman_c.h"

extern "C" daSnowman_c *_ZN11daSnowman_cD0Ev(daSnowman_c *thiz)
{
    thiz->daSnowman_c::~daSnowman_c();    /* the D1 body, through MSVC's one destructor */
    daSnowman_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
