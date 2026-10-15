/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN7MinimapD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/ui/hud/d_map.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dMap_c.h"

extern "C" dMap_c *_ZN6dMap_cD0Ev(dMap_c *thiz)
{
    thiz->dMap_c::~dMap_c();    /* the D1 body, through MSVC's one destructor */
    dMap_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
