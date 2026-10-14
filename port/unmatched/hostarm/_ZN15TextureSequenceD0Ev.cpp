/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN15TextureSequenceD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/model/TextureSequence.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "TextureSequence.h"

extern "C" TextureSequence *_ZN15TextureSequenceD0Ev(TextureSequence *thiz)
{
    thiz->TextureSequence::~TextureSequence();    /* the D1 body, through MSVC's one destructor */
    TextureSequence::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
