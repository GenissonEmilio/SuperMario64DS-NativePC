/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN7TornadoD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_tor.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. Renamed by address to main's
 * _ZN7daTor_cD0Ev / class daTor_c.
 * Main writes ~daTor_c() out of line in its TU, but MSVC still emits no flat name; the flat D1 is defined by port/unmatched/Tornado_HostSites.cpp. */
#include "daTor_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D0 NAME, AND MSVC NEVER EMITS IT. This arm
 * makes the D0's two calls: the D1 body through its flat ROM name, then the
 * class-specific operator delete. */
extern "C" void _ZN7daTor_cD1Ev(void *self);
extern "C" daTor_c *_ZN7daTor_cD0Ev(daTor_c *thiz)
{
    _ZN7daTor_cD1Ev(thiz);           /* the D1 body, through its flat ROM name */
    daTor_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
