/* menu_hostimg.h -- a host-resolution picture for the one-screen menus (run menu1, lane ASSET2).
 *
 * The first user is the "Super Mario 64 PC" logo of the title main menu. The PNG is decoded once (stb_image, the
 * reader hdtex and the minimap already use), premultiplied, and scaled with a separable triangle/area filter to the
 * exact destination size, cached per size. Pixels are PREMULTIPLIED 0xAARRGGBB so a draw is dst = src + dst * (255 - a) / 255
 * and the colour of a transparent texel can never bleed into an edge.
 *
 * WHERE THE FILE COMES FROM, first hit wins:
 *   SM64DS_MENU_LOGO=<path>        outright; a path that cannot be read means "no logo" (the DS logo is then drawn)
 *   <asset root>/menu/<name>.png         the kit file beside the exe (logo_sm64_pc.png, btn_mp.png, btn_adventure.png, btn_rec.png)
 *   the copy embedded in the exe (hal/menu_logo_data.cpp, generated from res/menu/logo_sm64_pc.png)
 * Nothing here can fail loudly: no picture = menu_hostimg_ready() is 0 and the recipe keeps the DS logo layer. */
#ifndef MENU_HOSTIMG_H
#define MENU_HOSTIMG_H
#include <stdint.h>
enum { MENU_IMG_LOGO = 1, MENU_IMG_BTN_MP = 2, MENU_IMG_BTN_ADV = 3, MENU_IMG_BTN_REC = 4, MENU_IMG_COUNT = 5 };
#ifdef __cplusplus
extern "C" {
#endif
int menu_hostimg_ready(int id);                                   /* 1 once the picture decoded */
int menu_hostimg_size(int id, int *w, int *h);                    /* source pixel size */
const uint32_t *menu_hostimg_scaled(int id, int dw, int dh);      /* premultiplied ARGB, dw*dh, cached; null on failure */
#ifdef __cplusplus
}
#endif
#endif
