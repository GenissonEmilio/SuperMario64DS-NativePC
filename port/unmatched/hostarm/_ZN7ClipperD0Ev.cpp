/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN7ClipperD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/collision/d_clipper.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dClipper.h"

extern "C" dClipper *_ZN8dClipperD0Ev(dClipper *thiz)
{
    thiz->dClipper::~dClipper();    /* the D1 body, through MSVC's one destructor */
    dClipper::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
