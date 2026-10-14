/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10SlidingIceD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjSlIceBlock_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjSlIceBlock_c.h"

extern "C" daObjSlIceBlock_c *_ZN17daObjSlIceBlock_cD0Ev(daObjSlIceBlock_c *thiz)
{
    thiz->daObjSlIceBlock_c::~daObjSlIceBlock_c();    /* the D1 body, through MSVC's one destructor */
    daObjSlIceBlock_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
