/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9RabbitKeyD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObj_Mip_Key_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObj_Mip_Key_c.h"

extern "C" daObj_Mip_Key_c *_ZN15daObj_Mip_Key_cD0Ev(daObj_Mip_Key_c *thiz)
{
    thiz->daObj_Mip_Key_c::~daObj_Mip_Key_c();    /* the D1 body, through MSVC's one destructor */
    daObj_Mip_Key_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
