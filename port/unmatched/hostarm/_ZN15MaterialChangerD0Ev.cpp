/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN15MaterialChangerD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/model/MaterialChanger.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "MaterialChanger.h"

extern "C" MaterialChanger *_ZN15MaterialChangerD0Ev(MaterialChanger *thiz)
{
    thiz->MaterialChanger::~MaterialChanger();    /* the D1 body, through MSVC's one destructor */
    MaterialChanger::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
