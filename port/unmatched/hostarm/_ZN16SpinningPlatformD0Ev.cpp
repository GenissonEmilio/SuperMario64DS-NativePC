/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN16SpinningPlatformD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjCtMecha11_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjCtMecha11_c.h"

extern "C" daObjCtMecha11_c *_ZN16daObjCtMecha11_cD0Ev(daObjCtMecha11_c *thiz)
{
    thiz->daObjCtMecha11_c::~daObjCtMecha11_c();    /* the D1 body, through MSVC's one destructor */
    daObjCtMecha11_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
