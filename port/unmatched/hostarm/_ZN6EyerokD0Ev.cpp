/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN6EyerokD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/Eyerok.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "Eyerok.h"

extern "C" Eyerok *_ZN6EyerokD0Ev(Eyerok *thiz)
{
    thiz->Eyerok::~Eyerok();    /* the D1 body, through MSVC's one destructor */
    Eyerok::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
