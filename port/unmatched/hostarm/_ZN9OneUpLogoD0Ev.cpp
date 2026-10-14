/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9OneUpLogoD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObj1UpLogo_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObj1UpLogo_c.h"

extern "C" daObj1UpLogo_c *_ZN14daObj1UpLogo_cD0Ev(daObj1UpLogo_c *thiz)
{
    thiz->daObj1UpLogo_c::~daObj1UpLogo_c();    /* the D1 body, through MSVC's one destructor */
    daObj1UpLogo_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
