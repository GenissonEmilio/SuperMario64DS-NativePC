/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN5ModelC2Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/model/Model.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 *
 * WHY IT EXISTS. The host needs the ROM's flat C2 name, and MSVC never emits
 * it: MSVC makes one constructor per class, ??0Model, which main's
 * Model::Model() in src/engine/model/Model.cpp defines (and the C1 face in
 * port/faces_sync.txt reaches). The base-object variant the ROM spells
 * _ZN5ModelC2Ev is the same body (Model has no virtual base), and host
 * callers that build a Model base subobject call it by that flat name. This
 * is that name: the one host constructor, run on the object in place. Logic
 * unchanged from the arm. */
#include "Model.h"
#include <new>

extern "C" Model *_ZN5ModelC2Ev(Model *thiz)
{
    ::new ((void *)thiz) Model();  /* the one host constructor, in place */
    return thiz;
}
