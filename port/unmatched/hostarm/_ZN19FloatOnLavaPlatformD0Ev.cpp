/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN19FloatOnLavaPlatformD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjFl_Block_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjFl_Block_c.h"

extern "C" daObjFl_Block_c *_ZN15daObjFl_Block_cD0Ev(daObjFl_Block_c *thiz)
{
    thiz->daObjFl_Block_c::~daObjFl_Block_c();    /* the D1 body, through MSVC's one destructor */
    daObjFl_Block_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
