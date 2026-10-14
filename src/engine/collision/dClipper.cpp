//cpp
/* The view clipper (arm9 .text 0x020150e8..0x0201577c): dClipper culls actor
 * positions against the camera frustum. The lone instance, data_0209f43c in
 * bss, is constructed by __sinit_02074e84; the camera setup path feeds it the
 * projection parameters through Func_020156DC, and dActor_c::BeforeBehavior
 * queries it per actor.
 *
 * The file is in ROM order under `#pragma defer_codegen off`, which emits
 * each function as it is parsed. ~dClipper is the only virtual (vtable
 * _ZTV8dClipper at 0x0208e730, slots D1/D0) and is inline in dClipper.h;
 * the cartridge stores D0 (0x020156fc) below D1 (0x02015720) with C1
 * (0x02015730) behind them, so two uncalled functions below ask for the
 * destructor's variants in that order before the constructor is written.
 *
 * leftovers:
 * - Func_020150E8 and Func_02015560 stay mangled free definitions: the
 *   real signatures take Fix12<int> by value and wall 6az homes
 *   class-typed by-value parameters (the dClipper.h declarations are
 *   the real ones for callers).
 * - Func_020150E8's opt_propagation off pair and goto-shaped body are
 *   the matched form; dClipper_EmitDeletingDestructor/EmitDestructor
 *   are uncalled stubs that force D0 then D1 out of the inline
 *   destructor (ROM order).
 */
#pragma defer_codegen off

#include "types.h"
#include "dClipper.h"
#include "math/Matrix.h"

extern "C" {
void MulVec3Mat4x3(const Vector3 *v, const Matrix4x3 *m, Vector3 *res);
void CrossVec3(Vector3 *a, Vector3 *b, Vector3 *result);
void NormalizeVec3(Vector3 *src, Vector3 *dst);
int _ZN4cstd4fdivEii(int a, int b);
/* Stays a mangled call: the real signature carries Fix12<int> and wall 6az
   (notes/mwccarm-codegen.md) homes class-typed by-value parameters. The
   declaration in dClipper.h is the real one and non-6az callers may use it. */
int _ZN8dClipper13Func_020150E8ER7Vector35Fix12IiEPh(dClipper *thiz, Vector3 *v, int clip, u8 *hint);
}
extern short data_02082214[];

#pragma opt_propagation off
// @symbol _ZN8dClipper13Func_020150E8ER7Vector35Fix12IiEPh
/* Stays a mangled free definition: the real signature carries Fix12<int> and
   wall 6az (notes/mwccarm-codegen.md) homes class-typed by-value parameters.
   The declaration in dClipper.h is the real one and callers may use it. */
extern "C" int _ZN8dClipper13Func_020150E8ER7Vector35Fix12IiEPh(dClipper *thiz, Vector3 *v, int clip, u8 *hint)
{
    if (hint != 0)
    {
        unsigned int first;
        long long y;
        long long x;
        long long z;
        long long c;
        int negZ;
        int one;
        int zero;
        long long dot;
        int outside;
        unsigned int i;
        int ok;
        long long d;

        negZ = -v->z;
        if (negZ < thiz->mNearZ - clip) goto fail1;
        if (negZ > thiz->mFarZ + clip) goto fail1;

        first = *hint & 3;
        x = v->x;
        c = clip;
        dot = ((long long)v->x * thiz->mPlaneNormals[first].x + (long long)v->y * thiz->mPlaneNormals[first].y + (long long)v->z * thiz->mPlaneNormals[first].z + 0x800) >> 12;
        outside = 0;
        if (c < dot) outside = 1;
        if (outside == 0)
        {
            i = (first + 1) & 3;
            y = (int)*(unsigned int *)&v->y;
            z = (int)*(unsigned int *)&v->z;
            zero = 0;
            one = 1;
            do {
                d = (z * thiz->mPlaneNormals[i].z + (x * thiz->mPlaneNormals[i].x + y * thiz->mPlaneNormals[i].y) + 0x800) >> 12;
                ok = d <= c ? one : zero;
                if (ok == 0) goto failstore;
                i = (i + 1) & 3;
            } while (i != first);
            return negZ;
        failstore:
            *hint = (u8)i;
        }
    fail1:
        return 0x7FFFFFFF;
    }
    else
    {
        int negZ;

        negZ = -v->z;
        if (negZ < thiz->mNearZ - clip) goto fail2;
        if (negZ > thiz->mFarZ + clip) goto fail2;

        if ((int)(((long long)v->z * thiz->mPlaneNormals[0].z + 0x800) >> 12)
            + ((int)(((long long)v->x * thiz->mPlaneNormals[0].x + 0x800) >> 12)
             + (int)(((long long)v->y * thiz->mPlaneNormals[0].y + 0x800) >> 12)) > clip)
            goto fail2;

        {
            long long x = v->x, y = v->y, z = v->z;
            if ((int)((z * thiz->mPlaneNormals[1].z + 0x800) >> 12)
                + ((int)((x * thiz->mPlaneNormals[1].x + 0x800) >> 12)
                 + (int)((y * thiz->mPlaneNormals[1].y + 0x800) >> 12)) > clip)
                goto fail2;
            if ((int)((z * thiz->mPlaneNormals[2].z + 0x800) >> 12)
                + ((int)((x * thiz->mPlaneNormals[2].x + 0x800) >> 12)
                 + (int)((y * thiz->mPlaneNormals[2].y + 0x800) >> 12)) > clip)
                goto fail2;
            if ((int)((z * thiz->mPlaneNormals[3].z + 0x800) >> 12)
                + ((int)((x * thiz->mPlaneNormals[3].x + 0x800) >> 12)
                 + (int)((y * thiz->mPlaneNormals[3].y + 0x800) >> 12)) <= clip)
                return negZ;
        }
    fail2:
        return 0x7FFFFFFF;
    }
}
#pragma opt_propagation on

// @symbol _ZN8dClipper13Func_02015560ER9Matrix4x3R7Vector35Fix12IiES3_
/* Stays a mangled free definition: the real signature carries Fix12<int> and
   wall 6az (notes/mwccarm-codegen.md) homes class-typed by-value parameters.
   The declaration in dClipper.h is the real one and callers may use it. */
extern "C" int _ZN8dClipper13Func_02015560ER9Matrix4x3R7Vector35Fix12IiES3_(
    dClipper *thiz, Matrix4x3 *mat, Vector3 *srcVec, int scale, Vector3 *dstVec)
{
    MulVec3Mat4x3(srcVec, mat, dstVec);
    return _ZN8dClipper13Func_020150E8ER7Vector35Fix12IiEPh(
        thiz, dstVec, scale, (u8 *)0);
}

// @symbol _ZN8dClipper13Func_0201559CEv
/* Rebuild the four view-frustum side planes from the current field of view
 * and near-plane distance.
 *
 * mFovAngle is an angle; its top 12 bits index a sine/cosine pair table at
 * data_02082214, and fdiv of that pair gives the tangent. Scaling by mNearZ
 * (the plane distance) gives the half-height b, and by mAspectRatio (the
 * aspect ratio) the half-width c. The four corner vectors at z = -mNearZ
 * follow, and each adjacent pair crossed and normalised is one side plane's
 * normal.
 */
void dClipper::Func_0201559C()
{
    int idx = (int)mFovAngle >> 4;
    int q = _ZN4cstd4fdivEii(data_02082214[2 * idx], data_02082214[2 * idx + 1]);
    Fix12i b = (Fix12i)(((long long)mNearZ * q + 0x800) >> 12);
    Fix12i c = (Fix12i)(((long long)mAspectRatio * b + 0x800) >> 12);
    Vector3 v0, v1, v2, v3;
    v0.x = -c; v0.y = -b; v0.z = -mNearZ;
    v1.x = -c; v1.y = b;  v1.z = -mNearZ;
    v2.x = c;  v2.y = b;  v2.z = -mNearZ;
    v3.x = c;  v3.y = -b; v3.z = -mNearZ;
    CrossVec3(&v1, &v0, (Vector3 *)&mPlaneNormals[0]);
    CrossVec3(&v2, &v1, (Vector3 *)&mPlaneNormals[1]);
    CrossVec3(&v3, &v2, (Vector3 *)&mPlaneNormals[2]);
    CrossVec3(&v0, &v3, (Vector3 *)&mPlaneNormals[3]);
    NormalizeVec3((Vector3 *)&mPlaneNormals[0], (Vector3 *)&mPlaneNormals[0]);
    NormalizeVec3((Vector3 *)&mPlaneNormals[1], (Vector3 *)&mPlaneNormals[1]);
    NormalizeVec3((Vector3 *)&mPlaneNormals[2], (Vector3 *)&mPlaneNormals[2]);
    NormalizeVec3((Vector3 *)&mPlaneNormals[3], (Vector3 *)&mPlaneNormals[3]);
}

// @symbol _ZN8dClipper13Func_020156DCEitii
/* The former `Ev` spelling claimed this method took no arguments, contradicting
   every call site and the register/stack reads in the ROM. Scalar parameter
   types reproduce both the observed ABI and the complete function bytes. */
void dClipper::Func_020156DC(Fix12i aspectRatio, u16 fovAngle,
                            Fix12i nearZ, Fix12i farZ)
{
    mAspectRatio = aspectRatio;
    mFovAngle = fovAngle;
    mNearZ = nearZ;
    mFarZ = farZ;
    Func_0201559C();
}

/* Not called. Forces the out-of-line copy of the deleting destructor. */
void dClipper_EmitDeletingDestructor(dClipper *p)
{
    delete p;
}

/* Not called. Forces the out-of-line copy of the inline destructor. */
void dClipper_EmitDestructor(dClipper *p)
{
    p->~dClipper();
}

// @symbol _ZN8dClipperC1Ev
/* The default projection: aspect 4:3 (0x1555), angle 0xe38 (20 degrees,
 * the half-angle Func_0201559C takes the tangent of), near plane 1.0 and
 * far plane 5000.0. */
dClipper::dClipper()
{
    Func_020156DC(0x1555, 0xe38, 0x1000, 0x1388000);
}
