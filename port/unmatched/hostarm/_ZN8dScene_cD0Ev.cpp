/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN8dScene_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/stages/d_scene.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. Renamed by address to main's
 * _ZN8dScene_cD0Ev / class dScene_c.
 * The flat D1 reaches ??1dScene_c@@UAE@XZ through port/faces_sync.txt's alias row. */
#include "dScene_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D0 NAME, AND MSVC NEVER EMITS IT. This arm
 * makes the D0's two calls: the D1 body through its flat ROM name, then the
 * class-specific operator delete. */
extern "C" void _ZN8dScene_cD1Ev(void *self);
extern "C" dScene_c *_ZN8dScene_cD0Ev(dScene_c *thiz)
{
    _ZN8dScene_cD1Ev(thiz);           /* the D1 body, through its flat ROM name */
    dScene_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
