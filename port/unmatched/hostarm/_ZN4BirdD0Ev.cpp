/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN4BirdD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_s_bird_head.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daSBird_c.h"

extern "C" daSBird_c *_ZN9daSBird_cD0Ev(daSBird_c *thiz)
{
    thiz->daSBird_c::~daSBird_c();    /* the D1 body, through MSVC's one destructor */
    daSBird_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
