/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/func_ov006_020ce988.cpp (port line). Its _MSC_VER arm made the
 * slot-5 Render call a __thiscall; the 2026-10 main merge (749f5799a) took main's
 * src/, which folded the function into src/actors/unit020cd744.cpp (namespace
 * s020ce988, still extern "C") without the arm. This file defines it again:
 * MAIN'S BODY, statement for statement, with only the arm's change applied
 * (main also spells the opacity call _ZN9ModelBase12ApplyOpacityEjj). The same
 * @symbol body must be parked in the TU's hostgen copy. mwccarm never sees this
 * file, so nothing here moves a ROM byte. */
#include "common.h"

namespace s020ce988 {
// @symbol func_ov006_020ce988

extern "C" {
void Matrix4x3_FromQuaternion(const void* q, struct Matrix4x3* mF);
void Matrix4x3_FromTranslation(struct Matrix4x3* m, int x, int y, int z);
void MulMat4x3Mat4x3(struct Matrix4x3* a, struct Matrix4x3* b, struct Matrix4x3* out);
void _ZN18TextureTransformer6UpdateER15ModelComponents(void* t, void* mc);
void _ZN9ModelBase12ApplyOpacityEjj(void* mb, unsigned int opacity, unsigned int unused);
}
extern "C" {extern struct Matrix4x3 data_020a0e68;}

struct VtO { int dummy[0x14/4]; void (*f)(void*, void*); };

extern "C" void func_ov006_020ce988(char* c){
    struct Matrix4x3 tmp;
    Matrix4x3_FromQuaternion(c+0x74, &tmp);
    Matrix4x3_FromTranslation(&data_020a0e68, *(int*)(c+8), *(int*)(c+0xc), *(int*)(c+0x10));
    MulMat4x3Mat4x3(&tmp, &data_020a0e68, &data_020a0e68);
    *(struct Matrix4x3*)(*(char**)(c+0x190) + 0x1c) = data_020a0e68;
    _ZN18TextureTransformer6UpdateER15ModelComponents(c+0x194, *(char**)(c+0x190) + 8);
    _ZN9ModelBase12ApplyOpacityEjj(*(void**)(c+0x190), *(unsigned char*)(c+0x9c), 0);
    {
        void* o = *(void**)(c+0x190);
        VtO* vt = *(VtO**)o;
        /* Slot 5 of this object's table is Model::Render(const Vector3 *scale), a member
           function: on the DS the call is r0 = this, r1 = scale. A plain (void*, void*)
           call hands the host body `this` in ecx and the model pointer itself as `scale`,
           so the scale was the model's first words (vptr, 0, pointer) and any sparkle
           element went out at a size set by where the exe's sections landed. */
        ((void (__thiscall *)(void*, void*))vt->f)(o, c+0x68);
    }
}
}
