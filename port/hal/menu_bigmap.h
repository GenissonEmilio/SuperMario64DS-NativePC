/* menu_bigmap.h -- Select shows the bottom-screen map big, over the picture (run menu1, lane MAP1).
 *
 * LOGIC side (reads the ROM, called from the pad seam once a tick, single player only):
 *     menu_bigmap_pad_seam()     swallows the Select press while OneScreenMenus is on (the ROM's pause on the controller
 *                                mode screen, Stage::Behavior :229-243, never sees it), toggles the map, and hides it again
 *                                on movement / Start / a pause / a scene change. Returns 1 when the map is up.
 * DRAW side (reads no ROM memory, only the frame's bottom-screen scan-out and the state the tick latched):
 *     menu_bigmap_up()           the latched answer for this frame.
 *     menu_bigmap_draw()         dims the picture and draws the map big, centred.
 * With OneScreenMenus off nothing here does anything.
 */
#ifndef MENU_BIGMAP_H
#define MENU_BIGMAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int menu_bigmap_pad_seam(void);
int menu_bigmap_up(void);
/* sub: the 256x192 scan-out (0xAARRGGBB, row stride 256); dst: the picture, extent w x h, row stride dst_stride.
   rect_out (4 ints: x0 y0 x1 y1) receives the map's rectangle, frame included. */
void menu_bigmap_draw(const uint32_t *sub, uint32_t *dst, int w, int h, int dst_stride, int *rect_out);

#ifdef __cplusplus
}
#endif
#endif
