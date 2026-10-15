/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN15TtcRotatingCubeD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_ct_rotate_block.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjCtRotateBlock_c.h"

extern "C" daObjCtRotateBlock_c *_ZN20daObjCtRotateBlock_cD0Ev(daObjCtRotateBlock_c *thiz)
{
    thiz->daObjCtRotateBlock_c::~daObjCtRotateBlock_c();    /* the D1 body, through MSVC's one destructor */
    daObjCtRotateBlock_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
