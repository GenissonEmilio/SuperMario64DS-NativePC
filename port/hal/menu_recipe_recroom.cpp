/* menu_recipe_recroom.cpp -- the REC ROOM select as one-screen menu recipe "recroom" (run menu1, lane RECROOM1).
 *
 * Tango (10-09): "we literally only need the bottom screen". Scene 5 (dScMiniGm_c, ov005; hal/scene_mg_menu.cpp seats
 * it). The upper screen (Toad's room, the "TOUCH TO SELECT" band) is not shown at all; the lower screen (engine B, the
 * unswapped layout: DISPCNT_B 00011d00, BG0 cards, BG2 the tab column's strip, BG3 the star pattern, OBJ tabs, star
 * counts, NEW! stickers, the page arrow and the back arrow; BG1 is off) is the whole menu.
 *
 * LAYOUT (canvas = 192 units tall, 256 wide at 4:3; nothing is stretched, every piece is 1:1):
 *   - the background is the BG3 star pattern ALONE, as SRC_SUB pieces of 224 columns tiled across the canvas from
 *     its left edge (the pattern's period is 32 columns and divides 224, so a copy is seamless at every join); the last
 *     copy is one unit wide of the exact edge (a piece may overrun by a unit and is clamped), so no sliver of the
 *     hidden upper screen shows;
 *   - on top, ONE piece: BG0 + BG2 + OBJ of the whole lower screen, the engine-B backdrop keyed out, so the cards,
 *     tab column, stickers, arrows and star counts keep their own transparency and drag no background along. It
 *     is centred (the DS arrangement). On a wide picture that leaves the pattern continuing out to both sides.
 *     DECISION (RECROOM1): centred, not tabs-left / arrow-right. The page scroll slides the cards sideways under a
 *     clip at the screen's own edges (left: under the tab column, which is the ROM's own look; right: the 256th
 *     column). Split pieces would add a second invisible clip in the middle of the wide picture; one block keeps
 *     the ROM's motion exactly and the block edges are the DS screen's.
 *
 * INPUT. The Rec Room is touch only in the ROM. Hotspots are the ROM's own boxes, cited per line below; they follow the
 * live page: the card cells that the ROM would accept (func_ov005_020c00b4: the first two of a group are free, the rest
 * need an unlock bit), the page arrow only where the ROM draws and accepts it (not while a page flip is running). Driver
 * TOUCH: the pad / keys move a focus over the hotspots (automatic spatial navigation), A synthesises the touch at
 * the focused box's centre, B / Backspace presses the back arrow (MenuLayout.back). Mouse clicks map through the touch
 * piece to the ROM's boxes exactly (1:1). Focus highlight: the core's own (the focused item's pixels brightened and
 * outlined), no new art.
 */
#include "menu_compose.h"

extern "C" {
void *port_scene_live_object(void);
int  port_scene_live_id(void);
int  func_ov005_020c00b4(void *self, int n);
int  func_ov005_020c00e4(void *self);
extern unsigned char data_020a0e40;     /* the local player's slot */
extern int data_020a0e58[];             /* PadData[i]: {u16 held, u16 pressed} */
extern unsigned char data_0209b304[];   /* the page mode: 0 = the 2 x 3 page, else the single row of three */
extern int data_0208a170;               /* the highlighted group's base index (0..3 = Yoshi / Mario / Luigi / Wario) */
}

namespace {

const int kScene = 5;
const int kBackX = 0xd8, kBackY = 0x90, kBackW = 0x21, kBackH = 0x21;   /* src/func_ov005_020c0250.cpp:25-31 */

inline int ow(const void *o, int off) { return *(const int *)((const char *)o + off); }
inline int ob(const void *o, int off) { return *(const unsigned char *)((const char *)o + off); }

}  // namespace

extern "C++" {

int menu_recroom_up(int scene_id)
{
    return scene_id == kScene ? 1 : 0;
}

void menu_recroom_build(int, MenuLayout *L)
{
    const void *o = port_scene_live_id() == kScene ? port_scene_live_object() : 0;
    const int cw = L->w * L->uden / L->unum;               /* exact canvas width in units, floored */

    /* the star pattern, BG3 alone, tiled from the left edge in 224 column copies of source columns 32..255: the pattern
       repeats every 32 columns and 224 = 7 x 32, so a copy joins the next seamlessly; the leftmost 8 source columns are
       not part of the repeat (measured, they differ from columns 32..39) and are never used */
    for (int x = 0; x < cw; x += 224) {
        int sw = cw - x + 1;                               /* one unit past the exact edge: clamped, never a sliver */
        if (sw > 224) sw = 224;
        MenuPiece t = menu_piece_make(SRC_SUB, 32, 0, sw, 192, MENU_AX_L, MENU_AY_T, x, 0, 1, 1);
        t.layers = MENU_L_BG3;
        t.bg = 1;
        menu_add_piece(L, t);
    }

    /* the menu itself: tab column, cards, stickers, star counts, arrows (BG0, BG2, OBJ), centred */
    MenuPiece m = menu_piece_make(SRC_SUB, 0, 0, 256, 192, MENU_AX_C, MENU_AY_M, 0, 0, 1, 1);
    m.layers = MENU_L_BG0 | MENU_L_BG2 | MENU_L_OBJ;
    m.key_backdrop = 1;
    m.touch = 1;
    m.bg = 1;                                              /* it overlaps the pattern pieces by design */
    const int pm = menu_add_piece(L, m);

    /* the ROM's boxes (inclusive byte compares in the cited source, so a box is one wider than the span) */
    /* category tabs: x <= 0x30, tab i y 2 + 0x30 i .. 0x2e + 0x30 i (src/func_ov005_020c0878.cpp:60-71) */
    for (int i = 0; i < 4; ++i) menu_add_hotspot(L, pm, 0, 2 + 0x30 * i, 0x31, 0x2d, -1);
    /* back arrow: x 0xd8..0xf8, y 0x90..0xb0 (src/func_ov005_020c0250.cpp:25-31) */
    L->back = (int8_t)menu_add_hotspot(L, pm, kBackX, kBackY, kBackW, kBackH, -1);

    /* no live object (the layout selftest): the 2 x 3 page with every card and the next-page arrow */
    const int mode = o ? data_0209b304[0] : 0;
    const int base = o ? data_0208a170 : 0;
    /* game cells (src/func_ov005_020c0378.cpp:77-90): the ROM accepts a cell whose game is unlocked (c00b4) */
    if (mode == 0) {
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 3; ++col) {
                if (o && !func_ov005_020c00b4((void *)o, base + row * 0xc + col * 4)) continue;
                menu_add_hotspot(L, pm, 0x40 + 0x30 * col, 0x18 + 0x48 * row, 0x30, 0x48, -1);
            }
    } else {
        for (int col = 0; col < 3; ++col) {
            if (o && !func_ov005_020c00b4((void *)o, base + 0x18 + col * 4)) continue;
            menu_add_hotspot(L, pm, 0x60 + 0x30 * col, 0x38, 0x30, 0x48, -1);
        }
    }
    /* page arrow: drawn only while no flip is running and, on the first page, only if some page two exists
       (src/_ZN11dScMiniGm_c6RenderEv.cpp:84-110); boxes src/func_ov005_020c06cc.c:30-45 */
    const bool arrow = !o || (ob(o, 0x54) == 0 && (mode != 0 || func_ov005_020c00e4((void *)o) != 0));
    if (arrow) {
        if (mode == 0) menu_add_hotspot(L, pm, 0xd8, 0x40, 0x21, 0x41, -1);
        else menu_add_hotspot(L, pm, 0x38, 0x40, 0x21, 0x41, -1);
    }
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
}

/* The ROM reads the touch only while it is not flipping a page, exiting or fading out its choice
   (the guards at the head of func_ov005_020c0250 / 0378 / 06cc / 0878); the pad is not read at all.
   Called once a frame by hal_menu_pad while this recipe is live. It is also where the pad's B goes: the port's
   B button is the DS A key (0x0001, "the punch"), which this scene never reads, so a press of it is taken
   here as the back arrow (a synthesised touch at the back box's centre) and stripped. Backspace takes the
   core's own host_back path. */
int menu_recroom_pad_ok(void)
{
    const void *o = port_scene_live_object();
    if (!o || port_scene_live_id() != kScene) return 0;
    if (ob(o, 0xac) != 0 || ob(o, 0x54) == 1) return 0;
    if (ow(o, 0x90) > 0 || ow(o, 0x94) > 0 || ow(o, 0x98) > 0) return 0;
    unsigned short *pressed = (unsigned short *)((char *)data_020a0e58 + data_020a0e40 * 4 + 2);
    if (*pressed & 0x0001) {
        hal_menu_touch_queue_press(kBackX + kBackW / 2, kBackY + kBackH / 2);
        *pressed = (unsigned short)(*pressed & ~0x0001u);
    }
    return 1;
}

}  // extern "C++"
