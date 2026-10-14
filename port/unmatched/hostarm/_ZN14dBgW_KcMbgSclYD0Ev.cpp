/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN14dBgW_KcMbgSclYD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/collision/dBgW_KcMbgSclY.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dBgW_KcMbgSclY.h"

extern "C" dBgW_KcMbgSclY *_ZN14dBgW_KcMbgSclYD0Ev(dBgW_KcMbgSclY *thiz)
{
    thiz->dBgW_KcMbgSclY::~dBgW_KcMbgSclY();    /* the D1 body, through MSVC's one destructor */
    dBgW_KcMbgSclY::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
