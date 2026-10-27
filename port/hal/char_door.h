/* hal/char_door.h: the Rec Room's white star door picks the character (run hunt8, lane CHARDOOR1).

   THE DOOR. The "character select room" is the Rec Room (level 50). It has four double doors: three
   character doors (door variants 9, 10, 11, the emblem doors) and a fourth, the white door with the star,
   variant 0xd (src/func_ov100_02145080.c calls Player::TryExitWhiteDoorWithStar only for it). Its lock
   is key flag 6 (data_ov100_02148204 entry 13: b9 = 6, so save flags1 bit 7 is the key and bit 14 is
   "already opened"). The ROM's own first use of it plays the key, shows "Another Power Star!" and gives
   its star (one of the DS's 150), then leaves the door standing and unlocked; from then on walking at it
   goes through.

   THE HOOK, host side only, no src/ edit. port/unmatched/Door_Behavior.cpp already owns the door's
   callback faces; its idle callback (func_ov100_02144cf8, the one that decides "locked / key / open") calls
   port_char_door_idle_hook first for the variant 0xd door. This file seats that hook:

     key flag 14 clear (the ROM has not given its star yet)        -> the ROM runs, nothing here
     opened, this save slot not unlocked yet (the star just given) -> a box "Custom character select
                                                                      unlocked", the slot is unlocked
     opened and unlocked                                           -> the player stands in front of the
                                                                      door and is walking: the picker
     multiplayer (hal/mods.h port_mods_multiplayer)                -> the ROM door, untouched

   A picker or a box holds the game still with the debug menu's own pause (menu_on in
   tests/walk_window.cpp, which owns the seat, the device reads, the drawing and the apply); this file is
   the state, the list, the input rules and the layout. After either closes the door stays shut to the
   player until they step out of its doorway (so Back leaves them standing at the door).

   THE LIST. Mario, Luigi, Wario when this save has them (SaveData::IsCharacterUnlocked, save flags2 bit n),
   Yoshi always (every file starts with him), then every installed character mod (Tango: every installed one, name and base).
   The cursor starts on the character being played. */
#ifndef PORT_HAL_CHAR_DOOR_H
#define PORT_HAL_CHAR_DOOR_H

#include <stdint.h>
#include "settings_page.h"   /* SpInput, SPK_* : the same device description the settings page takes */

#ifdef __cplusplus
extern "C" {
#endif

/* 1 while the picker or the box is up (the game is held still) */
int  port_char_door_open(void);
/* a request is waiting: the door asked for a picker or a box and the frame loop has not opened it yet */
int  port_char_door_pending(void);
void port_char_door_clear_request(void);
/* the frame loop honours the request. cur = what is being played: 0..3 plain, 4 + n the custom character n */
void port_char_door_open_now(int cur);
/* the box or picker is no longer wanted (the pause was taken away) */
void port_char_door_cancel(void);
/* the surface it will be drawn on: picture width / height (pixels); the advance and line height are unused since CHARMENU1 (the layout is in
   units of the picture height) and stay for the seat's call */
void port_char_door_set_metrics(int w, int h, int adv, int line);
/* one logic frame with this frame's device state; 1 while still up, 0 on the frame it closed */
int  port_char_door_frame(const struct SpInput *in);
/* after it closed: -1 nothing chosen, else the id the choice names (0..3 plain, 4 + n custom n); read once */
int  port_char_door_take_pick(void);

/* CHARMENU1: the picker or the box drawn onto the finished picture (0x00RRGGBB / 0xFFRRGGBB pixels, w x h, stride pixels a row). The look is
   hal/char_door_skin.cpp's: the HD UI Textures skin with the mod on, the plain skin without it. */
void port_char_door_draw(uint32_t *px, int stride, int w, int h);

/* the player was put back in the doorway by the restart a costume change made: no picker until they step out and in again */
void port_char_door_disarm(void);

/* test seams (the proof rows read these from the log lines the engine prints) */
int  port_char_door_rows(void);
int  port_char_door_cursor(void);

#ifdef __cplusplus
}
#endif
#endif
