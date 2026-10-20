/* hal/char_strip.h: the character strip on the star select (run hunt8, lane PRECOURSE1).

   Tango: "Do character select but just before course like how Yoshi caps usually sit. And have it primed for when custom characters are added an
   arrow appears if you have any that lets you scroll to the next 3 selectable characters."

   WHERE. The star select is the screen every painting shows just before a course, and the ROM already puts a row of character heads on it, but
   only when the character being played is Yoshi (src/actors/dScStarSel_c.cpp: the "strip", STRIP_MODE, three small heads under the big one on
   the lower screen). With the one-screen menus on, the lower screen is not drawn, so this lane draws ONE row of heads for every character, in the
   free band at the bottom of the upper picture, between the lives counter and the star counter. It replaces the ROM's Yoshi row there (the Down
   key is taken here for every character, so the ROM's own row never opens).

   WHO. Mario always, then Luigi, Wario and Yoshi when this save has them (the ROM's own rule, save flags2; a locked one is not drawn, as the ROM
   draws none). One more slot, an arrow, appears only when at least one character mod is installed (hal/char_registry.h): it pages through the
   installed characters three at a time, and after the last page back to the four. A slot is the character's head (the HUD life icon for a ROM
   character, the mod's own icon or preview for a costume, else its base's head) on a wooden plank; the character being played is on the cream plaque
   with a star; the name of the slot under the cursor or the pointer is on a cream plaque above it. The HD UI Textures mod's art when it is on,
   the plain skin when it is not (hal/char_door_skin.cpp).

   INPUT. Down (D-pad, stick, key) moves the cursor onto the strip; Left / Right move along it (and onto the arrow); A / Enter / Space picks (or
   pages, on the arrow); Up, B or Escape give the cursor back to the stars. The strip holds every pad bit away from the ROM while the cursor is on
   it, and for the frames until the pad is released after it leaves, so no press of the strip also moves a star or starts the act. The mouse
   hovers and clicks a slot; a stylus tap (a touch screen) is the same click. Nothing here fires in multiplayer (the base character is forced there).

   THE PICK. A ROM character with no costume active is the in-place change the debug menu's character row makes between two courses: the save
   byte and data_02092128 (the pair StartFile writes), so the course that starts next spawns that character. A costume, or leaving a costume, is a
   change of SKIN, which the file layer latches at process start (hal/char_registry.h): the pick is remembered, the base is set at once, and when the
   act is confirmed the game starts the course again in a new process with the skin (the same one-way restart the white door's picker makes), at the
   same course, entrance and act. */
#ifndef PORT_HAL_CHAR_STRIP_H
#define PORT_HAL_CHAR_STRIP_H

#include <stdint.h>
#include "settings_page.h"   /* SpInput, SPK_* */

#ifdef __cplusplus
extern "C" {
#endif

/* 1 while the strip is part of the screen: the star select is up, the one-screen menus are on, nothing multiplayer, and there is a choice to make */
int  port_char_strip_up(void);
/* 1 while the strip is up and the cursor is on it (the pad pointer stays off then: A picks) */
int  port_char_strip_focus(void);
/* one logic frame with this frame's device state. Returns the DS key bits (the host's btn word: 0x10 right, 0x20 left, 0x40 up, 0x80 down, 0x08
   start, 0x04 select, and the low shared bits) to take away from the ROM this frame; 0 when the strip is not up. */
unsigned port_char_strip_frame(const struct SpInput *in);
/* a frame outside the star select: forgets the screen that was up (a few instructions when there was none) */
void port_char_strip_idle(void);
/* the picture overlay: draws onto the finished upper picture (0x00RRGGBB / 0xFFRRGGBB, stride pixels a row); does nothing when the strip is not up */
void port_char_strip_draw(uint32_t *px, int stride, int w, int h);
/* after a frame: -1 nothing picked, else the id picked (0..3 plain, 4 + n custom n); read once */
int  port_char_strip_take_pick(void);
/* the id the strip shows as the one being played (0..3, 4 + n) */
int  port_char_strip_current(void);
void port_char_strip_set_current(int id);
/* the pick waiting for the act: id (0..3 plain, 4 + n custom n) whose SKIN differs from the one this process runs, else -1 */
int  port_char_strip_skin_pending(void);
/* the frame loop tells the strip what a pick did: id when its skin change waits for the act, else -1 */
void port_char_strip_set_pending(int id);
/* the star select is over (the scene is gone): forget everything for the next one */
void port_char_strip_reset(void);
/* SNDCAM1 (S4): a character was picked on the star select (the strip, or the ROM's caps row): the select sound and that character's voice,
   the clip heard on unlocking them (ch 0..3 Mario Luigi Wario Yoshi) */
void port_char_strip_pick_sound(int ch);

/* test seams, read from the log lines the proof rows grep */
int  port_char_strip_slots(void);
int  port_char_strip_page(void);

#ifdef __cplusplus
}
#endif
#endif
