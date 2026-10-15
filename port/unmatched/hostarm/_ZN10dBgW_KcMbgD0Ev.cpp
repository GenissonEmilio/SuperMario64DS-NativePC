/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10dBgW_KcMbgD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/collision/d_bg_w_kc_mbg.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dBgW_KcMbg.h"

extern "C" dBgW_KcMbg *_ZN10dBgW_KcMbgD0Ev(dBgW_KcMbg *thiz)
{
    thiz->dBgW_KcMbg::~dBgW_KcMbg();    /* the D1 body, through MSVC's one destructor */
    dBgW_KcMbg::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
