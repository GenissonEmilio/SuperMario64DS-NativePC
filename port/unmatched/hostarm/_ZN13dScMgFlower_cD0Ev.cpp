/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN13dScMgFlower_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/minigames/d_s_mg_flower.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dScMgFlower_c.h"

extern "C" dScMgFlower_c *_ZN13dScMgFlower_cD0Ev(dScMgFlower_c *thiz)
{
    thiz->dScMgFlower_c::~dScMgFlower_c();    /* the D1 body, through MSVC's one destructor */
    dScMgFlower_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
