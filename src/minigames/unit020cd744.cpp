//cpp
/* ov006 stylus-drawn trampolines, 0x020cd744..0x020d1018.
 *
 * The run sits between dMgTrmpln2Mario_c and dScMgAmida_c. No RTTI
 * in it. A line slot is a 4x4 patch: positions at +0x5c, normals at
 * +0x1dc, packed normals at +0x2dc. Four slots of 0x32c bytes sit at
 * data_ov006_02140990. defer_codegen off keeps ROM order.
 *
 * leftovers:
 * - Address names stay. Each function keeps its namespace, because
 *   the local structs and externs disagree.
 * - TrmpItem_Motion4 keeps opt_propagation off.
 *   TrmpLine_ComputeNormals keeps opt_strength_reduction off.
 *   TrmpLine_PushJumper keeps opt_common_subs off and opt_propagation off.
 * - Vector3 locals emit an empty destructor, licensed as a deadstrip
 *   duplicate of the arm9 copy. Spare4 in TrmpLine_Build emits
 *   another, deadstripped as compiler-only output.
 * - The slot fields stay offsets. These bodies do not share one
 *   struct for the 0x32c record.
 */

#include "types.h"
#include "decl_common.h"
#include "common.h"

#pragma defer_codegen off

extern int ApproachLinear(int &, int, int);
struct BMD_File; struct BTA_File;
struct ModelBase { void SetFile(BMD_File*, int, int); };
struct Model : ModelBase { void SetPolygonID(int); static unsigned int LoadTextureToVram(char *, unsigned int); };
struct TextureTransformer { static void Prepare(BMD_File&, BTA_File&); void SetFile(BTA_File&, int, int, unsigned int); };

namespace s020cd744 {
extern "C" extern void _Z14ApproachLinearRiii(int* a, int b, int c);
extern "C" extern int _Z15ApproachLinear2Riii(int* a, int b, int c);
extern "C" {extern int data_ov006_02140828;}

extern "C" void TrmpItem_GrowScale(char* c) {
    _Z14ApproachLinearRiii((int*)(c + 0x68), 0x1200, 0x80);
    _Z14ApproachLinearRiii((int*)(c + 0x70), 0x1200, 0x80);
    _Z14ApproachLinearRiii((int*)(c + 0x6c), 0x20000, 0x400);
    if (_Z15ApproachLinear2Riii((int*)(c + 0x9c), 0, 1) == 0) return;
    *(int*)(c + 0x84) = 0;
    _Z14ApproachLinearRiii(&data_ov006_02140828, 0, 1);
}
}

namespace s020cd7b8 {
extern "C" extern void Camera_ProjectToScreen(void* a, void* b, int* c, int* d);
extern "C" extern int Mg_GetTopScreenYOffset(void);
extern "C" extern void Jump2Popup_Spawn(int a, int b, int c);
extern "C" {extern void* data_ov006_02141a50;}
extern "C" {extern void* data_ov006_02141a40;}
struct P2 { int w[2]; };
extern "C" {extern struct P2 pmf_TrmpItem_GrowScale;}

extern "C" void TrmpItem_ProjectToScreen(char* c, int arg1)
{
    int v0, v1;
    if (*(int*)(c + 0xc) > 0) {
        Camera_ProjectToScreen(data_ov006_02141a50, (void*)(c + 8), &v0, &v1);
        v1 = v1 - (Mg_GetTopScreenYOffset() + 0xc0);
    } else {
        Camera_ProjectToScreen(data_ov006_02141a40, (void*)(c + 8), &v0, &v1);
    }
    Jump2Popup_Spawn(v0 << 0xc, v1 << 0xc, (short)arg1);
    *(struct P2*)c = pmf_TrmpItem_GrowScale;
}
}

namespace s020cd864 {
// @symbol TrmpItem_Motion7
extern "C" extern void *_ZN7Vector3D1Ev(void *object);

extern "C" extern void func_020731dc(void *object, void *destructor, void **node);

extern "C" void TrmpItem_Motion7(char* arg) {
  struct Vector3 a;
  struct Vector3 b;
  struct Vector3 c;
  if ((data_ov006_02140820 & 1) == 0) {
    data_ov006_02140938.x = 0;
    data_ov006_02140938.y = 0x1000;
    data_ov006_02140938.z = 0;
    func_020731dc(&data_ov006_02140938, (void *)_ZN7Vector3D1Ev, &data_ov006_02140920);
    data_ov006_02140820 |= 1;
  }
  if ((data_ov006_02140840 & 1) == 0) {
    data_ov006_02140884.x = 0;
    data_ov006_02140884.y = 0;
    data_ov006_02140884.z = 0x1000;
    func_020731dc(&data_ov006_02140884, (void *)_ZN7Vector3D1Ev, &data_ov006_0214086c);
    data_ov006_02140840 |= 1;
  }
  func_0203cc28((int*)(arg + 0x38), 0x100);
  func_0203ce80(&a, (struct Vector3*)(arg + 0x38));
  func_0203cf00(&b, (struct Vector3*)(arg + 0x38), &data_ov006_02140884);
  *(int*)(arg + 0x44) = b.x;
  *(int*)(arg + 0x48) = b.y;
  *(int*)(arg + 0x4c) = b.z;
  func_0203ce80(&c, (struct Vector3*)(arg + 0x44));
  Quaternion_FromVector3((int*)(arg + 0x74), &data_ov006_02140938, (struct Vector3*)(arg + 0x38));
  Quaternion_Normalize((int*)(arg + 0x74));
  TrmpItem_Motion6(arg);
}
}

namespace s020cd98c {
struct S{int w[2];};
extern "C" {extern struct S pmf_TrmpItem_Motion7;}
extern "C" void TrmpItem_SetMotion7(int *c){
  *(short*)((char*)c+0x9a)=0;
  *(struct S*)c=pmf_TrmpItem_Motion7;
}
}

namespace s020cd9b0 {
typedef struct { int x, y, z; } Vec3;
extern "C" extern void func_020731dc(void *object, void *destructor, void **node);
extern "C" extern void func_0203cc28(int *p, int angle);
extern "C" extern void func_0203ce80(Vec3* dst, Vec3* src);
extern "C" extern void func_0203cf00(Vec3 *out, Vec3 *a, Vec3 *b);
extern "C" extern void Quaternion_FromVector3(int* q, Vec3* axis, Vec3* v);
extern "C" extern void Quaternion_Normalize(int *q);
extern "C" extern void *_ZN7Vector3D1Ev(void *object);

extern "C" {extern int data_ov006_02140834;}
extern "C" {extern Vec3 data_ov006_021408c0;}
extern "C" {extern int data_ov006_021408b4;}
extern "C" {extern int data_ov006_0214083c;}
extern "C" {extern Vec3 data_ov006_021408e4;}
extern "C" {extern int data_ov006_021408d8;}

extern "C" void TrmpItem_Motion8(char* self)
{
    Vec3 a, b, c, d;

    if (!(data_ov006_02140834 & 1)) {
        data_ov006_021408c0.x = 0;
        data_ov006_021408c0.y = 0x1000;
        data_ov006_021408c0.z = 0;
        func_020731dc(&data_ov006_021408c0, (void *)&_ZN7Vector3D1Ev, (void**)&data_ov006_021408b4);
        data_ov006_02140834 |= 1;
    }
    if (!(data_ov006_0214083c & 1)) {
        data_ov006_021408e4.x = 0;
        data_ov006_021408e4.y = 0;
        data_ov006_021408e4.z = 0x1000;
        func_020731dc(&data_ov006_021408e4, (void *)&_ZN7Vector3D1Ev, (void**)&data_ov006_021408d8);
        data_ov006_0214083c |= 1;
    }
    func_0203cc28((int*)(self + 0x38), 0x100);
    func_0203ce80(&a, (Vec3*)(self + 0x38));
    func_0203cf00(&b, (Vec3*)(self + 0x38), &data_ov006_021408e4);
    *(int*)(self + 0x44) = b.x;
    *(int*)(self + 0x48) = b.y;
    *(int*)(self + 0x4c) = b.z;
    func_0203ce80(&c, (Vec3*)(self + 0x44));
    Quaternion_FromVector3((int*)(self + 0x74), &data_ov006_021408c0, (Vec3*)(self + 0x38));
    Quaternion_Normalize((int*)(self + 0x74));
}
}

namespace s020cdad0 {
struct S { int w[2]; };
extern "C" {extern struct S pmf_TrmpItem_Motion8;}
extern "C" void TrmpItem_SetMotion8(char *p) { *(struct S *)(p + 0x0) = pmf_TrmpItem_Motion8; }
}

namespace s020cdaec {
// @symbol TrmpItem_Motion5
extern "C" extern void *_ZN7Vector3D1Ev(void *object);

extern "C" extern void func_020731dc(void *object, void *destructor, void **node);

extern "C" void TrmpItem_Motion5(char* arg) {
  struct Vector3 a;
  struct Vector3 b;
  struct Vector3 c;
  if ((data_ov006_02140808 & 1) == 0) {
    data_ov006_02140860.x = 0;
    data_ov006_02140860.y = 0x1000;
    data_ov006_02140860.z = 0;
    func_020731dc(&data_ov006_02140860, (void *)_ZN7Vector3D1Ev, &data_ov006_0214095c);
    data_ov006_02140808 |= 1;
  }
  if ((data_ov006_02140810 & 1) == 0) {
    data_ov006_02140890.x = 0;
    data_ov006_02140890.y = 0;
    data_ov006_02140890.z = 0x1000;
    func_020731dc(&data_ov006_02140890, (void *)_ZN7Vector3D1Ev, &data_ov006_02140878);
    data_ov006_02140810 |= 1;
  }
  func_0203cc28((int*)(arg + 0x38), 0x100);
  func_0203ce80(&a, (struct Vector3*)(arg + 0x38));
  func_0203cf00(&b, (struct Vector3*)(arg + 0x38), &data_ov006_02140890);
  *(int*)(arg + 0x44) = b.x;
  *(int*)(arg + 0x48) = b.y;
  *(int*)(arg + 0x4c) = b.z;
  func_0203ce80(&c, (struct Vector3*)(arg + 0x44));
  Quaternion_FromVector3((int*)(arg + 0x74), &data_ov006_02140860, (struct Vector3*)(arg + 0x38));
  Quaternion_Normalize((int*)(arg + 0x74));
  TrmpItem_Motion4(arg);
}
}

namespace s020cdc14 {
struct S{int w[2];}; extern "C" {extern struct S pmf_TrmpItem_Motion5;}
extern "C" void TrmpItem_SetMotion5(char*c){*(short*)(c+0x9a)=0x2000;*(struct S*)c=pmf_TrmpItem_Motion5;}
}

namespace s020cdc38 {
extern "C" void TrmpItem_Motion6(void *arg0)
{
    *(u16 *)((char *)arg0 + 0x9a) += 0x100;
    if (*(u16 *)((char *)arg0 + 0x9a) & 0x8000) {
        *(s32 *)((char *)arg0 + 0x30) = 0xc00;
    } else {
        *(s32 *)((char *)arg0 + 0x30) = -0xc00;
    }
}
}

namespace s020cdc68 {
struct S{int w[2];}; extern "C" {extern struct S pmf_TrmpItem_Motion6;}
extern "C" void TrmpItem_SetMotion6(char*c){*(short*)(c+0x9a)=0;*(struct S*)c=pmf_TrmpItem_Motion6;}
}

namespace s020cdc8c {
extern "C" extern int _ZN4cstd4fdivEii(int a, int b);

#pragma push
#pragma opt_propagation off
extern "C" void TrmpItem_Motion4(char *self)
{
    int d;
    int v;
    int base = 0x400;
    u16 *pa = (u16 *)(self + 0x9a);

    *pa = *pa + 0x40;

    d = *(s32 *)(self + 0xc) - 0x74000;
    if (d < 0)
        d = -d;
    v = (_ZN4cstd4fdivEii(d, 0x4c000) >> 3) + base;

    if (*(u16 *)(self + 0x9a) & 0x4000)
        *(s32 *)(self + 0x2c) = v;
    else
        *(s32 *)(self + 0x2c) = -v;
}
#pragma pop
}

namespace s020cdce4 {
struct S{int w[2];}; extern "C" {extern struct S pmf_TrmpItem_Motion4;}
extern "C" void TrmpItem_SetMotion4(char*c){*(short*)(c+0x9a)=0x2000;*(struct S*)c=pmf_TrmpItem_Motion4;}
}

namespace s020cdd08 {
typedef struct { int x, y, z; } Vec3;
extern "C" extern void func_020731dc(void *object, void *destructor, void **node);
extern "C" extern void func_0203cc28(int *p, int angle);
extern "C" extern void func_0203ce80(Vec3* dst, Vec3* src);
extern "C" extern void func_0203cf00(Vec3 *out, Vec3 *a, Vec3 *b);
extern "C" extern void Quaternion_FromVector3(int* q, Vec3* axis, Vec3* v);
extern "C" extern void Quaternion_Normalize(int *q);
extern "C" extern void *_ZN7Vector3D1Ev(void *object);

extern "C" {extern int data_ov006_02140824;}
extern "C" {extern Vec3 data_ov006_021408fc;}
extern "C" {extern int data_ov006_021408f0;}
extern "C" {extern int data_ov006_0214080c;}
extern "C" {extern Vec3 data_ov006_02140914;}
extern "C" {extern int data_ov006_02140908;}

extern "C" void TrmpItem_Motion3(char* self)
{
    Vec3 a, b, c, d;

    if (!(data_ov006_02140824 & 1)) {
        data_ov006_021408fc.x = 0;
        data_ov006_021408fc.y = 0x1000;
        data_ov006_021408fc.z = 0;
        func_020731dc(&data_ov006_021408fc, (void *)&_ZN7Vector3D1Ev, (void**)&data_ov006_021408f0);
        data_ov006_02140824 |= 1;
    }
    if (!(data_ov006_0214080c & 1)) {
        data_ov006_02140914.x = 0;
        data_ov006_02140914.y = 0;
        data_ov006_02140914.z = 0x1000;
        func_020731dc(&data_ov006_02140914, (void *)&_ZN7Vector3D1Ev, (void**)&data_ov006_02140908);
        data_ov006_0214080c |= 1;
    }
    func_0203cc28((int*)(self + 0x38), 0x100);
    func_0203ce80(&a, (Vec3*)(self + 0x38));
    func_0203cf00(&b, (Vec3*)(self + 0x38), &data_ov006_02140914);
    *(int*)(self + 0x44) = b.x;
    *(int*)(self + 0x48) = b.y;
    *(int*)(self + 0x4c) = b.z;
    func_0203ce80(&c, (Vec3*)(self + 0x44));
    Quaternion_FromVector3((int*)(self + 0x74), &data_ov006_021408fc, (Vec3*)(self + 0x38));
    Quaternion_Normalize((int*)(self + 0x74));
}
}

namespace s020cde28 {
struct S{int w[2];}; extern "C" {extern struct S pmf_TrmpItem_Motion3;}
extern "C" void TrmpItem_SetMotion3(char*c){*(int*)(c+0x30)=0;*(struct S*)c=pmf_TrmpItem_Motion3;}
}

namespace s020cde4c {
extern "C" void TrmpItem_Motion2(char*c){
int v=*(int*)(c+0xc);
if(v>0x80000){*(int*)(c+0x30)=-0x1000;return;}
if(v<-0x60000)*(int*)(c+0x30)=0x1000;
}
}

namespace s020cde7c {
struct S{int w[2];}; extern "C" {extern struct S pmf_TrmpItem_Motion2;}
extern "C" void TrmpItem_SetMotion2(char*c){*(int*)(c+0x30)=0x1000;*(struct S*)c=pmf_TrmpItem_Motion2;}
}

namespace s020cdea0 {
extern "C" {extern int data_ov006_0212e07c[];}
extern "C" {extern int data_ov006_0212e088[];}
extern "C" void TrmpItem_Motion1(char *c) {
  int idx = *(short *)(c + 0x96);
  if (*(int *)(c + 8) > data_ov006_0212e07c[idx]) {
    *(int *)(c + 0x2c) = -data_ov006_0212e088[idx];
    return;
  }
  if (*(int *)(c + 8) < -data_ov006_0212e07c[idx]) {
    *(int *)(c + 0x2c) = data_ov006_0212e088[idx];
  }
}
}

namespace s020cdeec {
extern "C" {extern int data_ov006_0212e088[];}
struct S2 { int w[2]; };
extern "C" {extern struct S2 pmf_TrmpItem_Motion1;}
extern "C" void TrmpItem_SetMotion1(char *c) {
  int idx = *(short *)(c + 0x96);
  *(int *)(c + 0x2c) = data_ov006_0212e088[idx];
  *(struct S2 *)c = pmf_TrmpItem_Motion1;
}
}

namespace s020cdf1c {
extern "C" void TrmpItem_Motion0(void)
{
}
}

namespace s020cdf20 {
struct S { int w[2]; };
extern "C" {extern struct S pmf_TrmpItem_Motion0;}
extern "C" void TrmpItem_SetMotion0(char *p) { *(struct S *)(p + 0x0) = pmf_TrmpItem_Motion0; }
}

namespace s020cdf3c {
/* TrmpItem_StateAppear at 0x020cdf3c
 *
 * Matched byte-for-byte with mwccarm 1.2/sp2p3 (ov006).
 */
extern "C" extern int _Z14ApproachLinearRiii(int* a, int b, int c);
extern "C" extern void TrmpItem_SetMotion1(char* c);
extern "C" extern void TrmpItem_SetMotion2(char* c);
extern "C" extern void TrmpItem_SetMotion3(char* c);
extern "C" extern void TrmpItem_SetMotion0(char* c);
extern "C" extern void TrmpItem_SetMotion4(char* c);
extern "C" extern void TrmpItem_SetMotion6(char* c);
extern "C" extern void TrmpItem_SetMotion8(char* c);
extern "C" extern void TrmpItem_SetMotion5(char* c);
extern "C" extern void TrmpItem_SetMotion7(char* c);

extern "C" void TrmpItem_StateAppear(char* c)
{
    int a = _Z14ApproachLinearRiii((int*)(c + 0x6c), 0x1000, 0xc0);
    int b = _Z14ApproachLinearRiii((int*)(c + 0x68), 0x1000, 0x180);
    int d = _Z14ApproachLinearRiii((int*)(c + 0x70), 0x1000, 0x180);
    int e = _Z14ApproachLinearRiii((int*)(c + 0x88), 0x800, 0x60);
    if (a == 0) return;
    if (b == 0) return;
    if ((d & e) == 0) return;
    switch (*(short*)(c + 0x98)) {
    case 1: TrmpItem_SetMotion1(c); break;
    case 2: TrmpItem_SetMotion2(c); break;
    case 3: TrmpItem_SetMotion3(c); break;
    case 0: TrmpItem_SetMotion0(c); break;
    case 4: TrmpItem_SetMotion4(c); break;
    case 6: TrmpItem_SetMotion6(c); break;
    case 8: TrmpItem_SetMotion8(c); break;
    case 5: TrmpItem_SetMotion5(c); break;
    case 7: TrmpItem_SetMotion7(c); break;
    }
}
}

namespace s020ce0ac {
extern "C" {extern int data_ov006_02140838;}
extern "C" {extern int data_ov006_02140828;}
struct S2 { int w[2]; };
extern "C" {extern struct S2 pmf_TrmpItem_StateAppear_2;}
extern "C" void TrmpItem_StartAppear(char *c) {
  ApproachLinear(data_ov006_02140828, data_ov006_02140838, 1);
  *(char *)(c + 0x9c) = 0x1f;
  *(int *)(c + 0x2c) = 0;
  *(int *)(c + 0x30) = 0;
  *(int *)(c + 0x34) = 0;
  *(struct S2 *)c = pmf_TrmpItem_StateAppear_2;
}
}

namespace s020ce108 {
// @symbol TrmpItem_ReflectOffLine


extern "C" {
extern void Vec3_Sub(Vector3 *out, Vector3 *a, Vector3 *b);
extern void Vec3_MulScalar(Vector3 *out, const Vector3 *in, int scale);
extern void AddVec3(Vector3 *a, Vector3 *b, Vector3 *c);
extern void SubVec3(Vector3 *a, Vector3 *b, Vector3 *c);
extern int LenVec3(Vector3 *v);
extern int DotVec3(const Vector3 *a, const Vector3 *b);
extern int NormalizeVec3IfNonZero(Vector3 *v);
extern void Vec3_MulScalarInPlace(Vector3 *v, int s);

extern "C" {extern Vector3 data_020a0ebc;}
}

struct Obj {
    virtual Vector3 *m00();
    virtual void m04();
    virtual Vector3 *m08();
};

extern "C" void TrmpItem_ReflectOffLine(char *a, Obj *o)
{
    Vector3 vA;
    Vector3 vB;
    Vector3 vC;
    Vector3 acc;
    Vector3 scaled;
    Vector3 vD;
    Vector3 diff;
    Vector3 t1;
    Vector3 t2;
    Vector3 t3;
    Vector3 *p1;
    int dot44;
    Vector3 *p2;
    int lenA;
    int lenB;
    int dot38;

    {
        int *g = pmf_TrmpItem_GrowScale_3;
        int f0 = *(int *)a;
        if (f0 == g[0]) {
            if (*(int *)(a + 4) == g[1])
                return;
            if (f0 == 0)
                return;
        }
    }

    p1 = o->m00();
    p2 = o->m08();

    acc = data_020a0ebc;
    Vec3_Sub(&diff, p1, (Vector3 *)(a + 8));
    vB = diff;
    vA = diff;
    vC = diff;

    Vec3_MulScalar(&scaled, (Vector3 *)(a + 0x44), data_ov006_0212e070[*(short *)(a + 0x96)]);
    AddVec3(&vA, &scaled, &vA);
    SubVec3(&vB, &scaled, &vB);

    vA.y += 0xE000;
    vB.y += 0xE000;
    vA.y >>= 1;
    vB.y >>= 1;

    lenA = LenVec3(&vA);
    lenB = LenVec3(&vB);
    dot44 = DotVec3(&vC, (Vector3 *)(a + 0x44));
    dot38 = DotVec3(&vC, (Vector3 *)(a + 0x38));

    if (lenA < 0xE000 && NormalizeVec3IfNonZero(&vA) != 0) {
        Vec3_MulScalarInPlace(&vA, 0xE000 - lenA);
        vA.y <<= 1;
        Vec3_MulScalar(&t1, &vA, *(int *)(a + 0x88));
        AddVec3(p1, &t1, p1);
        AddVec3(&acc, &vA, &acc);
        *(short *)(((int)o + 0x22)) += 1;
    } else if (lenB < 0xE000 && NormalizeVec3IfNonZero(&vB) != 0) {
        Vec3_MulScalarInPlace(&vB, 0xE000 - lenB);
        vB.y <<= 1;
        Vec3_MulScalar(&t2, &vB, *(int *)(a + 0x88));
        AddVec3(p1, &t2, p1);
        AddVec3(&acc, &vB, &acc);
        *(short *)(((int)o + 0x22)) += 1;
    }

    Vec3_MulScalar(&t3, &acc, 0x100);
    AddVec3(p2, &t3, p2);

    {
        int x = p2->x;
        if (x < -0x1000)
            x = -0x1000;
        else if (x > 0x1000)
            x = 0x1000;
        p2->x = x;
    }

    {
        int mx = data_ov006_021405a8[0];
        int mn = data_ov006_021405b4[0];
        int y = p2->y;
        if (y >= mn) {
            if (y <= mx)
                mx = y;
            mn = mx;
        }
        p2->y = mn;
    }

    if ((dot38 < 0 ? -dot38 : dot38) >= 0xE000)
        return;

    if (dot44 < 0)
        dot44 = -dot44;
    if (dot44 >= data_ov006_0212e070[*(short *)(a + 0x96)])
        return;

    Vec3_MulScalar(&vD, (Vector3 *)(a + 0x38), 0x70);
    if (dot38 > 0)
        Vec3_MulScalarInPlace(&vD, -0x1000);
    {
        int t = vD.y;
        if (t > 0x80)
            t = 0x80;
        vD.y = t;
    }
    AddVec3(p2, &vD, p2);
}
}

namespace s020ce46c {
extern "C" {extern int data_ov006_021405ac;}
extern "C" {extern int data_ov006_02140838;}
extern "C" {extern char *data_ov006_0214082c;}
extern "C" {extern int data_ov006_02140818;}
extern "C" {extern int data_ov006_02140830;}

extern "C" extern void TrmpItem_ReflectOffLine(char *a, void *o);
extern "C" extern int TrmpItem_HitTest2D(char *a, void *o, int arg1);
extern "C" extern void _Z14ApproachLinearRiii(int *p, int target, int step);
extern "C" extern void D3D_PlaySeAtX(int a, int b);
extern "C" extern void Trmp2Spinner_InitAll(int c);
extern "C" extern void TrmpItem_ProjectToScreen(char *c, unsigned short arg1);

#define ELEM ((char *)data_ov006_0214082c + i * 0x1d0)

extern "C" void TrmpItem_UpdateWithJumper(void *thiz, int arg1)
{
    char *sb = (char *)thiz;
    int i;
    int j;
    int step;
    int v;
    int count7;

    if (data_ov006_021405ac != 0)
        return;
    *(unsigned short *)(sb + 0x22) = 0;

    for (i = 0; i < data_ov006_02140838; i++) {
        if (*(int *)(ELEM + 0x84) == 0)
            continue;
        TrmpItem_ReflectOffLine(ELEM, sb);
        if (TrmpItem_HitTest2D(ELEM, sb, arg1) == 0)
            continue;
        _Z14ApproachLinearRiii(&data_ov006_02140818, 0x270f, 1);
        v = *(unsigned short *)(sb + 0x20);
        step = 100;
        count7 = (v > 6) ? 6 : v;
        for (j = 0; j < count7; j++)
            step <<= 1;
        switch (v) {
        case 0:
            D3D_PlaySeAtX(0x131, *(int *)(ELEM + 8));
            break;
        case 1:
            D3D_PlaySeAtX(0x132, *(int *)(ELEM + 8));
            break;
        case 2:
            D3D_PlaySeAtX(0x133, *(int *)(ELEM + 8));
            break;
        case 3:
            D3D_PlaySeAtX(0x134, *(int *)(ELEM + 8));
            break;
        case 4:
        default:
            D3D_PlaySeAtX(0x135, *(int *)(ELEM + 8));
            break;
        }
        _Z14ApproachLinearRiii(&data_ov006_02140830, 0xf423f, step);
        Trmp2Spinner_InitAll(*(unsigned short *)(sb + 0x20));
        TrmpItem_ProjectToScreen(ELEM, (short)step);
        (*(unsigned short *)(sb + 0x20))++;
    }
}
}

namespace s020ce674 {
struct V2 { int x, y; };

#define AT(p, off) ((void*)(int)((char*)(p) + (off)))

extern "C" extern void TrmpItem_ProjectOnLine(char* self, void* other, struct V2* a, struct V2* b);
extern "C" extern void Vec2_Sub(struct V2* o, struct V2* a, struct V2* b);
extern "C" extern int func_0203d524(struct V2* a, struct V2* b);
extern "C" {extern int data_ov006_0212e070[];}
extern "C" {extern int pmf_TrmpItem_StateAppear[2];}
extern "C" {extern int pmf_TrmpItem_GrowScale_2[2];}

extern "C" int TrmpItem_HitTest2D(char* self, void* other) {
    struct V2 a, b, n, t0, d, e, f, g, h, i;
    int f0 = *(int*)self;
    int s0, s1, s2, s3;
    int result;

    {
        int *p324 = (int *)AT(pmf_TrmpItem_StateAppear, 0);
        int *p334;
        if ((f0 == p324[0] && (*(int*)(self + 4) == p324[1] || f0 == 0)) ||
            (p334 = (int *)AT(pmf_TrmpItem_GrowScale_2, 0),
             f0 == p334[0] && (*(int*)(self + 4) == p334[1] || f0 == 0)))
            return 0;
    }

    result = 0;
    TrmpItem_ProjectOnLine(self, other, &a, &b);

    {
        int *dp = (int *)AT(data_ov006_0212e070, 0);
        n.x = -dp[*(short*)(self + 0x96)];
        n.y = 0;
        t0.x = dp[*(short*)(self + 0x96)];
        t0.y = 0;
    }

    Vec2_Sub(&d, &t0, &n);
    Vec2_Sub(&e, &a, &n);
    Vec2_Sub(&f, &b, &n);
    s0 = func_0203d524(&d, &e);
    s1 = func_0203d524(&d, &f);
    if (s0 < -1) s0 = -1; else if (s0 > 1) s0 = 1;
    if (s1 < -1) s1 = -1; else if (s1 > 1) s1 = 1;
    if (s0 * s1 > 0) goto done;
    if (s0 == 0 && s1 == 0) goto done;

    Vec2_Sub(&g, &a, &b);
    d = g;
    Vec2_Sub(&h, &t0, &a);
    e = h;
    Vec2_Sub(&i, &n, &a);
    f = i;
    s2 = func_0203d524(&d, &e);
    s3 = func_0203d524(&d, &f);
    if (s2 < -1) s2 = -1; else if (s2 > 1) s2 = 1;
    if (s3 < -1) s3 = -1; else if (s3 > 1) s3 = 1;
    if (s2 * s3 < 0 && (s2 != 0 || s3 != 0)) result = 1;
done:
    return result;
}
}

namespace s020ce8a0 {
// @symbol TrmpItem_ProjectOnLine

extern "C" void Vec3_Sub(struct Vector3* out, struct Vector3* a, struct Vector3* b);
extern "C" int DotVec3(struct Vector3* a, struct Vector3* b);

struct Obj {
  virtual struct Vector3* GetA();
  virtual struct Vector3* GetB();
};

extern "C" void TrmpItem_ProjectOnLine(char* self, struct Obj* o, int* outR5, int* outR4){
  struct Vector3 a, b, sa, sb;
  struct Vector3* p;
  int t;
  p=o->GetA();
  a.x=p->x; a.y=p->y; a.z=p->z;
  p=o->GetB();
  b.x=p->x; b.y=p->y; b.z=p->z;
  Vec3_Sub(&sa,&a,(struct Vector3*)(self+8));
  Vec3_Sub(&sb,&b,(struct Vector3*)(self+0x14));
  sa.y += 0xc000;
  t=DotVec3((struct Vector3*)(self+0x38),&sa);
  outR5[0]=DotVec3((struct Vector3*)(self+0x44),&sa);
  outR5[1]=t;
  t=DotVec3((struct Vector3*)(self+0x50),&sb);
  outR4[0]=DotVec3((struct Vector3*)(self+0x5c),&sb);
  outR4[1]=t;
}
}

namespace s020ce988 {
// @symbol TrmpItem_UpdateMatrix

extern "C" {
void Matrix4x3_FromQuaternion(const void* q, struct Matrix4x3* mF);
void Matrix4x3_FromTranslation(struct Matrix4x3* m, int x, int y, int z);
void MulMat4x3Mat4x3(struct Matrix4x3* a, struct Matrix4x3* b, struct Matrix4x3* out);
void _ZN18TextureTransformer6UpdateER15ModelComponents(void* t, void* mc);
void _ZN9ModelBase12ApplyOpacityEjj(void* mb, unsigned int opacity, unsigned int unused);
}
extern "C" {extern struct Matrix4x3 data_020a0e68;}

struct VtO { int dummy[0x14/4]; void (*f)(void*, void*); };

extern "C" void TrmpItem_UpdateMatrix(char* c){
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
        vt->f(o, c+0x68);
    }
}
}

namespace s020cea2c {
extern "C" {
void AddVec3(void *a, void *b, void *c);
/* local extern: the call passes an untyped this, so it cannot use the header method. */
void _ZN15dExtFrameCtrl_c7AdvanceEv(void *anim);
}

struct C;
typedef void (C::*PMF)();

extern "C" void TrmpItem_Update(char *c)
{
    PMF *pp = (PMF *)c;
    (((C *)c)->**pp)();
    AddVec3(c + 8, c + 0x2c, c + 8);
    _ZN15dExtFrameCtrl_c7AdvanceEv(c + 0x194);
    *(int *)(c + 0x14) = *(int *)(c + 8);
    *(int *)(c + 0x18) = *(int *)(c + 0xc);
    *(int *)(c + 0x1c) = *(int *)(c + 0x10);
    *(int *)(c + 0x50) = *(int *)(c + 0x38);
    *(int *)(c + 0x54) = *(int *)(c + 0x3c);
    *(int *)(c + 0x58) = *(int *)(c + 0x40);
    *(int *)(c + 0x5c) = *(int *)(c + 0x44);
    *(int *)(c + 0x60) = *(int *)(c + 0x48);
    *(int *)(c + 0x64) = *(int *)(c + 0x4c);
}
}

namespace s020ceabc {
typedef struct { int x, y, z; } Vec3;

extern "C" {extern int data_ov006_0214081c;}
extern "C" {extern Vec3 data_ov006_021408a8;}
extern "C" {extern void *data_ov006_0214089c;}
extern "C" {extern int data_02092768[4];}
extern "C" {extern int data_020a0ebc[3];}
extern "C" extern void *_ZN7Vector3D1Ev(void *object);
extern "C" extern void func_020731dc(void *object, void *destructor, void **node);
extern "C" extern void func_0203cc28(int *p, int angle);
extern "C" extern void Quaternion_FromVector3(int *q, Vec3 *a, Vec3 *b);
extern "C" extern void TrmpItem_StartAppear(char *c);

extern "C" void TrmpItem_Init(char *self, int *v1, int *v2, int a3, short a4)
{
    if ((data_ov006_0214081c & 1) == 0) {
        data_ov006_021408a8.x = 0;
        data_ov006_021408a8.y = 0x1000;
        data_ov006_021408a8.z = 0;
        func_020731dc(&data_ov006_021408a8, (void *)_ZN7Vector3D1Ev, &data_ov006_0214089c);
        data_ov006_0214081c |= 1;
    }
    *(int *)(self + 0x8) = v1[0];
    *(int *)(self + 0xc) = v1[1];
    *(int *)(self + 0x10) = v1[2];
    *(int *)(self + 0x38) = v2[0];
    *(int *)(self + 0x3c) = v2[1];
    *(int *)(self + 0x40) = v2[2];
    {
        int *p38 = (int *)(int)(self + 0x38);
        *(int *)(self + 0x44) = p38[0];
        *(int *)(self + 0x48) = p38[1];
        *(int *)(self + 0x4c) = p38[2];
    }
    func_0203cc28((int *)(self + 0x44), -0x4000);
    *(int *)(self + 0x14) = *(int *)(self + 0x8);
    *(int *)(self + 0x18) = *(int *)(self + 0xc);
    *(int *)(self + 0x1c) = *(int *)(self + 0x10);
    {
        int *p14 = (int *)(int)(self + 0x14);
        *(int *)(self + 0x20) = p14[0];
        *(int *)(self + 0x24) = p14[1];
        *(int *)(self + 0x28) = p14[2];
    }
    *(int *)(self + 0x50) = *(int *)(self + 0x38);
    *(int *)(self + 0x54) = *(int *)(self + 0x3c);
    *(int *)(self + 0x58) = *(int *)(self + 0x40);
    *(int *)(self + 0x5c) = *(int *)(self + 0x44);
    *(int *)(self + 0x60) = *(int *)(self + 0x48);
    *(int *)(self + 0x64) = *(int *)(self + 0x4c);
    *(int *)(self + 0x74) = data_02092768[0];
    *(int *)(self + 0x78) = data_02092768[1];
    *(int *)(self + 0x7c) = data_02092768[2];
    *(int *)(self + 0x80) = data_02092768[3];
    Quaternion_FromVector3((int *)(self + 0x74), &data_ov006_021408a8, (Vec3 *)(self + 0x38));
    *(short *)(self + 0x96) = a3;
    *(short *)(self + 0x98) = a4;
    *(int *)(self + 0x84) = 1;
    switch (*(short *)(self + 0x96)) {
    case 0:
        *(void **)(self + 0x190) = self + 0xa0;
        break;
    case 1:
        *(void **)(self + 0x190) = self + 0xf0;
        break;
    case 2:
    default:
        *(void **)(self + 0x190) = self + 0x140;
        break;
    }
    *(int *)(self + 0x68) = data_020a0ebc[0];
    *(int *)(self + 0x6c) = data_020a0ebc[1];
    *(int *)(self + 0x70) = data_020a0ebc[2];
    *(int *)(self + 0x88) = 0;
    *(short *)(self + 0x92) = 0x5a;
    TrmpItem_StartAppear(self);
}
}

namespace s020cecb4 {
extern "C" void TrmpItem_Hide(int *p)
{
    p[33] = 0;
}
}

namespace s020cecc0 {
extern "C" {
void TrmpItem_Hide(int *p);
}
/* Signature deliberately copied from the local declaration above: the
   ROM name carries by-value class parameters (e.g. Fix12<int>), which
   mwccarm passes differently at the call site, so declaring the true
   types breaks the byte match. See notes/mwccarm-codegen.md 6az. */
extern "C" void _ZN18TextureTransformer7SetFileER8BTA_Filei5Fix12IiEj(void *, BTA_File&, int, int, unsigned int);


struct data_t { int a; BMD_File* f; };
extern "C" {extern data_t data_ov006_02140850;}
extern "C" {extern data_t data_ov006_02140858;}
extern "C" {extern data_t data_ov006_02140848;}
extern "C" {extern BTA_File data_ov006_0213b3a4;}

extern "C" void TrmpItem_Load(char* c)
{
    ((Model*)(c+0xa0))->SetFile(data_ov006_02140850.f, 1, -1);
    ((Model*)(c+0xf0))->SetFile(data_ov006_02140858.f, 1, -1);
    ((Model*)(c+0x140))->SetFile(data_ov006_02140848.f, 1, -1);
    ((Model*)(c+0xa0))->SetPolygonID(1);
    ((Model*)(c+0xf0))->SetPolygonID(2);
    ((Model*)(c+0x140))->SetPolygonID(3);
    TextureTransformer::Prepare(*data_ov006_02140850.f, data_ov006_0213b3a4);
    _ZN18TextureTransformer7SetFileER8BTA_Filei5Fix12IiEj((TextureTransformer*)(c+0x194), data_ov006_0213b3a4, 0, 0x1000, 0);
    TrmpItem_Hide((int*)c);
}
}

namespace s020ced84 {
/* TrmpItem_UpdateMatricesAll at 0x020ced84
 *
 * Matched byte-for-byte with mwccarm 1.2/sp2p3 (ov006).
 */
extern "C" {extern int data_ov006_02140838;}
extern "C" {extern char *data_ov006_0214082c;}
extern "C" extern void TrmpItem_UpdateMatrix(char *c);

extern "C" void TrmpItem_UpdateMatricesAll(void)
{
    int i;
    for (i = 0; i < data_ov006_02140838; i++)
    {
        char *p = data_ov006_0214082c + i * 0x1d0;
        if (*(int *)(p + 0x84) != 0)
            TrmpItem_UpdateMatrix(p);
    }
}
}

namespace s020cedf0 {
/* TrmpItem_UpdateAll at 0x020cedf0
 *
 * Matched byte-for-byte with mwccarm 1.2/sp2p3 (ov006).
 */
extern "C" {extern int data_ov006_02140838;}
extern "C" {extern char* data_ov006_0214082c;}
extern "C" extern void TrmpItem_Update(char *c);

extern "C" void TrmpItem_UpdateAll(void){
  int i=0;
  int off;
  if(data_ov006_02140838>0){
    off=0;
    do{
      char *p = data_ov006_0214082c + off;
      if(*(int*)(p+0x84)!=0){
        TrmpItem_Update(p);
      }
      i++;
      off+=0x1d0;
    }while(i<data_ov006_02140838);
  }
}
}

namespace s020cee5c {
extern "C" {extern int data_ov006_02140838;}
extern "C" {extern unsigned char* data_ov006_0214082c;}
extern "C" {extern int data_ov006_02140828;}
extern "C" {extern int data_ov006_02140818;}
extern "C" {extern int data_ov006_02140830;}
extern "C" extern void TrmpItem_Hide(int* p);
extern "C" void TrmpItem_HideAll(void){
  int i=0;
  int off;
  if(data_ov006_02140838>0){
    off=0;
    do{
      TrmpItem_Hide((int*)(data_ov006_0214082c+off));
      i++;
      off+=0x1d0;
    }while(i<data_ov006_02140838);
  }
  data_ov006_02140828=0;
  data_ov006_02140818=0;
  data_ov006_02140830=0;
}
}

namespace s020ceedc {
extern "C" extern void _ZN13SharedFilePtr7ReleaseEv(void *);
extern "C" {extern int data_ov006_02140850[];}
extern "C" {extern int data_ov006_02140858[];}
extern "C" {extern int data_ov006_02140848[];}
extern "C" void TrmpItem_ReleaseFiles(void)
{
    _ZN13SharedFilePtr7ReleaseEv(data_ov006_02140850);
    _ZN13SharedFilePtr7ReleaseEv(data_ov006_02140858);
    _ZN13SharedFilePtr7ReleaseEv(data_ov006_02140848);
}
}

namespace s020cef14 {
extern "C" extern void *_ZN5Model8LoadFileER13SharedFilePtr(void *fp);
extern "C" extern void TrmpItem_Load(void *p);
extern "C" {extern int data_ov006_02140850[];}
extern "C" {extern int data_ov006_02140858[];}
extern "C" {extern int data_ov006_02140848[];}
extern "C" {extern char *data_ov006_0214082c[];}
extern "C" {extern int data_ov006_02140838[];}

extern "C" void TrmpItem_LoadAll(char *a, int count)
{
    int i;
    int off;
    _ZN5Model8LoadFileER13SharedFilePtr(data_ov006_02140850);
    _ZN5Model8LoadFileER13SharedFilePtr(data_ov006_02140858);
    _ZN5Model8LoadFileER13SharedFilePtr(data_ov006_02140848);
    data_ov006_0214082c[0] = a;
    data_ov006_02140838[0] = count;
    i = 0;
    if (count <= 0)
        return;
    off = 0;
    do {
        TrmpItem_Load(data_ov006_0214082c[0] + off);
        i++;
        off += 0x1d0;
    } while (i < data_ov006_02140838[0]);
}
}

namespace s020cefa4 {
/* TrmpItem_Spawn at 0x020cefa4
 *
 * Matched byte-for-byte with mwccarm 1.2/sp2p3 (ov006).
 */
extern "C" {extern int data_ov006_02140838;}
extern "C" {extern char *data_ov006_0214082c;}
extern "C" extern int TrmpItem_Init(char *p, int a0, int a1, int a2, int a3);

extern "C" char *TrmpItem_Spawn(int a0, int a1, int a2, int a3)
{
    int i = 0;
    char *base;
    int off;
    if (data_ov006_02140838 > 0) {
        base = data_ov006_0214082c;
        do {
            if (*(int*)(base + 0x84) == 0) {
                off = i * 0x1d0;
                TrmpItem_Init(data_ov006_0214082c + off, a0, a1, a2, a3);
                return data_ov006_0214082c + off;
            }
            i++;
            base += 0x1d0;
        } while (i < data_ov006_02140838);
    }
    return 0;
}
}

namespace s020cf040 {
struct Vec3
{
  int x;
  int y;
  int z;
};
extern "C" extern int DotVec3(struct Vec3 *a, struct Vec3 *b);
extern "C" extern void SubVec3(struct Vec3 *a, struct Vec3 *b, struct Vec3 *c);
extern "C" extern int LenVec3(struct Vec3 *v);
extern "C" extern int _ZN4cstd4fdivEii(int a, int b);
extern "C" {extern int data_ov006_0212e0f0[];}
extern "C" void TrmpLine_PointDistance(char *sl, void *arg1, struct Vec3 *r2)
{
  int i;
  int n5 = -DotVec3(r2, (struct Vec3 *) (sl + 0x14));
  int *op = (int *) (sl + 0x29c);
  char *vp = sl + 0x11c;
  int new_var;
  int *dp = data_ov006_0212e0f0;
  for (i = 0; i < 4; i++)
  {
    int j;
    for (j = 0; j < 4; j++)
    {
      struct Vec3 d;
      SubVec3((struct Vec3 *) vp, (struct Vec3 *) arg1, &d);
      int r4 = (long long) (*((int *) (sl + 0x58)));
      int len = LenVec3(&d);
      int f = _ZN4cstd4fdivEii(r4, r4 + len);
      int t = (int) (((((long long) n5) * (*dp)) + 0x800) >> 12);
      new_var = (int) (((((long long) t) * f) + 0x800) >> 12);
      *op = new_var;
      vp += 0xc;
      dp++;
      op++;
    }

  }

}
}

namespace s020cf124 {
typedef struct { int x, y, z; } Vec3;

extern "C" extern void SubVec3(Vec3* a, Vec3* b, Vec3* c);
extern "C" extern void CrossVec3(const Vec3* a, const Vec3* b, Vec3* out);
extern "C" extern void NormalizeVec3(int* v, int* out);

#pragma push
#pragma opt_strength_reduction off
extern "C" void TrmpLine_ComputeNormals(char* self)
{
    Vec3 ej;
    Vec3 ei;
    char* p;
    Vec3* n;
    int* out;
    int i;
    int j;
    int mask = 0x3ff;

    p = self + 0x5c;
    n = (Vec3*)(self + 0x1dc);
    out = (int*)(self + 0x2dc);

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            int *q;
            int z = 0;

            q = (int*)&ej; q[0] = z; q[1] = z; q[2] = z;
            q = (int*)&ei; q[0] = z; q[1] = z; q[2] = z;

            if (j == 0)
                SubVec3((Vec3*)p, (Vec3*)(p + 0xc), &ej);
            else if (j == 3)
                SubVec3((Vec3*)(p - 0xc), (Vec3*)p, &ej);
            else
                SubVec3((Vec3*)(p - 0xc), (Vec3*)(p + 0xc), &ej);

            if (i == 0)
                SubVec3((Vec3*)p, (Vec3*)(p + 0x30), &ei);
            else if (i == 3)
                SubVec3((Vec3*)(p - 0x30), (Vec3*)p, &ei);
            else
                SubVec3((Vec3*)(p - 0x30), (Vec3*)(p + 0x30), &ei);

            CrossVec3(&ej, &ei, n);
            NormalizeVec3((int*)n, (int*)n);

            n->x = (int)(((long long)n->x * 0xff8 + 0x800) >> 12);
            n->y = (int)(((long long)n->y * 0xff8 + 0x800) >> 12);
            n->z = (int)(((long long)n->z * 0xff8 + 0x800) >> 12);

            *out = ((n->x >> 3) & mask)
                 | (((n->y >> 3) & mask) << 10)
                 | (((n->z >> 3) & mask) << 20);

            n++;
            out++;
            p += 0xc;
        }
    }
}
#pragma pop
}

namespace s020cf2fc {
typedef volatile unsigned int vu32;

typedef struct
{
  s32 x;
  s32 y;
  s32 z;
} Vec3;

struct Matrix4x3;
extern "C" extern struct Matrix4x3 data_020a0e68;
extern "C" extern struct Matrix4x3 data_0209b3ec;
extern "C" extern unsigned short data_ov006_0212e060[];
extern "C" extern unsigned short data_ov006_0212e068[];
extern "C" extern int data_ov006_0212e0b0[];
extern "C" extern void *data_ov006_02140844;
extern "C" extern void *data_ov006_02140814;
extern "C" extern void Matrix4x3_FromTranslation(struct Matrix4x3 *m, int x, int y, int z);
extern "C" extern void MulMat4x3Mat4x3(const int *a, const int *b, int *dst);
extern "C" extern void Matrix4x3_ApplyInPlaceToScale(struct Matrix4x3 *m, int x, int y, int z);
extern "C" extern void func_020553a4(int *m);

#define REG_MTX_MODE       (*(vu32 *)0x4000440)
#define REG_MTX_IDENTITY   (*(vu32 *)0x4000454)
#define REG_MTX_SCALE      (*(vu32 *)0x400046c)
#define REG_NORMAL         (*(vu32 *)0x4000484)
#define REG_TEXCOORD       (*(vu32 *)0x4000488)
#define REG_VTX_16         (*(vu32 *)0x400048c)
#define REG_POLYGON_ATTR   (*(vu32 *)0x40004a4)
#define REG_TEXIMAGE_PARAM (*(vu32 *)0x40004a8)
#define REG_TEXPLTT_BASE   (*(vu32 *)0x40004ac)
#define REG_DIF_AMB        (*(vu32 *)0x40004c0)
#define REG_SPE_EMI        (*(vu32 *)0x40004c4)
#define REG_BEGIN_VTXS     (*(vu32 *)0x4000500)
#define REG_END_VTXS       (*(vu32 *)0x4000504)

#define PATCH_VTX(obj) ((Vec3 *)((obj) + 0x5c))
#define PATCH_NRM(obj) ((int *)((obj) + 0x2dc))

static inline void G3_Vtx(s16 x, s16 y, s16 z)
{
  REG_VTX_16 = (u16)x | ((u16)y << 16);
  REG_VTX_16 = (u16)z;
}

#define SEND_VTX(p) G3_Vtx((s16)((p)->x >> 8), (s16)((p)->y >> 8), (s16)((p)->z >> 8))

// @symbol TrmpLine_Draw
/* Loads the object's placement and scale matrices, then draws its 4x4 vertex
 * patch (positions at +0x5c, packed normals at +0x2dc) as three triangle
 * strips per face, the back face first with negated normals. Each normal
 * pointer steps in its own statement after the REG_NORMAL write; stepping it
 * inside the write reorders the vertex loads. */
extern "C" void TrmpLine_Draw(char *obj)
{
  int i;
  int m2[12];
  int m1[12];
  Matrix4x3_FromTranslation(&data_020a0e68, *((int *) (obj + 8)), *((int *) (obj + 0xc)), *((int *) (obj + 0x10)));
  MulMat4x3Mat4x3((const int *) &data_020a0e68, (const int *) &data_0209b3ec, m1);
  Matrix4x3_ApplyInPlaceToScale(&data_020a0e68, *((int *) (obj + 0x2c)), *((int *) (obj + 0x30)), *((int *) (obj + 0x34)));
  MulMat4x3Mat4x3((const int *) &data_020a0e68, (const int *) &data_0209b3ec, m2);
  REG_MTX_MODE = 2;
  func_020553a4(m1);
  REG_MTX_MODE = 1;
  func_020553a4(m2);
  REG_MTX_SCALE = 0x100000;
  REG_MTX_SCALE = 0x100000;
  REG_MTX_SCALE = 0x100000;
  REG_MTX_MODE = 3;
  REG_MTX_IDENTITY = 0;
  REG_TEXIMAGE_PARAM = 0x8da70000 | ((u32) data_ov006_02140844 >> 3);
  REG_TEXPLTT_BASE = (u32) data_ov006_02140814 >> 4;
  {
    short *p31e = (short *) (obj + 0x31e);
    int sh = *p31e;
    unsigned short *dif = data_ov006_0212e060;
    volatile unsigned char *alpha = (volatile unsigned char *) (obj + 0x329);
    int pl = *alpha;
    REG_POLYGON_ATTR = (((sh + 1) << 24) | 0x82) | (pl << 16);
    {
      unsigned short *p326 = (unsigned short *) (obj + 0x326);
      unsigned idx = *p326;
      REG_DIF_AMB = dif[idx] | (data_ov006_0212e068[idx] << 16);
    }
  }
  REG_SPE_EMI = 0x8000;

  /* back face: row i then row i+1, normals negated */
  for (i = 0; i < 3; i++)
  {
    Vec3 *v0 = &PATCH_VTX(obj)[i * 4];
    Vec3 *v1 = &PATCH_VTX(obj)[(i + 1) * 4];
    int *n0 = &PATCH_NRM(obj)[i * 4];
    int *n1 = &PATCH_NRM(obj)[(i + 1) * 4];
    int k;
    REG_BEGIN_VTXS = 2;
    for (k = 0; k < 4; k++)
    {
      REG_TEXCOORD = data_ov006_0212e0b0[i * 4 + k];
      REG_NORMAL = -*n0 & 0x3fffffff;
      n0++;
      SEND_VTX(v0);
      v0++;
      REG_TEXCOORD = data_ov006_0212e0b0[(i + 1) * 4 + k];
      REG_NORMAL = -*n1 & 0x3fffffff;
      n1++;
      SEND_VTX(v1);
      v1++;
    }
    REG_END_VTXS = 0;
  }

  /* front face: row i+1 then row i */
  for (i = 0; i < 3; i++)
  {
    Vec3 *v0 = &PATCH_VTX(obj)[i * 4];
    Vec3 *v1 = &PATCH_VTX(obj)[(i + 1) * 4];
    int *n0 = &PATCH_NRM(obj)[i * 4];
    int *n1 = &PATCH_NRM(obj)[(i + 1) * 4];
    int k;
    REG_BEGIN_VTXS = 2;
    for (k = 0; k < 4; k++)
    {
      REG_TEXCOORD = data_ov006_0212e0b0[(i + 1) * 4 + k];
      REG_NORMAL = *n1;
      n1++;
      SEND_VTX(v1);
      v1++;
      REG_TEXCOORD = data_ov006_0212e0b0[i * 4 + k];
      REG_NORMAL = *n0;
      n0++;
      SEND_VTX(v0);
      v0++;
    }
    REG_END_VTXS = 0;
  }
}
}

namespace s020cf758 {
struct C;
typedef void (C::*PMF)();
struct C { PMF pmf; };
// @symbol TrmpLine_Update
extern "C" void TrmpLine_Update(C *c) {
  (c->*(c->pmf))();
}
}

namespace s020cf790 {
extern "C" extern int _Z15ApproachLinear2Riii(int* a, int b, int c);
extern "C" extern void TrmpLine_ComputeNormals(char* c);

// @symbol TrmpLine_StateFade
extern "C" void TrmpLine_StateFade(char* c) {
    if (_Z15ApproachLinear2Riii((int*)(c + 0x329), 0, 1) != 0) {
        *(unsigned char*)(c + 0x328) = 0;
        return;
    }
    *(int*)(((int)c + 0x2C)) += 0x80;
    *(int*)(((int)c + 0x30)) += 0x80;
    *(int*)(((int)c + 0x34)) += 0x80;
    TrmpLine_ComputeNormals(c);
}
}

namespace s020cf804 {
struct S { int w[2]; };
extern "C" {extern struct S pmf_TrmpLine_StateFade;}
// @symbol TrmpLine_SetStateFade
extern "C" void TrmpLine_SetStateFade(char *p) { *(struct S *)(p + 0x0) = pmf_TrmpLine_StateFade; }
}

namespace s020cf820 {
extern "C" extern int _Z15ApproachLinear2Riii(int *p, int a, int b);
extern "C" extern int _Z14ApproachLinearRiii(int *p, int a, int b);
extern "C" extern void TrmpLine_ComputeNormals(char *c);
extern "C" {extern int data_020a0db0;}
extern "C" {extern s16 data_02082214[];}

#define LA(p) ((int)(p))

// @symbol TrmpLine_StateSettle
extern "C" void TrmpLine_StateSettle(char *c)
{
    int vx, vz;
    int negS;
    int dx, dz;
    u16 ang;
    int j, i;
    int target, step;
    int *src, *dst, *val;

    if ((data_020a0db0 & 1) != 0) {
        if (_Z15ApproachLinear2Riii((int *)(c + 0x329), 0, 1) != 0) {
            *(u8 *)(c + 0x328) = 0;
        }
    }

    {
        u16 *fieldPtr = (u16 *)LA(c + 0x320);
        *fieldPtr = *fieldPtr + *(u16 *)(c + 0x322);
        ang = *(u16 *)(c + 0x320);
    }

    vx = *(int *)(c + 0x14);
    vz = *(int *)(c + 0x18);

    negS = -(int)data_02082214[(ang >> 4) * 2];

    dx = (int)(((s64)vx * negS + 0x800) >> 12);
    dz = (int)(((s64)vz * negS + 0x800) >> 12);

    src = (int *)(c + 0x11c);
    dst = (int *)(c + 0x5c);
    val = (int *)(c + 0x29c);

    target = 0;
    step = 0x100;

    for (j = 0; j < 4; j++) {
        for (i = 0; i < 4; i++) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[0] = dst[0] + (int)(((s64)*val * dx + 0x800) >> 12);
            dst[1] = dst[1] + (int)(((s64)*val * dz + 0x800) >> 12);
            _Z14ApproachLinearRiii(val, target, step);
            src += 3;
            dst += 3;
            val += 1;
        }
    }

    TrmpLine_ComputeNormals(c);
}
}

namespace s020cfa28 {
struct S { int w[2]; };
extern "C" {extern struct S pmf_TrmpLine_StateSettle;}
// @symbol TrmpLine_SetStateSettle
extern "C" void TrmpLine_SetStateSettle(char *p) { *(struct S *)(p + 0x0) = pmf_TrmpLine_StateSettle; }
}

namespace s020cfa44 {
typedef struct { int x, y, z; } Vec3;

extern "C" {extern s16 data_02082214[];}
extern "C" extern int TrmpLine_PushJumper(char *o);
extern "C" extern int _Z14ApproachLinearRiii(int *cur, int target, int step);
extern "C" extern int _Z15ApproachLinear2Rsss(short *cur, short target, short step);
extern "C" extern void TrmpLine_ComputeNormals(char *o);
extern "C" extern void TrmpLine_SetStateFade(char *o);

// @symbol TrmpLine_StateFlex
extern "C" void TrmpLine_StateFlex(char *o)
{
    int j, i;
    int a14, a18;
    int k;
    s16 s;
    int ns;
    int mulX, mulY;
    int zeroArg;
    int izero;
    int stepArg;
    Vec3 *src;
    Vec3 *dst;
    int *ratep;
    u16 *p320;

    TrmpLine_PushJumper(o);

    p320 = (u16 *)LA(o + 0x320);
    *p320 = *p320 + *(u16 *)(o + 0x322);

    a14 = *(int *)(o + 0x14);
    a18 = *(int *)(o + 0x18);

    k = *(u16 *)(o + 0x320) >> 4;
    s = data_02082214[k * 2];
    ns = -(int)s;

    src = (Vec3 *)(o + 0x11c);
    dst = (Vec3 *)(o + 0x5c);

    mulX = (int)(((s64)a14 * ns + 0x800) >> 12);
    mulY = (int)(((s64)a18 * ns + 0x800) >> 12);

    ratep = (int *)(o + 0x29c);

    zeroArg = 0;
    izero = 0;
    stepArg = 0x100;

    for (j = 0; j < 4; j++) {
        for (i = izero; i < 4; i++) {
            dst->x = src->x;
            dst->y = src->y;
            dst->z = src->z;
            dst->x += (int)(((s64)(*ratep) * mulX + 0x800) >> 12);
            dst->y += (int)(((s64)(*ratep) * mulY + 0x800) >> 12);

            _Z14ApproachLinearRiii(ratep, zeroArg, stepArg);
            src++;
            dst++;
            ratep++;
        }
    }

    TrmpLine_ComputeNormals(o);

    if (*(s16 *)(o + 0x31e) == 3)
        return;

    if (_Z15ApproachLinear2Rsss((short *)(o + 0x31c), 0, 1) == 0)
        return;

    TrmpLine_SetStateFade(o);
}
}

namespace s020cfc58 {
struct S { int w[2]; };
extern "C" {extern struct S pmf_TrmpLine_StateFlex;}
// @symbol TrmpLine_SetStateFlex
extern "C" void TrmpLine_SetStateFlex(char *p) { *(struct S *)(p + 0x0) = pmf_TrmpLine_StateFlex; }
}

namespace s020cfc74 {
/*
 * Per-frame gate-crossing check over the five tracked objects
 * (data_ov006_0214097c). For each live object, project its position and
 * target onto the gate frame (c+0x14 / c+0x20 basis, c+0x58 half-width). A
 * segment that crosses the gate records the hit (c+0x38 position, c+0x44
 * direction, state 1 or 2 with a sound whose pitch is lerped from the
 * crossing point), decrements the remaining count at c+0x324 and returns 1
 * when it reaches zero. Segments that miss just outside the gate edges are
 * marked state 3.
 *
 * Shape notes: the +-1 clamps are a ternary macro so the constants rank
 * above a/i in the callee-saved band and still hoist in the compiler's
 * own order; arr is assigned after the null/active checks so its pool
 * load sits in the hoisted-constant sequence; the +0x320/+0x324 halfword
 * accesses go through c directly, which yields the ROM's shared
 * c+0x300 base and the copied read.
 */
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))

struct V2 { int x, y; };

struct VT {
    Vector3 *(*GetPos)(void *);
    Vector3 *(*GetTargetPos)(void *);
    void (*Pad08)(void *);
    int (*IsActive)(void *);
};
struct Cannon {
    struct VT *vt;
    Vector3 v4;
    int f10;
    int f14;
    u16 f18;
};

extern "C" extern void Vec3_Sub(Vector3 *out, Vector3 *a, Vector3 *b);
extern "C" extern void SubVec3(Vector3 *a, Vector3 *b, Vector3 *c);
extern "C" extern int DotVec3(Vector3 *a, Vector3 *b);
extern "C" extern void Vec2_Sub(int *o, int *a, int *b);
extern "C" extern int func_0203d524(int *a, int *b);
extern "C" extern int _ZN4cstd4fdivEii(int a, int b);
extern "C" extern void D3D_PlaySePannedLoop(int a0, int a1, int a2);
extern "C" extern void TrmpLine_PointDistance(char *sl, void *arg1, Vector3 *r2);
extern "C" extern void TrmpLine_SetStateSettle(char *p);
extern "C" extern void Vec3_MulScalar(Vector3 *out, Vector3 *in, int scale);
extern "C" extern void Vec3_Add(Vector3 *out, Vector3 *a, Vector3 *b);

extern "C" {extern struct Cannon *data_ov006_0214097c[];}
extern "C" {extern int data_ov006_0213b30c;}
extern "C" {extern int data_ov006_0213b310;}
extern "C" {extern int data_ov006_0213b2f4;}
extern "C" {extern int data_ov006_0213b308;}

#pragma push
#pragma opt_common_subs off
#pragma opt_propagation off
// @symbol TrmpLine_PushJumper
extern "C" int TrmpLine_PushJumper(char *c)
{
    int i;
    int a;
    struct Cannon **arr;

    i = 0;
    do {
        int b, cc, dd;
        Vector3 pos0, pos1, diff;
        struct V2 p1proj, p0proj;
        struct V2 negW, posW;
        struct V2 gateDir, aTest, bTest;
        int flag, s1, s2;
        Vector3 sumPt, diffPt, farFwd, farBack;
        Vector3 *p0;
        Vector3 *p1;

        if (data_ov006_0214097c[i] == 0)
            continue;
        if (data_ov006_0214097c[i]->vt->IsActive(data_ov006_0214097c[i]) == 0)
            continue;
        arr = data_ov006_0214097c;

        p0 = arr[i]->vt->GetPos(arr[i]);
        pos0.x = p0->x;
        pos0.y = p0->y;
        pos0.z = p0->z;

        p1 = arr[i]->vt->GetTargetPos(arr[i]);
        pos1.x = p1->x;
        pos1.y = p1->y;
        pos1.z = p1->z;

        Vec3_Sub(&diff, &pos0, &pos1);
        SubVec3(&pos0, (Vector3 *)(c + 8), &pos0);
        SubVec3(&pos1, (Vector3 *)(c + 8), &pos1);

        a = DotVec3((Vector3 *)(c + 0x20), &pos0);
        b = DotVec3((Vector3 *)(c + 0x14), &pos0);
        cc = DotVec3((Vector3 *)(c + 0x20), &pos1);
        dd = DotVec3((Vector3 *)(c + 0x14), &pos1);
        DotVec3((Vector3 *)(c + 0x14), &diff);
        DotVec3((Vector3 *)(c + 0x20), &diff);

        p0proj.y = b;
        p1proj.x = cc;
        p0proj.x = a;
        p1proj.y = dd;

        negW.x = -(*(int *)(c + 0x58));
        negW.y = 0;
        posW.x = *(int *)(c + 0x58);
        posW.y = 0;

        Vec2_Sub((int *)&gateDir, (int *)&negW, (int *)&posW);
        Vec2_Sub((int *)&aTest, (int *)&negW, (int *)&p1proj);
        Vec2_Sub((int *)&bTest, (int *)&negW, (int *)&p0proj);

        flag = 0;
        s1 = func_0203d524((int *)&gateDir, (int *)&aTest);
        s2 = func_0203d524((int *)&gateDir, (int *)&bTest);
        s1 = CLAMP(s1, -1, 1);
        s2 = CLAMP(s2, -1, 1);

        if (s1 * s2 <= 0 && s1 > s2) {
            struct V2 edge, e1, e2;
            int t1, t2;
            Vec2_Sub((int *)&edge, (int *)&p1proj, (int *)&p0proj);
            gateDir = edge;
            Vec2_Sub((int *)&e1, (int *)&p1proj, (int *)&negW);
            aTest = e1;
            Vec2_Sub((int *)&e2, (int *)&p1proj, (int *)&posW);
            bTest = e2;

            t1 = func_0203d524((int *)&gateDir, (int *)&aTest);
            t2 = func_0203d524((int *)&gateDir, (int *)&bTest);
            t1 = CLAMP(t1, -1, 1);
            t2 = CLAMP(t2, -1, 1);
            if (t1 * t2 <= 0) flag = 1;
        }

        if (flag != 0) {
            int mag, t;
            *(int *)(c + 0x38) = pos0.x;
            *(int *)(c + 0x3c) = pos0.y;
            *(int *)(c + 0x40) = pos0.z;
            *(int *)(c + 0x44) = diff.x;
            *(int *)(c + 0x48) = diff.y;
            *(int *)(c + 0x4c) = diff.z;

            t = _ZN4cstd4fdivEii((a < 0) ? -a : a, *(int *)(c + 0x58));
            {
                int w = *(int *)(c + 0x58);
                int av = (a < 0) ? -a : a;
                mag = (int)(((long long)av * w + 0x800) >> 12);
            }

            if (mag < 0x400000) {
                arr[i]->f18 = 2;
                D3D_PlaySePannedLoop(0x1b1, *(int *)(c + 8),
                    (data_ov006_0213b310 * t + data_ov006_0213b30c * (0x1000 - t)) >> 12);
            } else {
                arr[i]->f18 = 1;
                D3D_PlaySePannedLoop(0x1ae, *(int *)(c + 8),
                    (data_ov006_0213b308 * t + data_ov006_0213b2f4 * (0x1000 - t)) >> 12);
            }

            {
                int *dst = (int *)((char *)arr[i] + 4);
                dst[0] = *(int *)(c + 0x14);
                dst[1] = *(int *)(c + 0x18);
                dst[2] = *(int *)(c + 0x1c);
                arr[i]->f10 = a;
                arr[i]->f14 = *(int *)(c + 0x58);
                *(u16 *)(c + 0x320) = 0;
                TrmpLine_PointDistance(c, (void *)(c + 0x38), (Vector3 *)(c + 0x44));

                *(u16 *)(c + 0x324) -= 1;
                if (*(u16 *)(c + 0x324) == 0) {
                    *(u8 *)(c + 0x328) = 3;
                    TrmpLine_SetStateSettle(c);
                    return 1;
                }
                *(u16 *)(c + 0x326) += 1;
            }
        } else {
            Vec3_MulScalar(&farFwd, (Vector3 *)(c + 0x20), *(int *)(c + 0x58));
            Vec3_Add(&sumPt, &pos0, &farFwd);
            Vec3_MulScalar(&farBack, (Vector3 *)(c + 0x20), *(int *)(c + 0x58));
            Vec3_Sub(&diffPt, &pos0, &farBack);

            if (sumPt.y < 0 && sumPt.y > -0x30000) {
                if (sumPt.x > -0x8000 && sumPt.x < 0) {
                    if (diff.x > 0) {
                        arr[i]->f18 = 3;
                    }
                }
            } else {
                if (diffPt.y < 0 && diffPt.y > -0x30000 && diffPt.x > 0 && diffPt.x < 0x8000 && diff.x < 0) {
                    arr[i]->f18 = 3;
                }
            }
        }
    } while (++i < 5);
    return 0;
}
#pragma pop
}

namespace s020d01e0 {
/* The minigame's touch-drawn line becomes a strip of grid points, and the
 * routine then picks the object that line cuts closest.
 *
 *   THE FRAME. The ROM opens with `sub sp, sp, #0xd4`: fourteen scratch words
 *   of which thirteen are ever written, then thirteen twelve-byte objects of
 *   which twelve are ever written. sp+0x34 and sp+0xc8 are reserved and never
 *   touched. In C that frame is unreachable, because an unused local emits
 *   nothing and is given no home (six shapes were measured, among them a dead
 *   struct copy, a guarded dead store and an aliasing pointer). A type with a
 *   DECLARED destructor behaves differently: the frontend still reserves its
 *   stack slot even when the optimiser empties it. Vector3 declares one already,
 *   for the arrays the ROM destroys through __cxa_vec_cleanup. `spareVec` and
 *   `spare4` are those two reservations written down. Their original spelling is
 *   not recoverable from the bytes; an elided copy of a by-value return is the
 *   likely source.
 *
 *   THE SWAP TEST. Its two reads go through a per-site const cast (see
 *   notes/mwccarm-codegen.md 6bk). The ROM re-reads both halfwords from memory
 *   after writing them, and the cast is its own CSE class, so the stores above
 *   do not forward into it.
 *
 * `colWeight += 0x555` sits at the tail of the inner loop with the other two
 * induction steps. Placed between the two `cell->x` statements instead, which is
 * where the earlier drafts had it, the whole 0x340..0x3e4 window schedules one
 * slot early and 35 words differ. Nothing else moves that residue: 246 pragma
 * names at on and off, every declaration order and type name, the loop form, the
 * pointer form and the multiply operand order are all inert on it.
 */
extern "C" {
extern void Vec3_Add(Vector3* out, Vector3* a, Vector3* b);
extern void Vec3_MulScalar(Vector3* out, Vector3* in, int s);
extern void Vec3_Sub(Vector3* out, Vector3* a, Vector3* b);
extern void Vec3_MulScalarInPlace(int *v, int s);
extern void SubVec3(Vector3 *a, Vector3 *b, Vector3 *c);
extern Fix12i Vec3_Dist(const Vector3* a, const Vector3* b);
extern Fix12i DotVec3(const Vector3 *a, const Vector3 *b);
extern int _ZN4cstd4fdivEii(int a, int b);
extern void TrmpLine_PointDistance(void *a, void *b, void *c);
extern void TrmpLine_ComputeNormals(void *a);
extern void D3D_PlaySePannedLoop(int a0, int a1, int a2);
extern void TrmpLine_SetStateSettle(char *p);
extern void TrmpLine_SetStateFlex(char *p);

extern void *data_ov006_0214097c[];
extern s32 data_ov006_0213b2fc;
extern s32 data_ov006_0213b300;
extern s32 data_ov006_0213b2f8;
extern s32 data_ov006_0213b304;
}

struct Spare4 { s32 v; ~Spare4() {} };
typedef s32 (*IsActiveFn)(void *);
typedef Vector3 *(*GetVecFn)(void *);

// @symbol TrmpLine_Build
extern "C" void TrmpLine_Build(char *c, short *p1, short *p2)
{
    Spare4 spare4;
    Vector3 va, vb;
    Vector3 bestRel, bestDir;
    Vector3 relPos, rawDir;
    Vector3 sumVec, scaledVec, diffVec, tmpVec, zAxis, crossVec;
    Vector3 spareVec;
    short swapX, swapZ;
    Fix12i dist, rowStep;
    s32 j;
    s32 flag;
    Vector3 *cell, *mirror;
    void *bestObj;
    void *obj;
    Vector3 *pos, *dir;
    s32 bestVal, bestD1;
    s32 k;
    s32 alongDot, crossDot, sideDot;
    s32 halfLen;
    s32 absOff, blendVal, ratio;

    *(s16 *)(c + 0x50) = p1[0];
    *(s16 *)(c + 0x52) = p1[1];
    *(s16 *)(c + 0x54) = p2[0];
    *(s16 *)(c + 0x56) = p2[1];

    p1[0] = (s16)(p1[0] - 0x80);
    p1[1] = (s16)(0 - p1[1]);
    p2[0] = (s16)(p2[0] - 0x80);
    p2[1] = (s16)(0 - p2[1]);

    if (*(const s16 *)p1 > *(const s16 *)p2) {
        swapX = p1[0];
        swapZ = p1[1];
        p1[0] = p2[0];
        p1[1] = p2[1];
        p2[0] = swapX;
        p2[1] = swapZ;
    }

    {
        s32 ax = ((s32)p1[0]) << 12;
        s32 ay = ((s32)p1[1]) << 12;
        va.x = ax; va.y = ay; va.z = 0;
    }
    {
        s32 bx = ((s32)p2[0]) << 12;
        s32 by = ((s32)p2[1]) << 12;
        vb.x = bx; vb.y = by; vb.z = 0;
    }

    Vec3_Add(&sumVec, &va, &vb);
    Vec3_MulScalar(&scaledVec, &sumVec, 0x800);
    *(s32 *)(c + 0x8) = scaledVec.x;
    *(s32 *)(c + 0xC) = scaledVec.y;
    *(s32 *)(c + 0x10) = scaledVec.z;

    Vec3_Sub(&diffVec, &vb, &va);
    *(s32 *)(c + 0x20) = diffVec.x;
    *(s32 *)(c + 0x24) = diffVec.y;
    *(s32 *)(c + 0x28) = diffVec.z;

    func_0203ce80(&tmpVec, (Vector3 *)(c + 0x20));

    zAxis.x = 0;
    zAxis.y = 0;
    zAxis.z = 0x1000;
    func_0203cf00(&crossVec, (Vector3 *)(c + 0x20), &zAxis);
    *(s32 *)(c + 0x14) = crossVec.x;
    *(s32 *)(c + 0x18) = crossVec.y;
    *(s32 *)(c + 0x1C) = crossVec.z;

    if (*(s32 *)(c + 0x18) < 0) {
        *(s32 *)(c + 0x14) = 0 - *(s32 *)(c + 0x14);
        *(s32 *)(c + 0x18) = 0 - *(s32 *)(c + 0x18);
    }

    *(s32 *)(c + 0x38) = 0;
    *(s32 *)(c + 0x3C) = 0;
    *(s32 *)(c + 0x40) = 0;
    *(s32 *)(c + 0x44) = 0 - *(s32 *)(c + 0x14);
    *(s32 *)(c + 0x48) = 0 - *(s32 *)(c + 0x18);
    *(s32 *)(c + 0x4C) = 0 - *(s32 *)(c + 0x1C);
    Vec3_MulScalarInPlace((s32 *)(c + 0x44), 0x800);

    SubVec3(&va, (Vector3 *)(c + 8), &va);
    SubVec3(&vb, (Vector3 *)(c + 8), &vb);
    dist = Vec3_Dist(&va, &vb);
    *(s32 *)(c + 0x58) = dist >> 1;
    rowStep = dist / 3;

    {
        s32 i;
        s32 rowFrac, colWeight, colFrac;
        s32 accum;
        s32 negRowFrac;
        s32 rowAbs, colAbs;
        s32 colW, invColWeight, rowW, cornerW, weightA, weightB, weightSq;

        j = 0;
        accum = 0;
        rowFrac = -0x180;
        cell = (Vector3 *)(c + 0x5C);
        mirror = (Vector3 *)(c + 0x11C);
        do {
            i = 0;
            negRowFrac = 0 - rowFrac;
            colFrac = -0x180;
            colWeight = 0;
            do {
                colAbs = (colFrac < 0) ? (0 - colFrac) : colFrac;
                colW = 0x1000 - (0x180 - colAbs);
                invColWeight = 0x1000 - colWeight;
                rowAbs = (rowFrac < 0) ? negRowFrac : rowFrac;
                rowW = 0x1000 - (0x180 - rowAbs);
                cornerW = (s32)(((s64)colW * rowW + 0x800) >> 12);
                flag = 0;
                weightA = (s32)(((s64)invColWeight * cornerW + 0x800) >> 12);
                weightSq = (s32)(((s64)cornerW * cornerW + 0x800) >> 12);
                weightB = (s32)(((s64)colWeight * cornerW + 0x800) >> 12);

                cell->x = (s32)(((s64)vb.x * weightB + 0x800) >> 12);
                cell->x += (s32)(((s64)va.x * weightA + 0x800) >> 12);
                cell->y = (s32)(((s64)vb.y * weightB + 0x800) >> 12);
                cell->y += (s32)(((s64)va.y * weightA + 0x800) >> 12);

                cell->z = accum - *(s32 *)(c + 0x58);
                cell->z = (s32)(((s64)cell->z * weightSq + 0x800) >> 12);

                mirror->x = cell->x;
                mirror->y = cell->y;
                mirror->z = cell->z;

                i++;
                colFrac += 0x100;
                colWeight += 0x555;
                cell++;
                mirror++;
            } while (i < 4);
            j++;
            accum += rowStep;
            rowFrac += 0x100;
        } while (j < 4);
    }

    *(s16 *)(c + 0x320) = (s16)flag;
    *(s16 *)(c + 0x322) = 0x1400;
    *(u8 *)(c + 0x329) = 0x1F;
    TrmpLine_PointDistance(c, c + 0x38, c + 0x44);
    TrmpLine_ComputeNormals(c);

    *(s32 *)(c + 0x2C) = 0x1000;
    *(s32 *)(c + 0x30) = 0x1000;
    *(s32 *)(c + 0x34) = 0x1000;

    bestObj = 0;
    bestVal = 0;
    bestD1 = 0;
    bestRel.x = 0;
    bestRel.y = 0;
    bestRel.z = 0;
    bestDir.x = 0;
    bestDir.y = 0;
    bestDir.z = 0;

    k = 0;
    do {
        obj = data_ov006_0214097c[k];
        if (obj != 0) {
            if (((IsActiveFn)((*(void ***)obj))[3])(obj) != 0) {
                obj = data_ov006_0214097c[k];
                pos = ((GetVecFn)((*(void ***)obj))[0])(obj);
                relPos.x = pos->x;
                relPos.y = pos->y;
                relPos.z = pos->z;
                obj = data_ov006_0214097c[k];
                dir = ((GetVecFn)((*(void ***)obj))[2])(obj);
                rawDir.x = dir->x;
                rawDir.y = dir->y;
                rawDir.z = dir->z;

                SubVec3(&relPos, (Vector3 *)(c + 8), &relPos);
                alongDot = DotVec3((Vector3 *)(c + 0x20), &relPos);
                crossDot = DotVec3((Vector3 *)(c + 0x14), &relPos);
                sideDot = DotVec3((Vector3 *)(c + 0x14), &rawDir);
                DotVec3((Vector3 *)(c + 0x20), &rawDir);

                if (sideDot < 0x100 && crossDot <= bestVal && crossDot > -0x24000) {
                    s32 h = *(s32 *)(c + 0x58);
                    if (alongDot > -h && alongDot < h) {
                        bestRel = relPos;
                        bestDir = rawDir;
                        bestObj = data_ov006_0214097c[k];
                        bestVal = crossDot;
                        bestD1 = alongDot;
                    }
                }
            }
        }
        k++;
    } while (k < 5);

    if (bestObj != 0) {
        *(s32 *)(c + 0x38) = bestRel.x;
        *(s32 *)(c + 0x3C) = bestRel.y;
        *(s32 *)(c + 0x40) = bestRel.z;
        *(s32 *)(c + 0x44) = bestDir.x;
        *(s32 *)(c + 0x48) = bestDir.y;
        *(s32 *)(c + 0x4C) = bestDir.z;

        absOff = (bestD1 < 0) ? (0 - bestD1) : bestD1;
        ratio = _ZN4cstd4fdivEii(absOff, *(s32 *)(c + 0x58));
        halfLen = *(s32 *)(c + 0x58);
        absOff = (bestD1 < 0) ? (0 - bestD1) : bestD1;
        blendVal = (s32)(((s64)absOff * halfLen + 0x800) >> 12);
        if (blendVal < 0x400000) {
            *(s16 *)((char *)bestObj + 0x18) = 2;
            D3D_PlaySePannedLoop(0x1B1, *(s32 *)(c + 8),
                (s32)(data_ov006_0213b2fc * ratio + data_ov006_0213b300 * (0x1000 - ratio)) >> 0xC);
        } else {
            *(s16 *)((char *)bestObj + 0x18) = 1;
            D3D_PlaySePannedLoop(0x1AE, *(s32 *)(c + 8),
                (s32)(data_ov006_0213b2f8 * ratio + data_ov006_0213b304 * (0x1000 - ratio)) >> 0xC);
        }
        *(s32 *)((char *)bestObj + 0x4) = *(s32 *)(c + 0x14);
        *(s32 *)((char *)bestObj + 0x8) = *(s32 *)(c + 0x18);
        *(s32 *)((char *)bestObj + 0xC) = *(s32 *)(c + 0x1C);
        *(s32 *)((char *)bestObj + 0x10) = bestD1;
        *(s32 *)((char *)bestObj + 0x14) = *(s32 *)(c + 0x58);
        *(u8 *)(c + 0x328) = 3;
        TrmpLine_PointDistance(c, c + 0x38, c + 0x44);
        TrmpLine_SetStateSettle(c);
        return;
    }

    *(s16 *)(c + 0x31C) = 0x258;
    *(u8 *)(c + 0x328) = 1;
    TrmpLine_SetStateFlex(c);
}
}

namespace s020d09e0 {
struct V3 { int z, y, x; };
extern "C" extern void func_020553a4(void* p);
extern "C" extern void func_0203cd80(struct V3* out, int a, int b);
extern "C" extern void TrmpLine_Draw(char* p);
extern "C" {extern char data_0209b3ec[];}
extern "C" {extern char data_ov006_02140990[];}
// @symbol TrmpLine_DrawAll
extern "C" void TrmpLine_DrawAll(void) {
    struct V3 s;
    *(volatile int*)0x4000440 = 2;
    func_020553a4(data_0209b3ec);
    s.z = 0;
    s.y = 0;
    s.x = 0xfffff008;
    func_0203cd80(&s, -0x2000, 0xfffff008);
    *(volatile int*)0x40004c8 = (((short)s.z >> 3) & 0x3ff)
        | ((((short)s.y >> 3) & 0x3ff) << 10)
        | ((((short)s.x >> 3) & 0x3ff) << 20)
        | 0x40000000;
    *(volatile int*)0x40004cc = 0x40007fff;
    int i;
    char* p = data_ov006_02140990;
    for (i = 0; i < 4; i++) {
        if (*(unsigned char*)(p + 0x328) != 0) {
            TrmpLine_Draw(p);
        }
        p += 0x32c;
    }
}
}

namespace s020d0ac0 {
extern "C" {extern char data_ov006_02140990[];}
extern "C" extern void TrmpLine_Update(void *c);
// @symbol TrmpLine_UpdateAll
extern "C" void TrmpLine_UpdateAll(void) {
    int i = 0;
    char *p = data_ov006_02140990;
    do {
        if (*(unsigned char*)(p + 0x328) != 0)
            TrmpLine_Update(p);
        i++;
        p += 0x32c;
    } while (i < 4);
}
}

namespace s020d0b04 {
extern "C" {extern unsigned char data_ov006_02140990[];}

// @symbol TrmpLine_ClearAll
extern "C" void TrmpLine_ClearAll(void) {
    int i = 0;
    unsigned char *p = data_ov006_02140990;
    int r0 = i;
    do {
        i++;
        *(p + 0x328) = r0;
        p += 0x32c;
    } while (i < 3);
}
}

namespace s020d0b2c {
extern "C" {
extern unsigned int func_02045a50(const void *src, unsigned int size);
extern char data_ov006_0213b414[];
extern void *data_ov006_02140844;
extern char data_ov006_0213b3f4[];
extern void *data_ov006_02140814;
}
// @symbol TrmpLine_LoadTextures
extern "C" void TrmpLine_LoadTextures(void) {
  data_ov006_02140844 = (void *)Model::LoadTextureToVram(data_ov006_0213b414, 0x400);
  data_ov006_02140814 = (void *)func_02045a50(data_ov006_0213b3f4, 0x20);
}
}

namespace s020d0b78 {
extern "C" {extern short data_ov006_02141314[];}
extern "C" {extern short data_ov006_02141590[];}
extern "C" extern void TrmpLine_Build(short* g, short* a, short* b);
// @symbol TrmpLine_CreateStartLineT2
extern "C" void TrmpLine_CreateStartLineT2(void) {
    short a[2];
    short b[2];
    a[0] = 0x10;
    a[1] = 0xb0;
    b[0] = 0xf0;
    b[1] = 0xb0;
    TrmpLine_Build(data_ov006_02141314, a, b);
    data_ov006_02141590[0x54] = 3;
    data_ov006_02141590[0x55] = 0;
    data_ov006_02141590[0x51] = 3;
}
}

namespace s020d0bd8 {
extern "C" {extern short data_ov006_02141314[];}
extern "C" {extern short data_ov006_02141590[];}
extern "C" extern void TrmpLine_Build(short* g, short* a, short* b);
// @symbol TrmpLine_CreateStartLineT1
extern "C" void TrmpLine_CreateStartLineT1(void) {
    short a[2];
    short b[2];
    a[0] = 0x18;
    a[1] = 0xb0;
    b[0] = 0xe8;
    b[1] = 0xb0;
    TrmpLine_Build(data_ov006_02141314, a, b);
    data_ov006_02141590[0x54] = 3;
    data_ov006_02141590[0x55] = 0;
    data_ov006_02141590[0x51] = 3;
}
}

namespace s020d0c38 {
/*
 * Try to place a new line segment between two 16-bit points (sl, sb).
 * Rejects segments shorter than 8 in x, with no direction, or under the
 * minimum half-length; when the full vector is short of 0x30000 it
 * re-scales the half vector (y stretched by 0xc00) to a fixed length and
 * rewrites both endpoints around the midpoint. Then finds the first free
 * slot (state byte +0x328 == 0) among the three, rejecting it if the
 * segment crosses any active (state 1) slot's own segment (+0x50/+0x54),
 * and initialises the slot through TrmpLine_Build, returning its
 * address (0 when none is free).
 *
 * Shape notes: the midpoint pair needs named x<<12 / y<<12 temps so the
 * y shift lands in place; the re-scale block reads half[1] inline at
 * both the multiply and the two-word copy into tmp (a named copy of it
 * takes r4 ahead of the umull low word).
 */
extern "C" extern void func_0203b958(s16* o, s16* a, s16* b);
extern "C" extern int func_0203d434(int* in);
extern "C" extern int Vec2_Len(int* v);
extern "C" extern int _ZN4cstd4fdivEii(int, int);
extern "C" extern void func_0203d630(int* p, int m);
extern "C" extern void func_0203d704(int* o, int* a, int* b);
extern "C" extern void Vec2_Sub(int* o, int* a, int* b);
extern "C" extern void TrmpLine_Build(s16* slot, s16* a, s16* b);

extern "C" {extern char data_ov006_02140990[];}
extern "C" {extern s16 data_ov006_02140cb4[];}
extern "C" {extern s16 data_ov006_02140cb6[];}
extern "C" {extern s16 data_ov006_02140cae[];}

// @symbol TrmpLine_Create
extern "C" int TrmpLine_Create(s16* sl, s16* sb) {
    s16 d[2];
    int fx[2];
    int half[2];
    int mid[2];
    int tmp[2];
    int p1[2];
    int p2[2];
    s16 v0[2];
    s16 v1[2];
    s16 v2[2];
    s16 v3[2];
    s16 v4[2];
    s16 v5[2];
    s16 a[2];
    s16 b[2];
    s16 s0;
    s16 s1;
    int abs0;
    int i;
    int j;
    char* base;
    char* p;
    int cross1;
    int cross2;
    long long m;
    int scaled;
    int len;
    int off;
    s16* slot;

    func_0203b958(d, sl, sb);

    s0 = d[0];
    if (s0 < 0)
        abs0 = (s16)(-s0);
    else
        abs0 = s0;
    if (abs0 < 8)
        return 0;

    s1 = d[1];
    fx[0] = (int)s0 << 12;
    fx[1] = (int)s1 << 12;
    half[0] = ((int)s0 >> 1) << 12;
    half[1] = ((int)s1 >> 1) << 12;

    {
        int ay = (s16)sl[1];
        int by = (s16)sb[1];
        int ax = (s16)sl[0];
        int bx = (s16)sb[0];
        int y = (ay + by) >> 1;
        int x = (ax + bx) >> 1;
        int x12 = x << 12;
        int y12 = y << 12;
        mid[0] = x12;
        mid[1] = y12;
    }

    if (func_0203d434(half) == 0)
        return 0;

    {
        int ah = half[0];
        if (ah < 0)
            ah = -ah;
        if (ah < 1401)
            return 0;
    }

    if (Vec2_Len(fx) < 0x30000) {
        m = (long long)half[1] * 0xc00 + 0x800;
        scaled = (int)(m >> 12);
        tmp[0] = half[0];
        tmp[1] = half[1];
        tmp[1] = scaled;
        len = Vec2_Len(tmp);
        func_0203d630(half, _ZN4cstd4fdivEii(0x18000, len));

        func_0203d704(p1, mid, half);
        Vec2_Sub(p2, mid, half);

        sl[0] = (s16)(p1[0] >> 12);
        sl[1] = (s16)(p1[1] >> 12);
        sb[0] = (s16)(p2[0] >> 12);
        sb[1] = (s16)(p2[1] >> 12);
    }

    base = data_ov006_02140990;
    i = 0;
    do {
        if ((unsigned char)base[0x328] == 0) {
            p = data_ov006_02140990;
            j = 0;
            do {
                if (i != j) {
                    if ((unsigned char)p[0x328] == 1) {
                        func_0203b958(v0, sb, sl);
                        func_0203b958(v1, (s16*)(p + 0x50), sl);
                        func_0203b958(v2, (s16*)(p + 0x54), sl);

                        {
                            s16 ax = v0[0];
                            s16 ay = v0[1];
                            s16 bx = v1[0];
                            s16 by = v1[1];
                            s16 cy = v2[1];
                            cross1 = (int)ax * (int)by - (int)ay * (int)bx;
                            {
                                s16 cx = v2[0];
                                cross2 = (int)ax * (int)cy - (int)ay * (int)cx;
                            }
                        }
                        if (cross1 * cross2 <= 0) {
                            func_0203b958(v3, (s16*)(p + 0x54), (s16*)(p + 0x50));
                            v0[0] = v3[0];
                            v0[1] = v3[1];
                            func_0203b958(v4, sl, (s16*)(p + 0x50));
                            v1[0] = v4[0];
                            v1[1] = v4[1];
                            func_0203b958(v5, sb, (s16*)(p + 0x50));
                            v2[0] = v5[0];
                            v2[1] = v5[1];

                            {
                                s16 ax = v0[0];
                                s16 ay = v0[1];
                                s16 bx = v1[0];
                                s16 by = v1[1];
                                s16 cy = v2[1];
                                cross1 = (int)ax * (int)by - (int)ay * (int)bx;
                                {
                                    s16 cx = v2[0];
                                    cross2 = (int)ax * (int)cy - (int)ay * (int)cx;
                                }
                            }
                            if (cross1 * cross2 <= 0)
                                return 0;
                        }
                    }
                }
                j++;
                p += 0x32c;
            } while (j < 4);

            a[0] = sl[0];
            a[1] = sl[1];
            b[0] = sb[0];
            b[1] = sb[1];
            off = i * 0x32c;
            slot = (s16*)((int)data_ov006_02140990 + off);
            TrmpLine_Build(slot, a, b);
            *(s16*)((int)data_ov006_02140cb4 + off) = 1;
            *(s16*)((int)data_ov006_02140cb6 + off) = 0;
            *(s16*)((int)data_ov006_02140cae + off) = (s16)i;
            return (int)slot;
        }
        i++;
        base += 0x32c;
    } while (i < 3);

    return 0;
}
}

namespace s020d0fe4 {
extern "C" extern int __cxa_vec_cleanup(void *dest, int a, int size, void (*func)(void));
extern "C" {extern char data_ov006_02140990;}
extern "C" extern void TrmpLine_Destroy(void);  /* local extern: only the two Trampoline scene headers declare it; this unit includes neither */

// @symbol TrmpLine_CleanupAll
extern "C" void TrmpLine_CleanupAll(void)
{
    __cxa_vec_cleanup(&data_ov006_02140990, 4, 0x32c, TrmpLine_Destroy);
}
}

namespace s020d1008 {
// @symbol TrmpLine_Destroy
extern "C" void TrmpLine_Destroy(void)
{
}
}

namespace s020d100c {
// @symbol TrmpLine_Reset
extern "C" void TrmpLine_Reset(char *p)
{
    p[808] = 0;
}
}
