/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN5SharkD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daShark_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daShark_c.h"

extern "C" daShark_c *_ZN9daShark_cD0Ev(daShark_c *thiz)
{
    thiz->daShark_c::~daShark_c();    /* the D1 body, through MSVC's one destructor */
    daShark_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
