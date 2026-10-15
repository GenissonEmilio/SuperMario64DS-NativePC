/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN9dScDSMT_c8BehaviorEv.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/stages/d_s_dsmt.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 *
 * The arm sat in the middle of the method, so the host definition is the
 * whole member: main's body text with the arm's slot-passing calls in place of
 * the mwcc (void) calls. Main's TU also defines this member, so the TU's
 * hostgen copy must park _ZN9dScDSMT_c8BehaviorEv. */
// @symbol _ZN9dScDSMT_c8BehaviorEv
#include "dScDSMT_c.h"
#include "types.h"
#include "Sound.h"
#include "SaveData.h"
#include "Heap.h"
#include "decl_common.h"

/* include/SaveData.h only forward-declares the 0x44-byte save file; main's
 * TU (src/game/stages/d_s_dsmt.cpp) defines it locally, and this is that
 * definition, needed here only for the `data_0209b33c + idx` stride. */
struct FileSaveData {
    u32 magic8000;
    u32 flags1;
    u32 flags2;
    u32 minigameRabbits;
    u32 cannonUnlocked;
    u8  stars[30];
    u8  coinRecords[15];
    u8  currentCharacter;
    u8  controllerMode;
    u8  unk43;
};

extern "C" {
extern FileSaveData *data_0209b33c;
extern int data_0209caa0[];
/* Linker-defined overlay IDs: the address is the overlay number. */
extern int OVERLAY_100_ID;
extern int OVERLAY_102_ID;

int func_ov007_020b7090(u16 a0, u16 a1, u16 a2, u16 a3, int arg4);
unsigned short DecIfAbove0_Short(unsigned short *p);
void StartMinigameMenu(u8 returnToRecRoom);
}

s32 dScDSMT_c::Behavior()
{
    int result;

    if (data_ov007_02103260 >= 0 && DecIfAbove0_Short(&data_ov007_02104c28) == 0) {
        Sound::LoadAndSetMusic_Layer1(data_ov007_02103260);
        data_ov007_02103260 = -1;
    }

    /* The ROM leaves each func_0203da9c() result, the comms slot index, in r0
       for the accessor after it (0x020cc310..0x020cc320); pass it. */
    {
        u16 *p = func_0203dabc(func_0203da9c());
        int arg4;
        arg4 = func_0203dae4(func_0203da9c());
        result = func_ov007_020b7090(p[0], p[1], p[2], p[3], arg4);
    }

    if ((unsigned int)(result - 3) <= 2) {
        int idx = 0;
        if (result == 4) {
            idx = 1;
        } else if (result == 5) {
            idx = 2;
        }
        func_02013c84(idx, data_0209b33c + idx, -1, data_0209caa0);
        StartFile(1, 0);
    } else if (result == 6) {
        if (data_0209d524 == 0) {
            Heap *h1 = (Heap *)func_0201a458();
            Heap *h2 = h1->SetDefault();
            LoadArchive(1);
            LoadTextNarcs();
            h1->Rescue();
            h2->SetDefault();
            LoadOverlay((int)&OVERLAY_100_ID);
            LoadOverlay((int)&OVERLAY_102_ID);
        }
        dScene_c::StartSceneFade(6, 0, 0x7fff);
    } else if (result == 7) {
        StartMinigameMenu(0);
    } else if (result == 2) {
        dScene_c::StartSceneFade(1, 0, 0x7fff);
    }

    dScene_c::SetFaders(&fader);
    return 1;
}
