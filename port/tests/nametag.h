/* ---- THE VS NAME TAGS: who that body is, and how they are doing ------------
 *
 * WHAT IT DRAWS. In a VS match, over every REMOTE player's head, two centred
 * lines: his carried-star count on top and the lobby nickname the room gave
 * that seat under it. Never over the local player -- his own body is the one
 * body on screen nobody needs told apart.
 *
 * WHY IT IS HERE AND NOT IN THE GAME. The cartridge's VS mode was four people
 * in one room who could look up from their own screen; the port's is strangers
 * on a wire, and the ROM has nothing to say about which body is which. So this
 * is a HOST OVERLAY, drawn where every other one in this file is drawn -- into
 * the composed framebuffer after the raster owns the frame -- and it READS game
 * state and WRITES none. That is the property the lockstep proofs depend on: an
 * overlay that touched an actor, a pad word or the heap would show up as a
 * divergence, and this one cannot, because every access below is a load.
 *
 * ---- THE GLYPHS ARE THE CARTRIDGE'S ---------------------------------------
 *
 * NOT overlay_font.h. That 5x7 ASCII font is the debug overlay's, and a tag
 * over a player's head in the shipped game reading in a different typeface from
 * every other word the game says would be the port's voice where the game's
 * belongs. So the tag is drawn with the ROM's own message font, and the whole
 * decode is read off matched src rather than guessed:
 *
 *   WHERE THE TILES ARE.  LoadFont (src/LoadFont.cpp) and LoadFont3D
 *     (src/LoadFont3D.c) both MultiCopy_Int file 0x980e -- 0x4000 bytes -- to
 *     func_02054d88(), which is engine A's BG character base in VRAM. VS mode
 *     takes the LoadFont3D arm (Stage::LoadGraphics2D(b=1),
 *     src/_ZN5Stage14LoadGraphics2DEbi.cpp:75). The port maps DS VRAM at its
 *     real address (ntr/mmio.h VRAM_BASE 0x06000000), so the font the game is
 *     itself reading is readable here, at the same address, with no second
 *     copy and no file load of our own.
 *
 *   WHERE A GLYPH IS INSIDE THEM.  Message::AddChar
 *     (src/_ZN7Message7AddCharEc.cpp) is the authority, and it is exact:
 *
 *         ip = font_base + (((c & 0x1f) + ((c & 0xe0) << 1)) << 5)
 *         for (i = 0; i < 0x10; i++) { word = *ip++; if (i == 7) ip += 0xf8; }
 *
 *     Sixteen words is 64 bytes is two 4bpp 8x8 tiles, and the ip += 0xf8
 *     after the eighth word steps 992 bytes on top of the 32 already walked --
 *     1024 bytes, exactly 32 tiles. So the sheet is 32 TILES WIDE and a glyph
 *     is 8 wide by 16 tall: the top tile at the computed offset and the bottom
 *     tile one tile-row (0x400 bytes) below it. The (c & 0xe0) << 1 term is
 *     that same 32-wide row pitch written for the character code.
 *
 *   HOW WIDE A GLYPH IS.  data_0208f074[c], the ROM's per-code advance table
 *     (the font is proportional). AddChar reads it, func_0201b6f8 and
 *     func_02031028 measure lines with it, and the port already emits it as
 *     real ROM bytes (port/tools/romdata.py's NAMED list). Nothing here
 *     invents a width.
 *
 *   WHAT A CHARACTER CODE IS.  The message stream is not ASCII. The ranges are
 *     hal/message_boot.cpp's, derived there off AddChar for the message dumper
 *     and reused verbatim: digits 0x00..0x09, A..Z 0x0a..0x23, a..z
 *     0x2d..0x46, space 0x4d. THE FONT HAS LOWERCASE, so a nickname renders as
 *     the player typed it rather than shouted.
 *
 *   WHAT IS MISSING, SAID OUT LOUD.  Those four ranges are the only ones any
 *     matched TU in this tree names. SM64DS_VS_NAMES allows 0x20..0x7E, so a
 *     nickname with punctuation in it has characters this decode cannot place,
 *     and they are drawn as SPACES rather than as a guessed glyph. Guessing
 *     would put an arbitrary symbol in a stranger's name. The day somebody
 *     maps 0x24..0x2c and 0x47..0x4c off the font sheet itself, nt_code_for is
 *     the one function that changes.
 *
 *   THE COLOURS ARE NOT THE ROM'S, and that is the one deliberate deviation.
 *     A 4bpp glyph's non-zero indices are the font palette's, which for a tag
 *     floating over the open world would be unreadable against half the sky in
 *     the game. So every inked pixel is drawn in the tag's own colour with a
 *     black outline behind it, which is what makes it legible over water, sky
 *     and stone alike. The SHAPES are the cartridge's to the pixel; the ink is
 *     the port's, exactly as the winner banner's is.
 *
 * ---- WHERE ON THE SCREEN --------------------------------------------------
 *
 * There is no world-to-screen helper in this port, so this file has one, and
 * it is the same arithmetic the renderer does rather than a second opinion:
 *
 *   view    hal/particle_bridges.cpp already writes it out for the fx probe,
 *           and it is Model::Render's compose -- v' = p * R + T over
 *           data_0209b3ec, the Matrix4x3 View::Render parks through
 *           CopyToViewMat (src/_Z13CopyToViewMatPK9Matrix4x3.cpp). Its
 *           translation row is in SCENE units because Camera::Render feeds
 *           LookAt_ its eye and look-at as (v + 4) >> 3, so the actor's Fix12
 *           WORLD position crosses that same divide first.
 *   proj    ntr::gx_debug_proj, and ntr/gx.cpp's project() is the convention:
 *           mul(mul(v, pos), proj) with row vectors.
 *   screen  ntr/gx.cpp's to_screen, copied term for term, viewport included.
 *
 * BOTH MATRICES ARE STASHED, not read at draw time, and that is not a
 * micro-optimisation: by the time the overlay runs, the frame has drawn its 2D
 * layers and whatever projection they left behind is the live one.
 * nt_stash_view is called from the frame loop at the point where the camera has
 * been pushed and nothing else has touched the matrices yet, which is true on
 * both the real Camera actor's path and the old hand-tuned rig's (there
 * data_0209b3ec is the identity walk_window seeds it with and the view is
 * folded into the projection push_camera makes, so one stash covers both by
 * construction).
 *
 * Behind the camera and off the screen are both culled. The tag steps up one
 * size when the player is close, which is the whole of the distance treatment:
 * a continuous scale shimmers as it rounds to whole pixels. The head anchor
 * over the actor's feet is measured off the game's own camera rather than
 * chosen; NT_HEAD_UP carries the measurement.
 *
 * ---- WHICH SLOTS -----------------------------------------------------------
 *
 * data_0209f2d8 == 1 (a VS match), i != data_0209f250 (not me),
 * data_0209f394[i] non-null (a Player exists) and data_0209fc5c[i] non-zero.
 * The last one is Player::Render's OWN guard (src/_ZN6Player6RenderEv.cpp:46
 * returns early on it), so a tag exists exactly when the body under it does.
 *
 * The name and the star count come out of hal/star_flow.cpp through
 * port_vs_slot_name and port_vs_slot_stars rather than being read here, so the
 * tag and the end-of-match banner cannot disagree about either -- including
 * about the lobby's known slot-order caveat, which that file states.
 *
 * ---- THE CHAT BUBBLE (run mp1, lane BUBBLE1) -------------------------------
 *
 * Over the two tag lines, what that player said last (5 s, fading over the
 * last one) or "." ".." "..." in his colour while his chat box is open. The
 * state is hal/adv_chat.cpp's (port::chat_bubble, per chat seat); this draws
 * it with the same projection, the same cull and the same ROM font -- but
 * through adv_chat's chat_font_code, the one table that decides which
 * characters a chat line can hold, so a '.' or a ',' in a line is the font's
 * own glyph and not the tag decode's space. Body -> seat: in VS the body's
 * slot IS its chat seat; an adventure ghost's seat is port_adventure_ghost_seat.
 * Your own bubble is drawn over your own body too (Tango's call), at the same
 * height with no tag under it. The Name Tags setting switches the two tag lines only: bubbles
 * still draw with it off. Bodies are drawn far to near so the nearest bubble
 * sits on top; a far body (past the tag's close step) gets a one-row bubble.
 */
#ifndef PORT_TESTS_NAMETAG_H
#define PORT_TESTS_NAMETAG_H

#include <math.h>   /* the ghost tag's distance fade (nt_draw) */

#include "hal/adv_chat.h"   /* port::chat_bubble, chat_font_code (BUBBLE1) */

extern "C" {
/* the settings key and its env override (hal/host_settings.cpp) */
int host_setting_name_tags(void);
/* the two facts, from hal/star_flow.cpp */
const char *port_vs_slot_name(int slot);
int port_vs_slot_stars(int slot);
/* engine A's BG character base: where LoadFont/LoadFont3D put file 0x980e */
int func_02054d88(void);
/* the ROM's per-character advance table (romdata.py NAMED) */
extern unsigned char data_0208f074[];
/* per-slot "this slot is live", Player::Render's own guard */
extern unsigned char data_0209fc5c[];
/* ADVENTURE co-op (M2): is this remote slot a ghost to draw now -- present and
   in this console's level (hal/comms_sync.cpp). The tag loop skips a peer in
   another level so no name floats over a body that is not drawn. */
int port_adventure_peer_visible(int slot);
/* run mp1 lane GHOSTWIRE1: a hub ghost's own facts, off its SYNI / SYNG
   (hal/comms_sync.cpp). seat -1 means "not a hub ghost": use the VS facts. */
int port_adventure_ghost_seat(int slot);
const char *port_adventure_ghost_name(int slot);
int port_adventure_ghost_stars(int slot);
unsigned port_adventure_ghost_rgb(int slot);
int port_adventure_ghost_fade(int slot);
int port_adventure_ghost_drawpos(int slot, int out[3]);
/* the VS round's seat mask, the same on every console at the same frame
   (hal/comms_loopback.cpp; 0 = no VS carrier session, no information) */
unsigned port_vs_seat_live_mask(void);
}

/* ADVENTURE GHOSTS: the same head tags label the ghosts in an adventure
   session, which is not a VS match (data_0209f2d8 stays 0). Forward-declared
   the way the settings key above is, so this header keeps its light include
   surface. */
namespace port { bool adventure_ghost_mode(); }

/* ---- the glyph atlas ------------------------------------------------------
   Decoded ONCE, out of the font the game itself is reading, and kept. One
   entry per printable ASCII code; have is 0 for a code the message encoding
   has no place for (see nt_code_for). */
enum { NT_GW = 8, NT_GH = 16, NT_FIRST = 0x20, NT_LAST = 0x7e };

struct NtGlyph {
    unsigned char ink[NT_GH][NT_GW];   /* the 4bpp index, 0 = transparent */
    unsigned char adv;                 /* data_0208f074's advance, in pixels */
    unsigned char have;
};

static NtGlyph g_nt_glyph[NT_LAST - NT_FIRST + 1];
/* the bubble's atlas: the same sheet, decoded through port::chat_font_code */
static NtGlyph g_nt_bglyph[NT_LAST - NT_FIRST + 1];
static int g_nt_font_ready;
static int g_nt_font_said;

/* ASCII to the message stream's character code, hal/message_boot.cpp's ranges.
   -1 for a code this encoding has no glyph for; the caller draws a space. */
static int nt_code_for(unsigned char a)
{
    if (a >= '0' && a <= '9') return a - '0';            /* 0x00..0x09 */
    if (a >= 'A' && a <= 'Z') return 0x0a + (a - 'A');   /* 0x0a..0x23 */
    if (a >= 'a' && a <= 'z') return 0x2d + (a - 'a');   /* 0x2d..0x46 */
    if (a == ' ') return 0x4d;
    return -1;
}

/* Build the atlas if the font is up. Returns 1 once it is built. Cheap to call
   every frame: one compare after the first success, and one VRAM probe before
   it (the font is not in VRAM until the scene's LoadGraphics2D has run). */
static int nt_font_build(void)
{
    if (g_nt_font_ready) return 1;

    const unsigned base = (unsigned)func_02054d88();
    if (!base) return 0;                 /* no BG char base in this mode */

    /* THE PROBE IS THE LETTER A, not the base pointer. func_02054d88 answers an
       address whenever the display is in a mode that has one, whether or not
       anybody has loaded a font into it, so a build that trusted the pointer
       would cache 96 blank glyphs on the first frame and never look again. */
    {
        const unsigned off =
            (unsigned)(((0x0a & 0x1f) + ((0x0a & 0xe0) << 1)) << 5);
        const unsigned char *t = (const unsigned char *)(base + off);
        int any = 0;
        for (int i = 0; i < 32 && !any; ++i) any = t[i] != 0;
        if (!any) return 0;
    }

    for (int a = NT_FIRST; a <= NT_LAST; ++a)
    for (int atlas = 0; atlas < 2; ++atlas) {
        NtGlyph &g = (atlas ? g_nt_bglyph : g_nt_glyph)[a - NT_FIRST];
        g.have = 0;
        g.adv = 6;
        const int c = atlas ? port::chat_font_code((unsigned char)a)
                            : nt_code_for((unsigned char)a);
        if (c < 0) continue;

        const unsigned off =
            (unsigned)(((c & 0x1f) + ((c & 0xe0) << 1)) << 5);
        const unsigned char *top = (const unsigned char *)(base + off);
        const unsigned char *bot = top + 0x400;   /* one 32-tile row down */
        for (int r = 0; r < NT_GH; ++r) {
            const unsigned char *t = (r < 8 ? top : bot) + (r & 7) * 4;
            for (int x = 0; x < NT_GW; ++x) {
                const unsigned char b = t[x >> 1];
                g.ink[r][x] = (unsigned char)((x & 1) ? (b >> 4) : (b & 0x0f));
            }
        }
        {
            const unsigned w = data_0208f074[c];
            g.adv = (unsigned char)((w >= 1 && w <= 16) ? w : 6);
        }
        g.have = 1;
    }
    g_nt_font_ready = 1;
    if (!g_nt_font_said) {
        g_nt_font_said = 1;
        fprintf(stderr, "[tag] the ROM's message font is decoded from VRAM "
                "%08x: 8x16 glyphs, 32-tile rows, widths from data_0208f074\n",
                base);
    }
    return 1;
}

/* ---- one line of ROM-font text --------------------------------------------
   pass 0 paints the black outline (every inked pixel's eight neighbours),
   pass 1 paints the ink. The caller runs the whole line through pass 0 before
   pass 1 so a glyph's outline cannot land on its neighbour's ink. measure
   returns the width and paints nothing. */
/* Blend src over dst by alpha 0..255. At 255 it is a plain store, so the VS
   callers (which pass 255) paint exactly the opaque pixels they always did. */
static inline uint32_t nt_blend(uint32_t dst, uint32_t src, int a)
{
    if (a >= 255) return src;
    if (a <= 0) return dst;
    const int ia = 255 - a;
    const uint32_t sr = (src >> 16) & 0xFF, sg = (src >> 8) & 0xFF, sb = src & 0xFF;
    const uint32_t dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    const uint32_t r = (sr * a + dr * ia) / 255;
    const uint32_t g = (sg * a + dg * ia) / 255;
    const uint32_t b = (sb * a + db * ia) / 255;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static int nt_line(const OvlSurface &fb, int x0, int y0, const char *s,
                   int scale, uint32_t rgb, int pass, int measure, int alpha = 255,
                   const NtGlyph *atlas = g_nt_glyph)
{
    int x = x0;
    for (; *s; ++s) {
        const unsigned char a = (unsigned char)*s;
        if (a < NT_FIRST || a > NT_LAST) continue;
        const NtGlyph &g = atlas[a - NT_FIRST];
        const int adv = (int)g.adv * scale;
        if (measure || !g.have) { x += adv; continue; }
        for (int r = 0; r < NT_GH; ++r)
            for (int c = 0; c < NT_GW; ++c) {
                if (!g.ink[r][c]) continue;
                for (int sy = 0; sy < scale; ++sy)
                    for (int sx = 0; sx < scale; ++sx) {
                        const int fx = x + c * scale + sx;
                        const int fy = y0 + r * scale + sy;
                        for (int oy = -1; oy <= 1; ++oy)
                            for (int ox = -1; ox <= 1; ++ox) {
                                if (pass == 1 && (ox || oy)) continue;
                                if (pass == 0 && !ox && !oy) continue;
                                const int qx = fx + ox * scale;
                                const int qy = fy + oy * scale;
                                if (qx < 0 || qx >= ntr::SCREEN_W ||
                                    qy < 0 || qy >= ntr::SCREEN_H)
                                    continue;
                                const size_t idx =
                                    (size_t)qy * (size_t)fb.stride + qx;
                                fb.px[idx] = nt_blend(fb.px[idx],
                                    pass ? rgb : 0xFF000000u, alpha);
                            }
                    }
            }
        x += adv;
    }
    return x - x0;
}

static void nt_text_centred(const OvlSurface &fb, int cx, int y, const char *s,
                            int scale, uint32_t rgb, int alpha = 255)
{
    const int w = nt_line(fb, 0, 0, s, scale, rgb, 1, 1);
    const int x = cx - w / 2;
    nt_line(fb, x, y, s, scale, rgb, 0, 0, alpha);
    nt_line(fb, x, y, s, scale, rgb, 1, 0, alpha);
}

/* ---- the stashed frame transform ------------------------------------------
   Taken where the camera has just been pushed. See the banner. */
static float g_nt_proj[16];
static int g_nt_view[12];
static int g_nt_vp[4] = {0, 0, ntr::SCREEN_W, ntr::SCREEN_H};
static int g_nt_have_view;

static void nt_stash_view(void)
{
    ntr::gx_debug_proj(g_nt_proj);
    for (int i = 0; i < 12; ++i) g_nt_view[i] = data_0209b3ec[i];
    int vx = 0, vy = 0, vw = 0, vh = 0, sets = 0;
    ntr::gx_debug_viewport(vx, vy, vw, vh, sets);
    if (sets && vw > 0 && vh > 0) {
        g_nt_vp[0] = vx; g_nt_vp[1] = vy; g_nt_vp[2] = vw; g_nt_vp[3] = vh;
    } else {
        g_nt_vp[0] = 0; g_nt_vp[1] = 0;
        g_nt_vp[2] = ntr::SCREEN_W; g_nt_vp[3] = ntr::SCREEN_H;
    }
    g_nt_have_view = 1;
}

/* Fix12 WORLD position -> framebuffer pixel. 0 when it is behind the camera or
   the frame has no stashed transform yet. depth comes back as the clip w, in
   scene units, which is what the size step reads. */
static int nt_project(const int wp[3], float *sx, float *sy, float *depth)
{
    if (!g_nt_have_view) return 0;
    const int *m = g_nt_view;
    /* the ROM's own world -> scene conversion (Camera::Render's (v + 4) >> 3),
       then out of Fix12 */
    const double px = (double)((wp[0] + 4) >> 3) / 4096.0;
    const double py = (double)((wp[1] + 4) >> 3) / 4096.0;
    const double pz = (double)((wp[2] + 4) >> 3) / 4096.0;

    const double vx = (px * m[0] + py * m[3] + pz * m[6]) / 4096.0
                      + m[9] / 4096.0;
    const double vy = (px * m[1] + py * m[4] + pz * m[7]) / 4096.0
                      + m[10] / 4096.0;
    const double vz = (px * m[2] + py * m[5] + pz * m[8]) / 4096.0
                      + m[11] / 4096.0;

    const float *P = g_nt_proj;
    const double cx = vx * P[0] + vy * P[4] + vz * P[8] + P[12];
    const double cy = vx * P[1] + vy * P[5] + vz * P[9] + P[13];
    const double cw = vx * P[3] + vy * P[7] + vz * P[11] + P[15];
    /* THE NEAR CULL, AND WHY IT IS NOT 1e-4. A body far off-axis but within a
       hair of the eye plane divides a large cx by a near-zero cw, and the
       quotient is what a caller eventually rounds into an int -- so an epsilon
       chosen only to avoid dividing by zero leaves a number no int can hold.
       1e-2 is still two orders under the smallest near plane anything in this
       port sets (push_camera's is 3/8 in these units, and the ROM camera's
       preset is larger), so nothing that could legitimately be drawn is lost,
       and it is the same order as ntr/gx.cpp's own clip epsilon. */
    if (cw <= 1e-2) return 0;                 /* behind the camera, or on it */

    const double fx = (cx / cw + 1.0) * 0.5 * g_nt_vp[2] + g_nt_vp[0];
    const double fy = (1.0 - (cy / cw + 1.0) * 0.5) * g_nt_vp[3] + g_nt_vp[1];
    /* AND THE RANGE IS CHECKED HERE, IN DOUBLES, so no caller can be handed a
       coordinate that will not survive a cast. The comparisons are written the
       positive way round on purpose: !(a && b) rejects a NaN, which an
       out-of-range test written as (a || b) would let through. The bound is
       generous -- a hundred screens either side -- because this is the "is this
       a number at all" guard, not the visibility test; nt_draw does that. */
    const double kSane = 100000.0;
    if (!(fx > -kSane && fx < kSane && fy > -kSane && fy < kSane))
        return 0;

    *sx = (float)fx;
    *sy = (float)fy;
    *depth = (float)cw;
    return 1;
}

/* THE HEAD, MEASURED RATHER THAN GUESSED, and it is the only chosen number in
   this file.

   The actor's position is at his feet. What sits above them is not written
   down anywhere in src/, so it was read off the GAME'S OWN CAMERA in a live
   four-window arena (SM64DS_TAG_PROBE=2, whose ruler line prints exactly this
   measurement): the player's feet at world y 254.0, and Camera::Render's
   look-at (cam+0x80) at world y 415.2. The ROM films the player from 161 world
   units above his feet, which is that camera's idea of his head.

   So the anchor is 200 -- a little clear of the ROM's own head height, and the
   name's bottom row lands just above it. The first pass used 320 and the same
   ruler is what caught it: at the distance a four-window arena films from, 320
   lifted the tag 164 px on a 384-row screen, a whole second body above him. */
enum { NT_HEAD_UP = 200 * 4096 };

/* SM64DS_TAG_PROBE=1 prints one line per tag on the frames it draws, which is
   what turns "no tag appeared" into "the slot was skipped" or "it projected
   off screen". =2 adds the RULER: the same body's FEET projected beside its
   head, which is how NT_HEAD_UP above was sized instead of guessed -- the
   pixel gap between the two is what a chosen world-unit offset is actually
   worth on screen at the distance the game films from. Off unless asked. */
static int nt_probe(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("SM64DS_TAG_PROBE");
        on = e ? (e[0] == '2' ? 2 : 1) : 0;
    }
    return on;
}

/* ---- the bubble -----------------------------------------------------------
   One chat line wrapped into at most max_rows rows no wider than half the
   screen (a word too long for a row is cut inside it); text past the last row
   ends that row in "...". Returns the row count. */
enum { NT_BUBBLE_ROWS = 3 };

static int nt_badv(unsigned char a, int scale)
{
    if (a < NT_FIRST || a > NT_LAST) return 6 * scale;
    return (int)g_nt_bglyph[a - NT_FIRST].adv * scale;
}

static int nt_bwidth(const char *s, int scale)
{
    int w = 0;
    for (; *s; ++s) w += nt_badv((unsigned char)*s, scale);
    return w;
}

static int nt_bubble_wrap(const char *text, int scale, int maxw, int max_rows,
                          char rows[NT_BUBBLE_ROWS][port::kChatTextMax + 4])
{
    const int n = (int)strlen(text);
    int i = 0, r = 0;
    while (i < n && text[i] == ' ') ++i;
    while (i < n && r < max_rows) {
        /* the longest run from i that fits, preferring to end at a space */
        int w = 0, j = i, last_space = -1;
        while (j < n) {
            const int a = nt_badv((unsigned char)text[j], scale);
            if (w + a > maxw && j > i) break;
            if (text[j] == ' ') last_space = j;
            w += a;
            ++j;
        }
        int end = j;
        if (j < n && last_space > i) end = last_space;   /* break at the space */
        int len = end - i;
        while (len > 0 && text[i + len - 1] == ' ') --len;
        memcpy(rows[r], text + i, (size_t)len);
        rows[r][len] = 0;
        i = end;
        while (i < n && text[i] == ' ') ++i;
        ++r;
    }
    if (i < n && r > 0) {
        /* more than fits: the last row ends in "...", trimmed until it fits */
        char *row = rows[r - 1];
        int len = (int)strlen(row);
        while (len > 0 && nt_bwidth(row, scale) + nt_bwidth("...", scale) > maxw)
            row[--len] = 0;
        memcpy(row + len, "...", 4);
    }
    return r;
}

/* The rows at the top of every chat glyph cell that nothing draws into:
   above the highest inked row of any glyph in the bubble atlas, less the one
   row its black outline takes. Read off the decoded font once. */
static int nt_bubble_blank_top(void)
{
    static int blank = -1;
    if (blank >= 0) return blank;
    int top = NT_GH;
    for (int a = 0; a <= NT_LAST - NT_FIRST; ++a) {
        const NtGlyph &g = g_nt_bglyph[a];
        if (!g.have) continue;
        for (int r = 0; r < top; ++r) {
            int any = 0;
            for (int c = 0; c < NT_GW && !any; ++c) any = g.ink[r][c] != 0;
            if (any) { top = r; break; }
        }
    }
    blank = top >= NT_GH ? 0 : (top > 0 ? top - 1 : 0);
    return blank;
}

/* The bubble whose BOTTOM edge is at y, centred on cx. Dots in the sender's
   colour; a line in white. The shade is the chat log's own (ovl_shade), under
   the text while it is still mostly opaque. */
static void nt_bubble_draw(const OvlSurface &fb, int cx, int y,
                           const port::ChatBubble &b, int scale, int alpha,
                           int max_rows)
{
    const int lh = NT_GH * scale + 2 * scale;
    /* KEEP IT ON THE PICTURE. At RenderScale 1 (256x192) a close body's head
       sits so high that three rows above it would start off the top edge, so
       only the rows that fit above y are drawn (at least one; the wrap ends
       the last one in "..."), and y never sits closer to the top than one
       row. A row counts as fitting when its INK is on the picture: the glyph
       cells' blank top rows (above the highest outline pixel any chat glyph
       has) may hang over the edge, so a bubble whose top row just touches
       the edge keeps that row. */
    const int fit = (y + nt_bubble_blank_top() * scale) / lh;
    if (max_rows > fit) max_rows = fit < 1 ? 1 : fit;
    if (y < lh) y = lh;
    if (b.typing) {
        static const char *const kDots[3] = {".", "..", "..."};
        const int w3 = nt_bwidth("...", scale);
        const int x = cx - w3 / 2;
        const int top = y - lh;
        if (alpha > 96)
            ovl_shade(fb, x - 3 * scale, top - scale, w3 + 6 * scale, lh);
        const char *d = kDots[(b.dots - 1) % 3];
        nt_line(fb, x, top, d, scale, b.rgb, 0, 0, alpha, g_nt_bglyph);
        nt_line(fb, x, top, d, scale, b.rgb, 1, 0, alpha, g_nt_bglyph);
        return;
    }
    char rows[NT_BUBBLE_ROWS][port::kChatTextMax + 4];
    /* half the ACTIVE width: SCREEN_W is the largest tier's buffer, not the
       screen this frame is drawn at */
    const int nr = nt_bubble_wrap(b.text, scale, ntr::active_w / 2, max_rows, rows);
    if (nr <= 0) return;
    const int a = alpha * b.alpha / 255;
    if (a <= 0) return;
    int w = 0;
    for (int r = 0; r < nr; ++r) {
        const int rw = nt_bwidth(rows[r], scale);
        if (rw > w) w = rw;
    }
    const int top = y - nr * lh;
    if (a > 96)
        ovl_shade(fb, cx - w / 2 - 3 * scale, top - scale, w + 6 * scale, nr * lh);
    for (int pass = 0; pass <= 1; ++pass)
        for (int r = 0; r < nr; ++r) {
            const int rw = nt_bwidth(rows[r], scale);
            nt_line(fb, cx - rw / 2, top + r * lh, rows[r], scale, 0xFFFFFFFFu,
                    pass, 0, a, g_nt_bglyph);
        }
}

/* ---- the tags -------------------------------------------------------------
   Called once per frame, with the surface every other host overlay draws on.
   Reads only. */
struct NtBody {
    int i;               /* the body (player slot) */
    int seat;            /* its chat seat, -1 when it has none */
    float sx, sy, d;
    int bp[3];
};

static void nt_draw(const OvlSurface &fb)
{
    /* A VS match OR an adventure-ghost session -- both draw a tag over every
       remote body. Adventure is not VS (data_0209f2d8 stays 0), so the gate is
       the two modes, not the one flag. */
    const bool adventure = port::adventure_ghost_mode();
    if (data_0209f2d8 != 1 && !adventure) return;
    /* The Name Tags setting turns the two tag lines off; a chat bubble still
       draws with it off (lane BUBBLE1). */
    const bool tags_on = host_setting_name_tags() != 0;
    const int my_seat = port::chat_my_seat();
    if (!tags_on && my_seat < 0) return;
    if (!nt_font_build()) return;

    const int me = (int)data_0209f250;
    const unsigned vs_live = adventure ? 0u : port_vs_seat_live_mask();
    NtBody bodies[kPortMaxPlayers];
    int nb = 0;
    for (int i = 0; i < kPortMaxPlayers; ++i) {
        const char *p = (const char *)data_0209f394[i];
        if (!p) continue;
        if (data_0209fc5c[i] == 0) continue;     /* Player::Render's own guard */
        /* A VS PLAYER WHO LEFT (quit, refused at the door, retired): the round
           no longer carries his slot, from the same round on every console, so
           no tag and no bubble float over the body he left behind. A mask of
           0 is "no information yet" and skips nothing. */
        if (vs_live && !((vs_live >> i) & 1u)) continue;
        /* ADVENTURE co-op (M2): in a ghost session, only tag a peer present in
           this console's level -- a body in another level is despawned and
           draws nothing, so a name over it would float on empty space. VS is
           one shared level and does not consult the filter. */
        if (i != me && adventure && !port_adventure_peer_visible(i)) continue;

        /* the body's chat seat: its VS slot, a hub ghost's hub seat, or this
           console's own seat over its own body */
        int seat = -1;
        if (i == me) seat = my_seat;
        else if (adventure) seat = port_adventure_ghost_seat(i);
        else if (my_seat >= 0) seat = i;
        /* never a tag over the local body; only its own bubble, if any */
        port::ChatBubble own;
        if (i == me && !(seat >= 0 && port::chat_bubble(seat, &own))) continue;
        if (i != me && !tags_on && seat < 0) continue;

        /* WHERE THE BODY WAS DRAWN. A hub ghost is parked out of the level
           again right after its draw (hal/comms_sync.cpp), so its tag reads
           the position the follower drew it at this frame, and a hub ghost
           that was not drawn gets no tag. Every other body: its own. */
        NtBody &e = bodies[nb];
        e.bp[0] = *(const int *)(p + 0x5c);
        e.bp[1] = *(const int *)(p + 0x60);
        e.bp[2] = *(const int *)(p + 0x64);
        if (i != me && adventure && port_adventure_ghost_seat(i) >= 0 &&
            !port_adventure_ghost_drawpos(i, e.bp))
            continue;
        const int wp[3] = {e.bp[0], e.bp[1] + NT_HEAD_UP, e.bp[2]};
        if (!nt_project(wp, &e.sx, &e.sy, &e.d)) continue;
        e.i = i;
        e.seat = seat;
        ++nb;
    }
    /* FAR TO NEAR, so the nearest body's tag and bubble land on top (a plain
       insertion sort: at most sixteen) */
    for (int a = 1; a < nb; ++a)
        for (int b = a; b > 0 && bodies[b].d > bodies[b - 1].d; --b) {
            const NtBody t = bodies[b];
            bodies[b] = bodies[b - 1];
            bodies[b - 1] = t;
        }

    static unsigned s_said[kPortMaxPlayers];     /* the probe's last bubble line */
    for (int k = 0; k < nb; ++k) {
        const NtBody &e = bodies[k];
        const int i = e.i;
        const int *bp = e.bp;
        const float sx = e.sx, sy = e.sy, d = e.d;

        /* SIZE. The VS tag steps up one size when the player is close (a
           continuous scale shimmers as it rounds to whole pixels; the w<200
           threshold is scene units, an arena films its others at w 100..130).
           A ghost tag is a touch smaller and does not take the close step, so a
           crowd of ghosts stays legible without shouting. */
        const int scale = adventure ? OVL_SCALE : OVL_SCALE * (d < 200.0f ? 2 : 1);
        const int lh = NT_GH * scale + 2 * scale;

        /* PROXIMITY FADE, ghosts only, on the voice lane's radii (world units,
           VoiceNearRadius 512 full / VoiceFarRadius 3072 gone; C:/tmp/voice
           defaults, copied so the tag and the voice it labels share a range).
           Full opacity inside near, linear to zero at far, and NO tag at all
           beyond it -- a name you cannot talk to is not drawn. */
        int alpha = 255;
        if (adventure && i != me) {
            const char *mp = (const char *)data_0209f394[me];
            if (mp) {
                const double ddx = (double)(bp[0] - *(const int *)(mp + 0x5c));
                const double ddy = (double)(bp[1] - *(const int *)(mp + 0x60));
                const double ddz = (double)(bp[2] - *(const int *)(mp + 0x64));
                const double dist =
                    sqrt(ddx * ddx + ddy * ddy + ddz * ddz) / 4096.0;
                const double kNear = 512.0, kFar = 3072.0;
                const double aa = dist <= kNear ? 1.0
                                : dist >= kFar  ? 0.0
                                : (kFar - dist) / (kFar - kNear);
                alpha = (int)(aa * 255.0 + 0.5);
            }
            /* a hub ghost whose frames stopped fades with its body */
            alpha = alpha * port_adventure_ghost_fade(i) / 256;
            if (alpha <= 0) continue;   /* out of prox-chat range: no tag */
        }
        /* OFF THE SCREEN ENTIRELY, with a whole tag's margin so a name that is
           half on does not flicker at the edge -- and TESTED IN FLOATS, BEFORE
           THE CASTS, because the cast is the thing that has to be made safe.
           Rounding first and testing the int afterwards is the wrong order: by
           then the conversion has already happened, and a float outside int
           range converts to an unspecified value rather than to a big number
           the test would catch. Written positive so a NaN falls out here too. */
        const float star_yf = sy - 2.0f * (float)lh;
        if (!(sx >= -256.0f && sx <= (float)ntr::SCREEN_W + 256.0f))
            continue;
        if (!(star_yf >= -2.0f * (float)lh && star_yf <= (float)ntr::SCREEN_H))
            continue;

        /* TWO LINES, and the order flips by mode. VS keeps its shipped stack,
           stars on top and name under. A ghost wants the name first and the
           server star count beneath it, so name takes the top row and stars
           the bottom. Both rows sit above the head anchor either way. */
        const int cx = (int)(sx + 0.5f);
        const int y_top = (int)(sy + 0.5f) - 2 * lh;
        const int y_bot = (int)(sy + 0.5f) - lh;
        const bool tagged = tags_on && i != me;
        if (tagged) {
            const int name_y = adventure ? y_top : y_bot;
            const int star_y = adventure ? y_bot : y_top;

            /* A HUB GHOST (run mp1 lane GHOSTWIRE1) is labelled from its own
               wire: the name its SYNI carries (already cut down to printable
               ASCII), the save's star total its SYNG carries, and the name
               drawn in its body colour. Anything else keeps the VS facts. */
            const int gseat = adventure ? port_adventure_ghost_seat(i) : -1;
            char who[32];
            const char *nick = gseat >= 0 ? port_adventure_ghost_name(i)
                                          : port_vs_slot_name(i);
            if (nick && nick[0])
                snprintf(who, sizeof who, "%s", nick);
            else
                snprintf(who, sizeof who, "PLAYER %d",
                         (gseat >= 0 ? gseat : i) + 1);

            /* The star count is port_vs_slot_stars, the per-server VS score,
               and it labels a ghost the same as a VS body now: how many stars
               that player has on this server. A hub ghost carries its own. */
            int stars = port_vs_slot_stars(i);
            if (gseat >= 0) {
                const int gs = port_adventure_ghost_stars(i);
                stars = gs >= 0 ? gs : 0;
            }
            char sline[24];
            snprintf(sline, sizeof sline, "%d STAR%s", stars, stars == 1 ? "" : "S");
            unsigned name_rgb = 0xFFFFFFFFu;
            if (gseat >= 0) {
                const unsigned rgb = port_adventure_ghost_rgb(i);
                if (rgb) name_rgb = rgb;
            }

            nt_text_centred(fb, cx, star_y, sline, scale, 0xFFFFE060u, alpha);
            nt_text_centred(fb, cx, name_y, who, scale, name_rgb, alpha);

            if (nt_probe()) {
                fprintf(stderr, "[tag] slot %d \"%s\" %s at (%.0f,%.0f) w=%.2f "
                        "scale %d\n", i, who, sline, sx, sy, d, scale);
                if (nt_probe() >= 2) {
                    const int fp[3] = {bp[0], bp[1], bp[2]};
                    float fx = 0, fy = 0, fd = 0;
                    if (nt_project(fp, &fx, &fy, &fd))
                        fprintf(stderr, "[tag]   ruler: feet y=%.1f world at "
                                "screen y %.0f, so +%d world lifts the anchor "
                                "%.0f px\n", fp[1] / 4096.0, fy,
                                (int)(NT_HEAD_UP / 4096), fy - sy);
                }
            }
        }

        /* THE BUBBLE, over the tag lines' place whether or not they are drawn
           (your own body, or tags switched off): one height over every head,
           so a bubble never lands on the tag of a body standing where you
           stand. */
        port::ChatBubble bub;
        const int have = e.seat >= 0 && port::chat_bubble(e.seat, &bub);
        if (have) {
            const int bottom = y_top - 2 * scale;
            nt_bubble_draw(fb, cx, bottom, bub, scale, alpha,
                           d < 200.0f ? NT_BUBBLE_ROWS : 1);
        }
        if (nt_probe()) {
            /* one line when a body's bubble changes, not one per frame */
            unsigned sig = !have ? 0u : bub.typing ? 1u : 2166136261u;
            if (have && !bub.typing)
                for (const char *q = bub.text; *q; ++q)
                    sig = (sig ^ (unsigned char)*q) * 16777619u;
            if (sig != s_said[i]) {
                s_said[i] = sig;
                if (!have)
                    fprintf(stderr, "[tag] bubble body=%d seat=%d%s none\n", i,
                            e.seat, i == me ? " (me)" : "");
                else
                    fprintf(stderr, "[tag] bubble body=%d seat=%d%s %s at "
                            "(%.0f,%.0f) w=%.2f rows=%d \"%s\"\n", i, e.seat,
                            i == me ? " (me)" : "",
                            bub.typing ? "typing" : "text", sx, sy, d,
                            bub.typing ? 1 : (d < 200.0f ? NT_BUBBLE_ROWS : 1),
                            bub.typing ? "..." : bub.text);
            }
        }
    }
}

#endif /* PORT_TESTS_NAMETAG_H */
