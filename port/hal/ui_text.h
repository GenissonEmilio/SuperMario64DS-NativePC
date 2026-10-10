/* ui_text.h -- the one-screen menus' text at WINDOW resolution (run menu1, lane SETMENU1).

   The DS lettering font (sm64ds_original.otf, the launcher's own) is drawn at any size: tools/prepare_settings_art.py bakes the
   printable ASCII glyphs once into an 8-bit coverage atlas at 96 px per em (res/menu/settings/font.png, embedded like the logo,
   decoded with stb_image) and this file area-averages a glyph down to the size asked for, keeps the result in a small cache keyed by
   (character, whole pixel size), and blends it into a 32 bit picture. A shrink is a box average of the atlas, so a size from 12 to
   about 200 px stays crisp and nothing is drawn per frame but the cached coverage (one multiply and blend per covered pixel).
   (stb_truetype is not used: the single header was not available to add; the baked atlas gives the same result for a fixed font.)

   Sizes are the em in pixels; the cap height is 0.75 em and the lower case x height 0.5 em. Pure host code: no ROM, no window. */
#ifndef UI_TEXT_H
#define UI_TEXT_H
#include <stdint.h>

struct UiSurface { uint32_t *px; int stride, w, h; };    /* 0x00RRGGBB / 0xFFRRGGBB pixels, row-major; draws write opaque pixels */

int   ui_text_ready(void);                                       /* 1 once the atlas decoded */
float ui_text_width(const char *s, float em_px);                 /* advance of the whole string, in pixels */
float ui_text_cap_height(float em_px);                           /* the height of a capital above the baseline */
/* draw s with its pen at (x, baseline_y); rgb = 0xRRGGBB, alpha 0..255; clip = {x0, y0, x1, y1} (exclusive) or null for the surface.
   Returns the pen's x after the string. */
float ui_text_draw(const UiSurface *dst, float x, float baseline_y, const char *s, float em_px, uint32_t rgb, int alpha, const int *clip);
int   ui_text_cache_count(void);                                 /* glyph bitmaps cached (for the cost line) */
#endif
