/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN18TextureTransformerD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/model/TextureTransformer.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "TextureTransformer.h"

extern "C" TextureTransformer *_ZN18TextureTransformerD0Ev(TextureTransformer *thiz)
{
    thiz->TextureTransformer::~TextureTransformer();    /* the D1 body, through MSVC's one destructor */
    TextureTransformer::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
