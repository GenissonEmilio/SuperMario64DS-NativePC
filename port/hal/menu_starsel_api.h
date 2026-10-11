/* menu_starsel_api.h -- where the big stars of the star select sit (run menu1, lane STARHOOK1; for mp1's lane SSWHO1).
 *
 * A FROZEN, read-only geometry query. mp1 draws small heads / names of the players in each star in its own pass after the
 * frame; menu1's recipe stays free of netcode and only answers "where is star i". Works with OneScreenMenus ON (the one-screen
 * recipe: the upper screen only) and OFF (the ROM's own star select on the DS upper screen). In both the big stars are the same
 * 3D models on the same upper-screen picture, so one geometry serves both; what differs is how the picture is put on the window
 * (present scaling, letterbox, the stacked layout's top band), and that is folded in by hal_menu_starsel_star_rect.
 *
 * THREADING: call from the LOGIC thread (the one that runs the ROM tick), any time after the tick. Where a one-screen decision is
 * live the values are that decision's facts (the last tick the player saw, MENUSPLIT1 contract); with OneScreenMenus off there is
 * no decision, so the few object fields are read on the logic thread here. A render thread must not call these: copy the
 * answers into its frame packet at the end of the tick. Nothing is cached, nothing is allocated; when the star select is not up
 * each call is one scene-id compare.
 *
 * THE STAR ORDER is the ROM's: index 0 is the left-most star (act 1), left to right. count is the number of big stars shown.
 * A star the player has not earned is still a star on screen (a dark one); an act's index is its position. During the fade in /
 * out and the intro (the stars drop in) the rects are the settled positions.
 * The selected star is drawn 1.25x larger by the ROM and its rect is larger by the same factor. */
#ifndef MENU_STARSEL_API_H
#define MENU_STARSEL_API_H

#ifdef __cplusplus
extern "C" {
#endif

int hal_menu_starsel_up(void);          /* 1 while the star / act select is the live screen (ON or OFF layout), else 0 */
int hal_menu_starsel_count(void);       /* number of big stars shown (0 when not up, or on a course that shows none) */
int hal_menu_starsel_selected(void);    /* selected star index 0..count-1, -1 when unknown */
/* 1 + the star's rect in WINDOW CLIENT PIXELS of the picture the player sees now (present scaling and letterbox applied, the
   stacked layout's top band included), for the current layout. 0 if not up, no such star, or nothing has been presented. */
int hal_menu_starsel_star_rect(int star, int *x, int *y, int *w, int *h);

/* The same rect in PICTURE pixels: the upper-screen picture the game composes each tick (the surface the F3 / menu / toast overlays
   paint on; width ntr::active_w, height ntr::active_h). Added for painters that draw before the present scaling. Same returns. */
int hal_menu_starsel_star_rect_pic(int star, int *x, int *y, int *w, int *h);

#ifndef PORT_ROM_CLEAN
/* Debug (SM64DS_STARSEL_RECT_DEBUG=1; compiled out under PORT_ROM_CLEAN): outlines the picture-space rects on an overlay surface
   (0xAARRGGBB, `stride` words per row; the same surface the menu / toast painters get). Does nothing with the env unset. */
void hal_menu_starsel_debug_draw(unsigned *px, int stride);
#endif

#ifdef __cplusplus
}
#endif
#endif
