/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN14TTC_MovingBeamD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_ct_mecha09.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjCtMecha09_c.h"

extern "C" daObjCtMecha09_c *_ZN16daObjCtMecha09_cD0Ev(daObjCtMecha09_c *thiz)
{
    thiz->daObjCtMecha09_c::~daObjCtMecha09_c();    /* the D1 body, through MSVC's one destructor */
    daObjCtMecha09_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
