/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN8PaintingD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_pic_gate.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daPicGate_c.h"

extern "C" daPicGate_c *_ZN11daPicGate_cD0Ev(daPicGate_c *thiz)
{
    thiz->daPicGate_c::~daPicGate_c();    /* the D1 body, through MSVC's one destructor */
    daPicGate_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
