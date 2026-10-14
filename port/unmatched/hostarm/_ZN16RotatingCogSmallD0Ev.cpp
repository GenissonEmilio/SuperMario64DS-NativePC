/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN16RotatingCogSmallD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjCtMecha10_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjCtMecha10_c.h"

extern "C" daObjCtMecha10_c *_ZN16daObjCtMecha10_cD0Ev(daObjCtMecha10_c *thiz)
{
    thiz->daObjCtMecha10_c::~daObjCtMecha10_c();    /* the D1 body, through MSVC's one destructor */
    daObjCtMecha10_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
