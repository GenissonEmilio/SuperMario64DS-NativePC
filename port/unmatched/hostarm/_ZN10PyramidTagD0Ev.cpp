/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10PyramidTagD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_dl_pyramid.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjDlPyramidDummy_c.h"

extern "C" daObjDlPyramidDummy_c *_ZN21daObjDlPyramidDummy_cD0Ev(daObjDlPyramidDummy_c *thiz)
{
    thiz->daObjDlPyramidDummy_c::~daObjDlPyramidDummy_c();    /* the D1 body, through MSVC's one destructor */
    daObjDlPyramidDummy_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
