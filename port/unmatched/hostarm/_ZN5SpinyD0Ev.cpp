/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN5SpinyD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daTgz_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daTgz_c.h"

extern "C" daTgz_c *_ZN7daTgz_cD0Ev(daTgz_c *thiz)
{
    thiz->daTgz_c::~daTgz_c();    /* the D1 body, through MSVC's one destructor */
    daTgz_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
