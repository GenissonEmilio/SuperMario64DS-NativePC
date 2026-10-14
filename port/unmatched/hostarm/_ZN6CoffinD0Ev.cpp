/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN6CoffinD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjCasket_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. Renamed by address to main's
 * _ZN13daObjCasket_cD0Ev / class daObjCasket_c.
 * The flat D1 is defined by port/unmatched/hostarm/_ZN6CoffinD1Ev.cpp. */
#include "daObjCasket_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D0 NAME, AND MSVC NEVER EMITS IT. This arm
 * makes the D0's two calls: the D1 body through its flat ROM name, then the
 * class-specific operator delete. */
extern "C" void _ZN13daObjCasket_cD1Ev(void *self);
extern "C" daObjCasket_c *_ZN13daObjCasket_cD0Ev(daObjCasket_c *thiz)
{
    _ZN13daObjCasket_cD1Ev(thiz);           /* the D1 body, through its flat ROM name */
    daObjCasket_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
