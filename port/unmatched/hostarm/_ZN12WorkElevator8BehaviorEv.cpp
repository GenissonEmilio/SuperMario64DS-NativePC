/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/_ZN12WorkElevator8BehaviorEv.cpp, which carried a host arm
 * under #ifdef _MSC_VER inside daObjCvNewsLift_c::Behavior (ov021 0x021116c8).
 * The 2026-10 main merge took main's src/, which folded that body into
 * src/actors/daObjCvNewsLift_c.cpp without the arm.
 *
 * WHY IT EXISTS. The ROM keeps the three line colliders back to back on the
 * frame (ctors at sp+0x80, +0xf8, +0x170, 0x78 apart) and the raycast loop
 * walks them with one pointer starting at the first. Main still declares
 * three separate locals (l0, l1, l2) and walks `&l0` forward. MSVC does not
 * keep three separate locals contiguous or in order, so on the host the walk
 * ran off the first one into the vector arrays and the stack cookie. An
 * array gives the same contiguous layout and ctor order, with the dtors in
 * reverse.
 *
 * The arm was two lines inside a whole function, so the function is carried:
 * this is MAIN'S body (main's member names and Sound:: calls) statement for
 * statement, with only the collider declaration and the loop's first pointer
 * replaced by the arm. Main's definition of the same member must be parked in
 * the TU's hostgen copy. mwccarm never compiles this file, so nothing here
 * moves a ROM byte. */
#include "daObjCvNewsLift_c.h"
#include "dBgCh_Lin.h"
#include "Sound.h"

extern "C" {
void MulVec3Mat4x3(const Vector3 *src, const Matrix4x3 *mtx, Vector3 *dst);
void AddVec3(const Vector3 *a, const Vector3 *b, Vector3 *dst);
void Matrix4x3_FromRotationY(Matrix4x3 *m, s16 angle);
void Quaternion_SLerp(void *q0, void *q1, int t, void *out);
s32 Vec3_Equal(const Vector3 *a, const Vector3 *b);
u16 DecIfAbove0_Short(u16 *p);
u8 DecIfAbove0_Byte(u8 *p);
extern struct Matrix4x3 data_020a0e68;
extern s32 data_02092768[];
extern s16 data_02082214[];
extern const u16 data_ov021_02114740[];
}

s32 daObjCvNewsLift_c::Behavior()
{
    Vector3 rayStart[3];
    Vector3 rayFrom[3];
    Vector3 rayTo[3];
    Vector3 ahead;
    s32 i;

    if (mBumped) {
        if (DecIfAbove0_Short(&mWobbleTimer)) {
            mWobblePhase += 0x1f00;
            {
                s16 sine = data_02082214[((u16)(s16)mWobblePhase >> 4) * 2];
                s32 amp = mWobbleTimer * 0x1a;
                s8 which = mLoweredPlatform;
                s16 wobble = (s16)(s32)(((long long)amp * sine + 0x800) >> 12);
                switch (which) {
                case 0: mAngleX = -wobble; break;
                case 1: mAngleX = wobble; break;
                case 2: mAngleZ = wobble; break;
                case 3: mAngleZ = -wobble; break;
                }
            }
        } else {
            s8 which = mLoweredPlatform;
            if (which == 0 || which == 2)
                mLoweredPlatform++;
            else
                mLoweredPlatform--;
            mBumped = 0;
            mPrevAngleY = data_ov021_02114740[mLoweredPlatform];
            mAngleX = 0;
            mAngleZ = 0;
            mPosY = mHomePos.y;
        }
        if (mPrevLoweredPlatform != mLoweredPlatform) {
            mBumped = 0;
            mPrevAngleY = data_ov021_02114740[mLoweredPlatform];
            mAngleX = 0;
            mAngleZ = 0;
            mPosY = mHomePos.y;
        }
    } else {
        if (mLoweredPlatform != -1) {
            mMotorSound = Sound::PlayLong(mMotorSound, 3, 0x88, *(Vector3 *)&mCamSpacePosX, 0);
            mPrevAngleY = data_ov021_02114740[mLoweredPlatform];
            mHorzSpeed = 0x5000;
            rayStart[0].x = 0; rayStart[0].y = -0xa000; rayStart[0].z = 0x10e000;
            {
                Vector3 *ray = &rayStart[1];
                ray->x = 0x12c000; ray->y = -0xa000; ray->z = 0x10e000;
                ray = &rayStart[2];
                ray->x = -0x12c000; ray->y = -0xa000; ray->z = 0x10e000;
            }
            rayFrom[0].x = 0; rayFrom[0].y = 0; rayFrom[0].z = 0;
            {
                Vector3 *ray = &rayFrom[1];
                ray->x = 0; ray->y = 0; ray->z = 0;
                ray = &rayFrom[2];
                ray->x = 0; ray->y = 0; ray->z = 0;
            }
            rayTo[0].x = 0; rayTo[0].y = 0; rayTo[0].z = 0;
            {
                Vector3 *ray = &rayTo[1];
                ray->x = 0; ray->y = 0; ray->z = 0;
                ray = &rayTo[2];
                ray->x = 0; ray->y = 0; ray->z = 0;
            }
            Matrix4x3_FromRotationY(&data_020a0e68, mPrevAngleY);
            {
                Vector3 *start = rayStart;
                Vector3 *from = rayFrom;
                Vector3 *to = rayTo;
                for (i = 0; i < 3; i++) {
                    ahead = *start;
                    ahead.z += 0x28000;
                    MulVec3Mat4x3(start, &data_020a0e68, from);
                    MulVec3Mat4x3(&ahead, &data_020a0e68, to);
                    AddVec3(from, (Vector3 *)&mPosX, from);
                    AddVec3(to, (Vector3 *)&mPosX, to);
                    start++; from++; to++;
                }
            }
            {
                /* HOST ARM: one array in place of three locals, so the
                   pointer walk below stays inside the colliders. */
                dBgCh_Lin lines[3];
                {
                    s32 j = 0;
                    Vector3 *to = rayTo;
                    Vector3 *from = rayFrom;
                    dBgCh_Lin *line = &lines[0];
                    u32 hitSound = 0x1b;
                    for (; j < 3; j++) {
                        line->SetObjAndLine(*from, *to, this);
                        if (line->DetectClsn() && line->clsnDist <= 0x1f000) {
                            mHorzSpeed = 0;
                            mBumped = 1;
                            mWobbleTimer = 60;
                            Sound::PlayBank3(hitSound, *(Vector3 *)&mCamSpacePosX);
                        }
                        to++; from++; line++;
                    }
                }
                UpdatePos(0);
            }
        }
        if (!DecIfAbove0_Byte(&mTiltHoldTimer)) {
            mTargetRotation[0] = data_02092768[0];
            mTargetRotation[1] = data_02092768[1];
            mTargetRotation[2] = data_02092768[2];
            mTargetRotation[3] = data_02092768[3];
        }
    }
    Quaternion_SLerp(&mRotation[0], &mTargetRotation[0], 0x199, &mRotation[0]);
    if (Vec3_Equal((Vector3 *)&mPosX, &mHomePos))
        mResetTimer = 300;
    if (!DecIfAbove0_Short(&mResetTimer)) {
        *(Vector3 *)&mPosX = mHomePos;
        mRotation[0] = *(volatile s32 *)&data_02092768[0];
        mRotation[1] = *(volatile s32 *)&data_02092768[1];
        mRotation[2] = *(volatile s32 *)&data_02092768[2];
        mRotation[3] = *(volatile s32 *)&data_02092768[3];
        mTargetRotation[0] = *(volatile s32 *)&data_02092768[0];
        mTargetRotation[1] = *(volatile s32 *)&data_02092768[1];
        mTargetRotation[2] = *(volatile s32 *)&data_02092768[2];
        mTargetRotation[3] = *(volatile s32 *)&data_02092768[3];
        mLoweredPlatform = -1;
        mWobblePhase = 0;
        mBumped = 0;
        mWobbleTimer = 0;
        mAngleX = 0;
        mAngleZ = 0;
        mHorzSpeed = 0;
    }
    if (!mBumped && mResetTimer < 270) {
        mLoweredPlatform = -1;
        mHorzSpeed = 0;
    }
    UpdateModelTransforms();
    UpdateClsnTransforms();
    mPlayerOnMesh = 0;
    mHorzSpeed = 0;
    if (mPrevLoweredPlatform != mLoweredPlatform)
        Sound::PlayBank3(0x3e, *(Vector3 *)&mCamSpacePosX);
    mPrevLoweredPlatform = mLoweredPlatform;
    return 1;
}
