// port/hal/smoke_model_tail.cpp -- host storage and name bridges the model
// family harnesses (smoke_model .. smoke_clsn) need since main's folds of GX,
// Model4, ModelComponents and G3i into whole TUs. Lane INT093F.
//
// (data_02086314 and the three after it are defined by the harnesses' own
// romdata.c; only the decorated spellings are bridged below.)
// The words below are plain zero-filled storage: no harness runs the paths
// that read them (texture-VRAM bank setup, the BG palette copy) with a meaning
// that matters, and the window build owns the real tables in hal/auto_bss.cpp
// and the arm9 data mount, which a harness does not link.

extern "C" {
// CP15::EnableDTCM: src/_ZN4CP1510EnableDTCMEv.cpp is two ARM coprocessor
// instructions (mrc / mcr), which no host compiler accepts; the window build
// takes the generated copy with the asm parked. The CP15 control register has
// no host meaning, so a harness gets the empty function.
void _ZN4CP1510EnableDTCMEv(void) {}
// MultiCopyHalf: the window build compiles src/MultiCopyHalf.c under the
// per-source rename MultiCopyHalf=MultiCopyHalf_rom and keeps the ROM name in
// hal/cxx_aliases.cpp with a PPU-audit census in front. A harness has no
// audit, so the ROM name is the straight call (same body as the census-less
// path).
void MultiCopyHalf_rom(const void *src, void *dst, int size);
void MultiCopyHalf(const void *src, void *dst, int size)
{ MultiCopyHalf_rom(src, dst, size); }
// cstd::strcmp: src/cstd6.cpp (the whole string TU) drags the OS tables; the
// harnesses want the comparison only.
int _ZN4cstd6strcmpEPKcS1_(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}
// Copy48Bytes: src/Copy48Bytes.c is a hand-asm ldm/stm pair no host compiler
// accepts (the window build takes the generated copy); the function is a
// 48-byte block copy, one Matrix4x3.
void Copy48Bytes(const void *src, void *dst)
{ for (int i = 0; i < 12; ++i) ((unsigned int *)dst)[i] = ((const unsigned int *)src)[i]; }
// Copy32Bytes: the same hand-asm block copy at 32 bytes.
void Copy32Bytes(const void *src, void *dst)
{ for (int i = 0; i < 8; ++i) ((unsigned int *)dst)[i] = ((const unsigned int *)src)[i]; }
unsigned int data_020a60b4[8];
unsigned int data_020a60b8[8];
}

// THE GX BANK-STATE BLOCK, 0x020a6088..0x020a60a4, in ROM order: the same
// thirteen grouped sections hal/cxx_aliases.cpp lays out for the window build
// (read its note for why a split object is wrong), because tools/gxband_guard.py
// checks EVERY target's map and the harnesses now link the SetBankFor* family.
#define GXBANK(sec, name, size, algn)                             \
    __pragma(section(sec, read, write))                           \
    extern "C" __declspec(allocate(sec)) __declspec(align(algn))  \
    unsigned char name[size] = {0}

GXBANK(".dsstate$gxbank00", data_020a6088, 2, 4);
GXBANK(".dsstate$gxbank01", data_020a608a, 2, 2);
GXBANK(".dsstate$gxbank02", data_020a608c, 2, 2);
GXBANK(".dsstate$gxbank03", data_020a608e, 2, 2);
GXBANK(".dsstate$gxbank04", data_020a6090, 2, 2);
GXBANK(".dsstate$gxbank05", data_020a6092, 2, 2);
GXBANK(".dsstate$gxbank06", data_020a6094, 2, 2);
GXBANK(".dsstate$gxbank07", data_020a6096, 2, 2);
GXBANK(".dsstate$gxbank08", data_020a6098, 2, 2);
GXBANK(".dsstate$gxbank09", data_020a609a, 2, 2);
GXBANK(".dsstate$gxbank10", data_020a609c, 2, 2);
GXBANK(".dsstate$gxbank11", data_020a609e, 2, 2);
GXBANK(".dsstate$gxbank12", data_020a60a0, 4, 2);

#undef GXBANK

#pragma comment(linker, "/alternatename:?data_02086314@@3PAGA=_data_02086314")
#pragma comment(linker, "/alternatename:?data_02086324@@3PAGA=_data_02086324")
#pragma comment(linker, "/alternatename:?data_02086326@@3PAGA=_data_02086326")
#pragma comment(linker, "/alternatename:?data_02086328@@3PAGA=_data_02086328")
#pragma comment(linker, "/alternatename:?data_020a60b4@@3IA=_data_020a60b4")
#pragma comment(linker, "/alternatename:?data_020a60b8@@3IA=_data_020a60b8")
#pragma comment(linker, "/alternatename:?data_020a6088@@3UVramReg@@A=_data_020a6088")
#pragma comment(linker, "/alternatename:__ZN2GX13SetBankForTexEt=?SetBankForTex@GX@@YAXG@Z")
#pragma comment(linker, "/alternatename:__ZN2GX17SetBankForTexPlttEt=?SetBankForTexPltt@GX@@YAXG@Z")
#pragma comment(linker, "/alternatename:__ZN3G3X13SetClearColorEtiiib=?SetClearColor@G3X@@SAXGHHH_N@Z")
#pragma comment(linker, "/alternatename:__ZN4cstd11fdiv_resultEv=?fdiv_result@cstd@@YAHXZ")
#pragma comment(linker, "/alternatename:__ZN4cstd11ldiv_resultEv=?ldiv_result@cstd@@YA_JXZ")
// data_0208e87c is the port's old name for ModelBase's vtable at arm9
// 0x0208e87c; config now calls it _ZTV9ModelBase and hal/model_host.cpp defines
// the storage under that name. port/unmatched/ModelBaseModel_Dtors_HostCopy.c
// still spells the old one; the window build does not compile that file, the
// harnesses do. One table, two names (the row the PORTSYNC3 alias cleanup
// removed from smoke_aliases_model.cpp with the window build's copy).
#pragma comment(linker, "/alternatename:_data_0208e87c=__ZTV9ModelBase")
