/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN7WigglerD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daHanachan_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daHanachan_c.h"

extern "C" daHanachan_c *_ZN12daHanachan_cD0Ev(daHanachan_c *thiz)
{
    thiz->daHanachan_c::~daHanachan_c();    /* the D1 body, through MSVC's one destructor */
    daHanachan_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
