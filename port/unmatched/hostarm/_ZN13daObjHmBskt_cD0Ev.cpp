/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN13daObjHmBskt_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_hm_bskt.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjHmBskt_c.h"

extern "C" daObjHmBskt_c *_ZN13daObjHmBskt_cD0Ev(daObjHmBskt_c *thiz)
{
    thiz->daObjHmBskt_c::~daObjHmBskt_c();    /* the D1 body, through MSVC's one destructor */
    daObjHmBskt_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
