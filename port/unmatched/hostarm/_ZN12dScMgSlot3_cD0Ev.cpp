/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN12dScMgSlot3_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/dScMgSlots.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dScMgSlot3_c.h"

extern "C" dScMgSlot3_c *_ZN12dScMgSlot3_cD0Ev(dScMgSlot3_c *thiz)
{
    thiz->dScMgSlot3_c::~dScMgSlot3_c();    /* the D1 body, through MSVC's one destructor */
    dScMgSlot3_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
