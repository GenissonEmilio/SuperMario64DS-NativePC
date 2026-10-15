/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN7dWipe_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/runtime/graphics/fader/d_wipe.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dWipe_c.h"

extern "C" dWipe_c *_ZN7dWipe_cD0Ev(dWipe_c *thiz)
{
    thiz->dWipe_c::~dWipe_c();    /* the D1 body, through MSVC's one destructor */
    dWipe_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
