/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN12EnemySpawnerD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_e_switch.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "EnemySpawner.h"

extern "C" daECreate_c *_ZN11daECreate_cD0Ev(daECreate_c *thiz)
{
    thiz->daECreate_c::~daECreate_c();    /* the D1 body, through MSVC's one destructor */
    daECreate_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
