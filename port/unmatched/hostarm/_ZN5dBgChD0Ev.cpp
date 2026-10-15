/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN5dBgChD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/collision/d_bg_ch.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dBgCh.h"

extern "C" dBgCh *_ZN5dBgChD0Ev(dBgCh *thiz)
{
    thiz->dBgCh::~dBgCh();    /* the D1 body, through MSVC's one destructor */
    dBgCh::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
