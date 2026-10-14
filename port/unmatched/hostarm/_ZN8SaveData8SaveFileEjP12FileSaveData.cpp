/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN8SaveData8SaveFileEjP12FileSaveData.cpp under
 * #ifdef _MSC_VER; the 2026-10 main merge (749f5799a) took main's src/, where that
 * code now lives in src/engine/save/SaveData.cpp without the arm. mwccarm never saw
 * the arm, so nothing here moves a ROM byte.
 *
 * The arm sat in the middle of the method, so the host definition is the
 * whole static member: main's body text with the arm's block put back between
 * the in-use bit and the card write. Main's TU also defines this member, so
 * the TU's hostgen copy must park _ZN8SaveData8SaveFileEjP12FileSaveData.
 * port/hal/scene_boot.cpp reaches it by the flat name through an
 * /alternatename to ?SaveFile@SaveData@@SAHIPAUFileSaveData@@@Z. */
// @symbol _ZN8SaveData8SaveFileEjP12FileSaveData
#include "SaveData.h"

extern "C" int data_0209caa0[]; /* the open file; int[] is the spelling main's TU uses */

int SaveData::SaveFile(u32 fileID, FileSaveData* data)
{
    int* ip = (int*)((char*)data + 4);
    *ip = *ip | 1;
    /* PORT ONLY. The port's save states roll the game's copy of the star
     * bytes back, and the card is not rolled back with it, so a save of the
     * open file that follows could write fewer stars than the slot already
     * holds. On the cartridge the open file is read off the card and only
     * ever added to, so this changes nothing there: it ORs the stars already
     * stored in this slot into the record about to be written. The record's
     * star bytes are +0x14..+0x31, data_0209cab4 for the open file at
     * 0x0209caa0.
     *
     * Only the open file (SaveCurrentFile passes data_0209caa0 itself). Any
     * other record is written unchanged: the file select's COPY writes the
     * source's bytes over a slot that may already hold stars, and the
     * cartridge leaves that slot an exact copy. */
    {
        if ((void*)data == (void*)data_0209caa0) {
            char stored[0x44];
            if (SaveData::ReadDataFromCart(stored, 0x44, fileID) == 0) {
                int i;
                for (i = 0x14; i < 0x32; i++)
                    ((char*)data)[i] |= stored[i];
            }
        }
    }
    if (SaveData::SaveDataToCart((char*)data, 0x44, fileID) == 0)
        return 1;
    return 0;
}
