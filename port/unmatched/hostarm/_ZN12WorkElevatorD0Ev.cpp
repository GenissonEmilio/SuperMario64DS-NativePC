/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN12WorkElevatorD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/daObjCvNewsLift_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "daObjCvNewsLift_c.h"

extern "C" daObjCvNewsLift_c *_ZN17daObjCvNewsLift_cD0Ev(daObjCvNewsLift_c *thiz)
{
    thiz->daObjCvNewsLift_c::~daObjCvNewsLift_c();    /* the D1 body, through MSVC's one destructor */
    daObjCvNewsLift_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
