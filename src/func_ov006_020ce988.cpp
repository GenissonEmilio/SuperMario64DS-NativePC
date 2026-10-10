//cpp
// @symbol func_ov006_020ce988
/* recovered: shared common types */
#include "common.h"

extern "C" {
void Matrix4x3_FromQuaternion(const void* q, struct Matrix4x3* mF);
void Matrix4x3_FromTranslation(struct Matrix4x3* m, int x, int y, int z);
void MulMat4x3Mat4x3(struct Matrix4x3* a, struct Matrix4x3* b, struct Matrix4x3* out);
void _ZN18TextureTransformer6UpdateER15ModelComponents(void* t, void* mc);
void _ZN9ModelBase12ApplyOpacityEj(void* mb, unsigned int op, int z);
}
extern struct Matrix4x3 data_020a0e68;

struct VtO { int dummy[0x14/4]; void (*f)(void*, void*); };

extern "C" void func_ov006_020ce988(char* c){
    struct Matrix4x3 tmp;
    Matrix4x3_FromQuaternion(c+0x74, &tmp);
    Matrix4x3_FromTranslation(&data_020a0e68, *(int*)(c+8), *(int*)(c+0xc), *(int*)(c+0x10));
    MulMat4x3Mat4x3(&tmp, &data_020a0e68, &data_020a0e68);
    *(struct Matrix4x3*)(*(char**)(c+0x190) + 0x1c) = data_020a0e68;
    _ZN18TextureTransformer6UpdateER15ModelComponents(c+0x194, *(char**)(c+0x190) + 8);
    _ZN9ModelBase12ApplyOpacityEj(*(void**)(c+0x190), *(unsigned char*)(c+0x9c), 0);
    {
        void* o = *(void**)(c+0x190);
        VtO* vt = *(VtO**)o;
#ifdef _MSC_VER
        /* Slot 5 of this object's table is Model::Render(const Vector3 *scale), a member
           function: on the DS the call is r0 = this, r1 = scale. A plain (void*, void*)
           call hands the host body `this` in ecx and the model pointer itself as `scale`,
           so the scale was the model's first words (vptr, 0, pointer) and any sparkle
           element went out at a size set by where the exe's sections landed. */
        ((void (__thiscall *)(void*, void*))vt->f)(o, c+0x68);
#else
        vt->f(o, c+0x68);
#endif
    }
}
