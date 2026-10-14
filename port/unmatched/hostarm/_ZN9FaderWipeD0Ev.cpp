/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/engine/fader/_ZN9FaderWipeD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/fader/dFader_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dFdWipe_c.h"

extern "C" dFdWipe_c *_ZN9dFdWipe_cD0Ev(dFdWipe_c *thiz)
{
    thiz->dFdWipe_c::~dFdWipe_c();    /* the D1 body, through MSVC's one destructor */
    dFdWipe_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
