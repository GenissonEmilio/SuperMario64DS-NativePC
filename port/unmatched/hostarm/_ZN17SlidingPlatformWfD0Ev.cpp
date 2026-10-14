/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN17SlidingPlatformWfD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjSimpleLift_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjSimpleLift_c.h"

extern "C" daObjSimpleLift_c *_ZN17daObjSimpleLift_cD0Ev(daObjSimpleLift_c *thiz)
{
    thiz->daObjSimpleLift_c::~daObjSimpleLift_c();    /* the D1 body, through MSVC's one destructor */
    daObjSimpleLift_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
