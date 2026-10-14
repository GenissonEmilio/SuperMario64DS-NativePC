/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/engine/fader/_ZN15FaderBrightnessD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/fader/dFader_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dFdBrightness_c.h"

extern "C" dFdBrightness_c *_ZN15dFdBrightness_cD0Ev(dFdBrightness_c *thiz)
{
    thiz->dFdBrightness_c::~dFdBrightness_c();    /* the D1 body, through MSVC's one destructor */
    dFdBrightness_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
