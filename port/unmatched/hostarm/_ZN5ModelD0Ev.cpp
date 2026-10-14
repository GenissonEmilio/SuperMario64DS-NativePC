/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN5ModelD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/model/Model.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 *
 * WHY IT EXISTS. The host needs the ROM's flat D0 name, and MSVC only makes
 * one destructor: it folds the Itanium D1/D0/D2 triple into the single
 * ??1Model@@QAE@XZ that main's Model::~Model() in src/engine/model/Model.cpp
 * defines (on _MSC_VER include/Model.h declares it non-virtual, with the slots
 * spelled Destructor1/Destructor0). This spells, in terms of that one host
 * destructor, what the D0 variant does: destroy through the one host destructor, then the class-specific operator delete.
 * Logic unchanged from the arm. */
#include "Model.h"

extern "C" Model *_ZN5ModelD0Ev(Model *thiz)
{
    thiz->~Model();                /* the D1 body, through the one host symbol */
    Model::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
