//cpp
/* leftovers:
 * func_020396dc, func_02039794, func_020397b8 and func_020397dc stay free
 * functions. The ITCM DetectClsn overrides call them by address.
 * func_020396dc turns a prism pointer into a triangle index,
 * func_02039794 classifies the slope band, func_020397b8 tests wall facing,
 * func_020397dc guards a near-zero divisor. The destructor is defined
 * first (D2, then D0, then D1) and the constructor after it. The
 * DetectClsn triple and the geometry slots live in dBgW_Kc_itcm.cpp.
 */
/* dBgW_Kc: static-mesh collider, the base KcMbg and KcMbgSclY share.
 * Type name 0x020993bc, typeinfo 0x020993c8, vtable 0x020993dc.
 */
#include "dBgW_Kc.h"
#include "dBgW.h"

extern "C" {
/* Shared base init, defined in its own TU (src/engine/collision/dBgW.cpp). */
void func_02039624(dBgW *self);
}

// @symbol _ZN7dBgW_KcC1Ev
// @symbol _ZN7dBgW_KcC2Ev
dBgW_Kc::dBgW_Kc()
{
    kclFile = 0;
}

// @symbol _ZN7dBgW_KcD2Ev
// @symbol _ZN7dBgW_KcD0Ev
// @symbol _ZN7dBgW_KcD1Ev
dBgW_Kc::~dBgW_Kc() {}

// @symbol func_020397dc
extern "C" int func_020397dc(int x)
{
    if (x <= 8 && x >= -8) return 1;
    return 0;
}

// @symbol func_020397b8
extern "C" int func_020397b8(int x)
{
    if (x < 0x600) {
        if (x > -0xccc) return 1;
    }
    return 0;
}

// @symbol func_02039794
extern "C" int func_02039794(int x)
{
    if (x > 0x600) return 0;
    if (x > -0xccc) return 1;
    return 2;
}

/* The four header words of a fresh KCL file are file-relative offsets;
   rebase them into pointers. */
// @symbol _ZN7dBgW_Kc17UpdateFileOffsetsER8KCL_File
void dBgW_Kc::UpdateFileOffsets(KCL_File &file)
{
    file.positions = (s32 (*)[3])((char *)&file + (int)file.positions);
    file.normals = (s16 (*)[3])((char *)&file + (int)file.normals);
    file.tris = (KCL_Tri *)((char *)&file + (int)file.tris);
    file.unk_0c = (char *)&file + (int)file.unk_0c;
}

// @symbol _ZN7dBgW_Kc7SetFileEP8KCL_FileR10CLPS_Block
void dBgW_Kc::SetFile(KCL_File *file, CLPS_Block &clpsBlock)
{
    func_02039624(this);
    kclFile = file;
    clps = clpsBlock;
    unk_28 = 0;
    unk_2c = 0x1000;
    unk_30 = 0;
    unk_34 = 0;
    unk_35 = 0;
    unk_38 = 0x1000;
    unk_3c = 0;
    unk_40 = 0;
    unk_44 = -0x1000;
    unk_48 = 2;
    unk_4c = 1;
    unk_4d = 0;
}

// @symbol func_020396dc
extern "C" unsigned func_020396dc(int **p, unsigned x)
{
    return (x - (unsigned)p[8][2]) >> 4;
}

// @symbol _ZN7dBgW_Kc9Virtual08Ev
void dBgW_Kc::Virtual08()
{
}
