/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN11dScMgJump_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/dScMgJump_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dScMgJump_c.h"

extern "C" dScMgJump_c *_ZN11dScMgJump_cD0Ev(dScMgJump_c *thiz)
{
    thiz->dScMgJump_c::~dScMgJump_c();    /* the D1 body, through MSVC's one destructor */
    dScMgJump_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
