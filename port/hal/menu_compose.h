/* menu_compose.h -- the one-screen menu compose layer (run menu1, lane P0CORE).
 *
 * WHAT THIS IS. A general, ROM-free layer that takes pieces of the two DS
 * screens (top 2D layers, the top picture, the bottom screen), places them on
 * ONE picture of any size and aspect, draws them, and turns a host click or a
 * pad press back into the DS touch the ROM already understands. The
 * level-clear save prompt (SaveMenuOnTop, hal/sub_screen.cpp swap_*) is the
 * proof of concept this generalises; nothing here calls into that code and
 * nothing in the frame calls this yet (phase 1 wires four hooks, see
 * runs/menu1/in/DESIGN.md section D).
 *
 * THE UNIT. One canvas unit is one DS pixel scaled by h / 192, kept as the
 * reduced fraction unum / uden (as swap_geom_for does) so the draw and the
 * inverse are handed small numbers. canvas_w = w * 192 / h: 256 at 4:3, 307 at
 * 16:10, 341 at 16:9, 448 at 21:9, 683 at 32:9. The canvas is 192 units tall.
 * A recipe builds a NARROW layout below MENU_WIDE_CANVAS (300) and a WIDE one
 * at or above it; wide spreads through anchors and offsets, it never stretches.
 *
 * COORDINATES. A rect in a *source* is DS pixels of that source (DsRect). A
 * piece's destination is an integer HOST rect in dest[i] = {x, y, w, h}; the
 * draw and the inverse both read it (one arithmetic): host X inside the rect
 * maps to DS x = s.x + (X - x0) * s.w / dw, floored.
 *
 * PIXELS are 0xAARRGGBB; alpha 0 is "nothing here".
 */
#ifndef MENU_COMPOSE_H
#define MENU_COMPOSE_H

#include <stdint.h>

#define MENU_WIDE_CANVAS 300      /* canvas_w at or above this takes the wide variant */
#define MENU_MAX_PIECES  16
#define MENU_MAX_HOTSPOTS 24

/* layer bits for MenuPiece.layers and the owner byte of a MenuTopCell:
   bit 0..3 = BG0..BG3, bit 4 = OBJ. (The brief wrote OBJ=4 as an index; as a
   MASK it is bit 4 so BG3 and OBJ never collide.) */
enum { MENU_L_BG0 = 1, MENU_L_BG1 = 2, MENU_L_BG2 = 4, MENU_L_BG3 = 8, MENU_L_OBJ = 16,
       MENU_L_ALL = 31 };

enum MenuSrc   { SRC_TOP_2D,      /* engine-A cells lifted out by layer owner, kept in DS space */
                 SRC_TOP_PIC,     /* the tick framebuffer (3D + 2D) at host resolution */
                 SRC_TOP_HOSTROW, /* legacy: lc_ov host-space band, moved by rows only */
                 SRC_SUB };       /* engine-B scan-out with a layer mask */

enum { MENU_AX_L = 0, MENU_AX_C = 1, MENU_AX_R = 2 };
enum { MENU_AY_T = 0, MENU_AY_M = 1, MENU_AY_B = 2 };

struct DsRect  { short x, y, w, h; };               /* DS 256x192 coordinates */

struct MenuPiece {
  MenuSrc src; uint8_t layers;                      /* bits as MENU_L_* */
  DsRect s; uint8_t autocrop;                       /* shrink s to the opaque bbox of `layers`, latched at open */
  uint8_t key_backdrop;                             /* drop engine-B backdrop pixels */
  int (*row_ok)(int sy);                            /* optional row filter (menu_plate_row today); nonzero = draw the row */
  uint8_t ax, ay; short dx, dy;                     /* anchor (L/C/R, T/M/B) + offset in canvas units (DS px) */
  uint8_t knum, kden;                               /* scale (1/1 = native DS size); allowed 1, 3/4, 2/3, 1/2 */
  uint8_t touch;                                    /* clicks on it map through to a DS touch (SRC_SUB only) */
  uint8_t bg;                                       /* P0CORE: a backdrop piece; exempt from the no-overlap rule */
};

struct MenuHotspot { uint8_t piece; DsRect box;     /* bottom-screen DS box = the ROM's own touch box */
                     int8_t rom_index;              /* >=0 ROM cursor index, -1 touch only */
                     int8_t nav[4]; };              /* indexed by MENU_DIR_*: -2 automatic spatial navigation,
                                                       -1 blocked, >=0 hotspot index to go to */

enum { MENU_DIR_UP = 0, MENU_DIR_DOWN = 1, MENU_DIR_LEFT = 2, MENU_DIR_RIGHT = 3 };

enum { MENU_BACKDROP_TOP_AS_IS = 0,   /* pause: the ROM already dims */
       MENU_BACKDROP_DIM_AUTO  = 1,   /* DS arithmetic evy 8 to black unless the ROM already dims */
       MENU_BACKDROP_TOP_FULL  = 2 }; /* title: the top screen is the artwork */

enum { MENU_STAGE_PRE_FADE = 0, MENU_STAGE_POST_FADE = 1 };

struct MenuRecipe;

struct MenuLayout { int w, h, unum, uden, canvas_w; /* unit = h/192 reduced; canvas_w = w*192/h */
                    MenuPiece p[MENU_MAX_PIECES]; int np; MenuHotspot hs[MENU_MAX_HOTSPOTS]; int nh;
                    int dest[MENU_MAX_PIECES][4]; int focus; uint8_t lift_owners; uint8_t stage; uint8_t backdrop;
                    /* P0CORE additions (never removed, only appended) */
                    int8_t back;                  /* hotspot the B / Backspace key presses, -1 none */
                    int variant;                  /* what build() was asked for */
                    const MenuRecipe *recipe; };

enum { MENU_DRV_NATIVE = 0, MENU_DRV_ROM_CURSOR = 1, MENU_DRV_TOUCH = 2, MENU_DRV_MIXED = 3 };
enum { MENU_KEY_NONE = 0, MENU_KEY_SAVE_MENU_ON_TOP = 1, MENU_KEY_ONE_SCREEN_MENUS = 2 };
enum { MENU_RF_FALLBACK = 1 };

struct MenuRecipe {
  const char *name;
  int  (*up)(int scene_id);                 /* 0 = not up; else a stable variant id (hysteresis inside) */
  void (*build)(int variant, MenuLayout *); /* pieces + hotspots for this variant and L->canvas_w */
  int  (*rom_cursor)(int dir, int *sel);    /* optional ROM-cursor driver; returns 1 if it moved */
  uint8_t enable_key;                       /* MENU_KEY_* : SaveMenuOnTop or OneScreenMenus */
  uint8_t driver;                           /* MENU_DRV_* */
  /* P0CORE additions */
  uint8_t flags;                            /* MENU_RF_* */
  uint8_t nvariants;                        /* variant ids 1..nvariants build something; 0 = stub (selftest skips) */
};

/* the ONE static table (menu_recipes.cpp) */
extern const MenuRecipe g_menu_recipes[];
extern const int        g_menu_nrecipes;
#define MENU_SCENE_TEST (-77)               /* the TEST recipe is up for this scene id only */

/* ---- the planes the real buffers arrive in (phase 1 fills these) ---------
   All pointers are borrowed for the frame. A null plane means "that source is
   not available this frame": its pieces draw nothing and say so in the trace. */
struct MenuTopCell { uint32_t argb; uint8_t bm; uint8_t owner; uint8_t pad[2]; };

struct MenuSources {
  const uint32_t *sub;            /* SRC_SUB: 256x192 ARGB engine-B scan-out (already engine-B bright) */
  uint32_t sub_backdrop;          /* the engine-B backdrop colour, RGB compared, for key_backdrop */
  const MenuTopCell *top2d;       /* SRC_TOP_2D: 256x192 cells, row-major; owner is a layer INDEX 0..4 */
  const uint32_t *pic;            /* SRC_TOP_PIC: host framebuffer, pic_stride pixels per row */
  int pic_stride, pic_w, pic_h;
  int pic_rx, pic_ry, pic_rw, pic_rh;  /* where the 256x192 top screen sits inside pic (host px) */
  const uint32_t *hostrow;        /* SRC_TOP_HOSTROW: host plane, same stride as the destination; s is in HOST pixels */
  int hostrow_stride, hostrow_w, hostrow_h;
  uint32_t (*bm_apply)(uint32_t argb, uint8_t bm);  /* engine-A fade/brightness verdict, null = identity */
  int rom_dimming;                /* the ROM is already dimming the game (BACKDROP_DIM_AUTO then draws nothing) */
};

/* ---- the core (C++), also what the selftest drives ---------------------- */
void menu_layout_begin(MenuLayout *L, int w, int h);                 /* zero it, unit + canvas_w */
void menu_layout_dest(MenuLayout *L);                                /* piece dest rects from anchors */
void menu_layout_solve(MenuLayout *L, const MenuRecipe *r, int variant, int w, int h);
int  menu_scale_allowed(int knum, int kden);
int  menu_add_piece(MenuLayout *L, const MenuPiece &p);              /* returns the index or -1 */
int  menu_add_hotspot(MenuLayout *L, int piece, int bx, int by, int bw, int bh, int rom_index);
MenuPiece menu_piece_make(MenuSrc src, int sx, int sy, int sw, int sh, int ax, int ay, int dx, int dy,
                          int knum, int kden);
int  menu_hit(const MenuLayout *L, int bx, int by, int *piece, int *dsx, int *dsy);   /* topmost touch piece, 1 = inside */
void menu_inverse(const MenuLayout *L, int bx, int by, int *drag_piece, int *dsx, int *dsy, int *in);
int  menu_hotspot_rect(const MenuLayout *L, int hs, int r[4]);      /* host rect, 0 if empty */
void menu_hotspot_centre(const MenuLayout *L, int hs, int *hx, int *hy);
int  menu_focus_move(MenuLayout *L, int dir);                        /* returns the new focus */
int  menu_focus_hover(MenuLayout *L, int bx, int by);                /* 1 if a hotspot is under the point */
void menu_piece_draw(const MenuLayout *L, int i, const MenuSources *src, uint32_t *dst, int w, int h);
void menu_highlight_paint(const MenuLayout *L, uint32_t *dst, int w, int h, unsigned frame);
int  menu_seam_census(const MenuLayout *L, int piece, const MenuSources *src, int counts[4]);
int  menu_autocrop_rect(const MenuPiece *p, const MenuSources *src, DsRect *out);
void menu_dim(uint32_t *dst, int w, int h, int evy);
int  menu_wide(const MenuLayout *L);                                 /* canvas_w >= MENU_WIDE_CANVAS */
int  menu_selftest(void);                                            /* 0 = all PASS */

#ifdef __cplusplus
extern "C" {
#endif
/* ---- what phase 1 calls ---------------------------------------------------*/
/* Once per DS frame at H1, before anything is lifted. Picks the recipe, builds the layout for (w, h). */
void hal_menu_frame_begin(int w, int h, int scene_id);
int  hal_menu_live(void);                       /* a recipe (or the fallback) owns this frame's picture */
void hal_menu_set_sources(const MenuSources *s);
/* H2. Draws the pieces whose layout stage matches `stage`; remembers the drawn layout for the inverse. */
void hal_menu_present(uint32_t *dst, int w, int h, int stage);
/* H3. Last frame's DRAWN layout. in = 1 only inside a touch piece. */
void hal_menu_touch_inverse(int bx, int by, int *dsx, int *dsy, int *in);
int  hal_menu_hover(int bx, int by);            /* mouse hover sets focus and shows the cursor; 1 if over a hotspot */
void hal_menu_touch_end(void);                  /* the stylus came up: forget the drag piece */
/* H4. `pressed` is the PadData pressed word; the recipe's driver decides what is consumed. */
void hal_menu_pad(uint16_t *pressed);
/* settings seam: key -> enabled. Default (no function set) = everything off. */
void hal_menu_set_enable_fn(int (*fn)(int key));
void hal_menu_set_netplay(int on);              /* in a transport session ROM_CURSOR is refused; TOUCH is used */
/* the synthesised-touch queue: single producer (hal_menu_pad), single consumer (poll_touch) */
int  hal_menu_touch_queue_press(int dsx, int dsy);  /* 1 queued; 0 refused (in flight or a real stylus is held) */
int  hal_menu_touch_queue_poll(int *dsx, int *dsy, int *down); /* once per poll; 1 = drive the stylus: down 1 held, 0 the release edge */
void hal_menu_touch_real_held(int held);        /* poll_touch tells the queue the real stylus state each poll */
int  hal_menu_fallback_frames(void);
#ifdef __cplusplus
}
#endif

/* pad bits as hal_lc_menu_pad uses them (hal/sub_screen.cpp): DS key word, PadData pressed */
#define MENU_PAD_CONFIRM 0x0002  /* the port's jump binding (pad A, Space, Enter), cited at hal_lc_menu_pad */
#define MENU_PAD_BACK    0x0001  /* ASSUMPTION for phase 1 to verify: the other face button */
#define MENU_PAD_START   0x0008
#define MENU_PAD_RIGHT   0x0010
#define MENU_PAD_LEFT    0x0020
#define MENU_PAD_UP      0x0040
#define MENU_PAD_DOWN    0x0080

#endif
