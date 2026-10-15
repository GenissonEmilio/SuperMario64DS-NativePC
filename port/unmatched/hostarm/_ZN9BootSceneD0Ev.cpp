/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9BootSceneD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/stages/d_s_boot.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dScBoot_c.h"

extern "C" dScBoot_c *_ZN9dScBoot_cD0Ev(dScBoot_c *thiz)
{
    thiz->dScBoot_c::~dScBoot_c();    /* the D1 body, through MSVC's one destructor */
    dScBoot_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
