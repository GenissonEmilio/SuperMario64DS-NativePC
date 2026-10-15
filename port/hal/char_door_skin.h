/* hal/char_door_skin.h: how the white door's character picker looks (run hunt8, lane CHARMENU1). Pure host drawing, no ROM state except the
   four head icons, which are read out of the sprite memory the HUD already filled (cdskin_begin).

   TWO SKINS, one layout. With the "HD UI Textures" mod on (it carries the launcher's wooden art under hd_ui/menu/settings/) the picker is a
   parchment sheet with a wooden title plank, wooden row planks and the DS lettering, the settings page's family. Without it the same layout is
   drawn in plain colours with the debug overlay's small pixel font, the "no mod" skin the other one-screen menus get. The layout is a function
   of the picture size only (one unit is a 384th of the picture height), so a 4:3, 16:9 and 21:9 picture show the same panel, centred.

   Every piece is placed on its own: the sheet, the title plank, each row, each icon, the preview frame and the Back plank are separate draws
   (no rectangle cut from a finished screen). */
#ifndef PORT_HAL_CHAR_DOOR_SKIN_H
#define PORT_HAL_CHAR_DOOR_SKIN_H

#include <stdint.h>

enum { CDS_BOX = 1, CDS_PICK = 2 };

struct CdItem {
    int id;                 /* 0..3 plain, 4 + n custom n */
    int base;               /* 0..3, the character it plays as */
    int custom;
    int current;            /* the character being played */
    char label[96];         /* the name */
    char plays[40];         /* "Plays as Luigi" for a mod character, "" for a ROM one */
    char credits[200];      /* the mod's credits line, "" for a ROM one */
    const char *preview;    /* png file or null */
    const char *icon;       /* png file or null */
};

/* picture pixels; everything the input needs to hit-test and the draw needs to place */
struct CdGeom {
    float u;                        /* picture pixels per layout unit */
    int sx, sy, sw, sh;             /* the sheet */
    int tx, ty, tw, th;             /* the title plank */
    int bx, by, bw, bh;             /* the Back plank */
    int lx, ly, lw, rowh, rows;     /* the list: first row's x, y, width, row height; visible rows */
    int px, py, pw, ph;             /* the preview frame */
    int dx, dy, dw;                 /* the detail column: x, top y, width */
    int ox, oy, ow, oh;             /* the box's OK plank */
};

/* picture w x h, n entries, mode CDS_BOX or CDS_PICK */
void cdskin_geom(int w, int h, int n, int mode, CdGeom *g);
/* once per open: reads the head icons from the sprite memory, forgets the pictures of an earlier open */
void cdskin_begin(void);
/* 1 when the HD UI skin is the one drawn (the mod is on and its art loaded) */
int cdskin_hd(void);
/* draw onto an opaque picture (0x00RRGGBB / 0xFFRRGGBB), `stride` pixels per row */
void cdskin_draw(uint32_t *px, int stride, int w, int h, const CdGeom *g, const CdItem *it, int n, int sel, int top, int mode,
                 const char *box_msg, int back_hot, int ok_hot);


/* PRECOURSE1: the character strip on the star select (hal/char_strip.cpp). A row of slots at the bottom of the upper picture, between the lives
   counter and the star counter: one slot per selectable character (its head icon on a wooden plank, the current one on the cream plaque with a
   star), and a last slot with an arrow when there are more characters than fit. The layout is a function of the picture height (one unit is a
   192nd, the DS's own row), so the strip is the same size on a 4:3, a 16:9 and a 21:9 picture and sits centred. Slot id -1 in a CdItem is the
   arrow. Every slot, the badge, the arrow and the name plaque are separate draws. */
struct CsGeom {
    float k;                        /* picture pixels per DS pixel (h / 192) */
    int x0, y0, sw, sh, pitch, n;   /* slot 0's left, the slots' top, a slot's size, the distance between slots, the number of slots */
};
void cdskin_strip_geom(int w, int h, int n, CsGeom *g);
/* focus / hover: slot index or -1; the name plaque goes over the focus slot, else the hover slot. */
void cdskin_strip_draw(uint32_t *px, int stride, int w, int h, const CsGeom *g, const CdItem *it, int n, int focus, int hover);

#endif
