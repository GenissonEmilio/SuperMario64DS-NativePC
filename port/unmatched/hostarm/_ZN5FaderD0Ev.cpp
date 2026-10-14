/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/engine/fader/_ZN5FaderD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/fader/dFader_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dFader_c.h"

extern "C" dFader_c *_ZN8dFader_cD0Ev(dFader_c *thiz)
{
    thiz->dFader_c::~dFader_c();    /* the D1 body, through MSVC's one destructor */
    dFader_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
