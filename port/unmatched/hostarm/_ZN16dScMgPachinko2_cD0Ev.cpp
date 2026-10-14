/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN16dScMgPachinko2_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/actors/dScMgPachinko2_c.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 *
 * WHY IT EXISTS. The host needs the ROM's flat D0 name, and MSVC only makes
 * one destructor: main's ~dScMgPachinko2_c() (out of line in the TU) becomes
 * the single ??1dScMgPachinko2_c. This spells the ROM's own D0 instead, the
 * three steps its body makes (overlay 6, 0x020ff444, 0x38 bytes): the store of
 * this class's own vtable, the word its literal pool names
 * (data_ov006_0213dbbc, which the host defines under that address name); the
 * base destructor, by the flat ROM name its bl reaches (_ZN11dScMgBase_cD2Ev);
 * then dScMgBase_c's inline operator delete, Memory::Deallocate with the game
 * heap word at 0x020a0eac. port/unmatched/MgPachinko2_Dtor.cpp forwards here.
 * This body is the port's one caller of the flat base destructor. Logic
 * unchanged from the arm. */
#include "dScMgPachinko2_c.h"

extern "C" void _ZN11dScMgBase_cD2Ev(void *self);
extern "C" int data_ov006_0213dbbc[];
extern "C" dScMgPachinko2_c *_ZN16dScMgPachinko2_cD0Ev(dScMgPachinko2_c *thiz)
{
    *(int **)thiz = data_ov006_0213dbbc;      /* this class's own vtable, as the ROM stores it */
    _ZN11dScMgBase_cD2Ev(thiz);               /* the base destructor, by its flat ROM name */
    dScMgPachinko2_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
