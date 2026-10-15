/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN15TtcRotatingGearD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_ct_mecha08.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjCtMecha08_c.h"

extern "C" daObjCtMecha08_c *_ZN16daObjCtMecha08_cD0Ev(daObjCtMecha08_c *thiz)
{
    thiz->daObjCtMecha08_c::~daObjCtMecha08_c();    /* the D1 body, through MSVC's one destructor */
    daObjCtMecha08_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
