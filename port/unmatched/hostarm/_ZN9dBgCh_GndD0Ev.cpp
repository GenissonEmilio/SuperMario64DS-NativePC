/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9dBgCh_GndD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/collision/d_bg_ch_gnd.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dBgCh_Gnd.h"

extern "C" dBgCh_Gnd *_ZN9dBgCh_GndD0Ev(dBgCh_Gnd *thiz)
{
    thiz->dBgCh_Gnd::~dBgCh_Gnd();    /* the D1 body, through MSVC's one destructor */
    dBgCh_Gnd::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
