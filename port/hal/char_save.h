/* hal/char_save.h: the player's character pick survives quitting (run hunt8, lane CHARSAVE1).

   A side file next to the cartridge save, save/characters.json (or, when
   SM64DS_SAVE_PATH names the save, that path plus ".characters.json"), holds per
   save file slot (A, B, C = 0, 1, 2):

     "unlocked"   the "custom character select" flag (set by the white door lane)
     "character"  the chosen costume's full key "<modid>:<key>", or "" for the
                  plain character
     "base"       the base character the pick was made on: mario luigi wario yoshi

   plus "lastSlot", the file played last. The cartridge save (save/sm64ds.sav)
   is never touched, so a retail save stays byte for byte what the ROM wrote.

   WHEN IT IS READ. The file layer is boot-latched (hal/char_registry.h), so the
   skin has to be known at process start. A boot with no SM64DS_CHAR_SKIN uses
   the saved pick of SM64DS_CHAR_SLOT when the debug menu's relaunch set it, else
   of lastSlot. When the player then picks a file whose saved skin differs from
   the one this process started with, the title hands over to a new process
   through the front door (hal/title_entry.cpp, SM64DS_BOOT_FILE) with the right
   skin: the same one-way restart the debug menu's character row already uses.

   A saved key whose mod is missing, off or too old falls back to the plain
   character with one log line, and the entry is kept so that putting the mod
   back brings the costume back. Multiplayer ignores all of it (the base
   character is forced there, as hal/char_registry.h says). */
#ifndef PORT_HAL_CHAR_SAVE_H
#define PORT_HAL_CHAR_SAVE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The pick the boot should apply: 1 and out = the saved key ("" = plain) when
   one is recorded for the boot's slot, else 0. Cheap, may be called early. */
int port_char_save_boot_pick(char *out, size_t cap);

/* The save file slot (0..2) this process is playing: SM64DS_CHAR_SLOT when the
   debug menu's relaunch set it, else the ROM's own picked slot. */
int port_char_save_slot(void);

/* The pick changed (debug menu row, later the white door's picker): key is the
   full key or "" for plain, base 0..3. Written at once. No-op in multiplayer. */
void port_char_save_record(int slot, const char *key, int base);

/* The "custom character select" unlock flag of a slot (CHARDOOR1 uses these). */
int port_char_save_unlocked(int slot);
void port_char_save_set_unlocked(int slot, int on);

/* The saved key of a slot ("" = plain or none). Pointer valid until the next call. */
const char *port_char_save_key(int slot);

/* Environment for a child that is about to be started on the same slot, with the
   given skin key ("" = plain). */
void port_char_save_child_env(int slot, const char *key);
/* FIELDPLAY1: 1 when this process is playing a real save file (the title picked it, or it is a restart for one) */
int port_char_save_file_known(void);

#ifdef __cplusplus
}
#endif
#endif
