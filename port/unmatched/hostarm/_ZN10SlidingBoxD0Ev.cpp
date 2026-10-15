/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN10SlidingBoxD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_slide_box.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daSlide_Box_c.h"

extern "C" daSlide_Box_c *_ZN13daSlide_Box_cD0Ev(daSlide_Box_c *thiz)
{
    thiz->daSlide_Box_c::~daSlide_Box_c();    /* the D1 body, through MSVC's one destructor */
    daSlide_Box_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
