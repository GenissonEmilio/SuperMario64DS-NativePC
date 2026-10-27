/* menu_join_plate.h -- the "Joining" plate of a join successor (run after093, lane JOINFLOW1).

   When the player picks an online file the game asks the launcher for a server and starts itself again at its front door with the hub environment
   (hal/online_join.cpp). That restarted game ("the successor") walks the ROM's own title, main menu and file select to the chosen file (hal/title_entry.cpp,
   SM64DS_BOOT_FILE). The walk is the ROM's and still runs, but none of it is shown: from the successor's first picture until the adventure is entered every
   picture the game presents is black (JOINWIN1: the whole-window plate is gone), with one small corner line "Joining..." once the join has taken more than
   3 s. B or Esc stops the join: the game starts again as a clean solo title (the join environment cleared) and this process ends. The process that started
   the successor holds black over its own last pictures (port_join_plate_raise_black), so the window goes from the file select to black to the level.

   The plate replaces the PICTURE at the one place every picture leaves the game (tests/walk_window.cpp present_body); nothing underneath is drawn differently.
   Only a game started by online_join.cpp carries the marker (SM64DS_JOIN_QUIET); the launcher's own Multiplayer-window starts never do, so they show what they
   always showed. */
#ifndef PORT_HAL_MENU_JOIN_PLATE_H
#define PORT_HAL_MENU_JOIN_PLATE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 1 while the plate covers the pictures (a join successor, from its first picture to the adventure's entry, or until it has started the solo title) */
int  port_join_plate_up(void);
/* the plate as a w x h picture (0xAARRGGBB, stride w); built once per size. Null when the plate is not up. */
const uint32_t *port_join_plate_picture(int w, int h);
/* a left click at (cx, cy) in the picture's pixels. 1 = the black took it (nothing reaches the game; there is nothing to click). */
int  port_join_plate_click(int cx, int cy);
/* once a frame while the plate is up: Esc and the game's B (attack key / pad B) held or not. A fresh press cancels. */
void port_join_plate_input(int esc_down, int b_down);
/* JOINWIN1: the parent of a join successor: black over this process's last pictures (no input, no cancel) */
void port_join_plate_raise_black(void);
/* 1 on most frames while the plate is up and the walk is hidden: the frame pacer may be skipped (the walk then takes a second or two, not ten) */
int  port_join_plate_fast(void);
/* a line of effective settings and window facts for the settings comparison (once per process, on its first picture) */
void port_join_settings_dump(int client_w, int client_h, int fullscreen, int maximised, int hand_sized);
#ifdef __cplusplus
}
#endif
#endif
