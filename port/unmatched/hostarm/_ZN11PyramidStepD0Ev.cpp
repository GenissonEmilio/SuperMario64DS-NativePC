/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN11PyramidStepD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_dp_brock.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjDpBrock_c.h"

extern "C" daObjDpBrock_c *_ZN14daObjDpBrock_cD0Ev(daObjDpBrock_c *thiz)
{
    thiz->daObjDpBrock_c::~daObjDpBrock_c();    /* the D1 body, through MSVC's one destructor */
    daObjDpBrock_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
