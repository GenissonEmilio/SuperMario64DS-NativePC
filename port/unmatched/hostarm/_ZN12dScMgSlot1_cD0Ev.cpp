/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN12dScMgSlot1_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/dScMgSlots.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dScMgSlot1_c.h"

extern "C" dScMgSlot1_c *_ZN12dScMgSlot1_cD0Ev(dScMgSlot1_c *thiz)
{
    thiz->dScMgSlot1_c::~dScMgSlot1_c();    /* the D1 body, through MSVC's one destructor */
    dScMgSlot1_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
