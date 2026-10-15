/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10BrickBlockD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_block_item_tag.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjBlockItemTag_c.h"

extern "C" daObjBlockItemTag_c *_ZN19daObjBlockItemTag_cD0Ev(daObjBlockItemTag_c *thiz)
{
    thiz->daObjBlockItemTag_c::~daObjBlockItemTag_c();    /* the D1 body, through MSVC's one destructor */
    daObjBlockItemTag_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
