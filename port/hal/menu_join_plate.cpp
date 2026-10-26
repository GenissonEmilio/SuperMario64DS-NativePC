/* menu_join_plate.cpp -- the "Joining" plate of a join successor (run after093, lane JOINFLOW1). See menu_join_plate.h.

   WHAT THE PLAYER USED TO SEE. Picking an online file asked the launcher for a server and then started the game again; the restarted game opened its
   own title, ran the main menu and the file select with the scripted presses showing, and only then entered the adventure. Nothing explained that.

   WHAT HAPPENS NOW. The marker SM64DS_JOIN_QUIET (set by online_join.cpp's apply_plan, read and cleared here at start-up) makes this process hold a plain
   plate over every picture it presents. The walk is NOT driven by hand and not skipped: the ROM's title, menu and file select are the machine that
   reaches the file (hal/title_entry.cpp port_title_skip_tick presses the file through the ROM's own handlers), it just is not drawn, and it is not heard
   (sd_host_mute), and the frame pacer is skipped on most frames while it runs so it takes a moment instead of ten seconds. title_entry.cpp's commit ends
   the plate when the ROM asks for the Stage; the first picture after it is the level's own.

   THE CANCEL. B (the pad's B, the attack key), Esc or a click on BACK. It clears every name a join set (online_join_clear_env), drops the boot file, and
   starts the game again at its front door as a clean solo title (the same hook the join itself used); this process then ends. A walk that has not reached
   the adventure after 90 s does the same, so the plate can never be a dead end.

   WHAT IT DOES NOT DO. It does not change what the launcher's Multiplayer window starts (party / coop / VS / world rooms): they never carry the marker. */
#include "menu_join_plate.h"
#include "online_join.h"
#include "ui_text.h"
#include "host_settings.h"
#include "mods.h"

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

extern "C" {
extern int  (*port_join_plate_up_hook)(void);            /* hal/sub_screen.cpp: the real mouse is not the stylus while the plate is up */
extern int  (*port_title_front_door_hook)(void);          /* hal/title_entry.cpp; tests/walk_window.cpp seats it */
extern int  (*port_join_quiet_tick_hook)(void);           /* hal/title_entry.cpp */
extern void (*port_join_quiet_end_hook)(const char *);    /* hal/title_entry.cpp */
extern int  port_join_quiet_cancelled;                    /* hal/title_entry.cpp */
void sd_host_mute(int on);                                /* hal/sdat/mixer.cpp */
int  host_setting_enabled_mods(char *bundle, size_t bcap, char *arr, size_t acap);
int  hal_menu_enabled(int key);                           /* hal/menu_compose.cpp */
}

extern "C" int port_join_plate_up(void);

namespace {

const ULONGLONG kGiveUpMs = 90000;
const int kFastKeep = 3;                 /* of every 4 frames the pacer is skipped while the walk is hidden */

struct Plate {
    int up;
    int server;
    ULONGLONG t0;
    unsigned frames;
    int said;
    int cancel;                          /* 0 none, 1 player, 2 the walk never finished */
    int armed;                           /* B / Esc have been seen released since the plate came up */
    int prev_esc, prev_b;
    int bx, by, bw, bh;                  /* the BACK plank in the last picture */
    int pw, ph;
    std::vector<uint32_t> px;
    int test_at;                         /* SM64DS_JOIN_QUIET_CANCEL=<frame>[:b|esc|click] (not in the kit) */
    int test_how;
    int test_file;
} P;

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
inline uint32_t mix(uint32_t a, uint32_t b, int k)
{
    const int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255, br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return 0xFF000000u | (uint32_t)(((ar * (255 - k) + br * k) / 255) << 16) | (uint32_t)(((ag * (255 - k) + bg * k) / 255) << 8) | (uint32_t)((ab * (255 - k) + bb * k) / 255);
}
inline uint32_t lerp3(uint32_t a, uint32_t b, int num, int den)
{
    if (den < 1) den = 1;
    return mix(a, b, clampi(num * 255 / den, 0, 255));
}
inline int rr_cov(int x, int y, int w, int h, int r)
{
    int dx = 0, dy = 0;
    if (x < r) dx = r - x - 1; else if (x >= w - r) dx = x - (w - r);
    if (y < r) dy = r - y - 1; else if (y >= h - r) dy = y - (h - r);
    if (!dx && !dy) return 255;
    const float d = sqrtf((float)(dx * dx + dy * dy)) + 0.5f;
    const float c = (float)r + 0.5f - d;
    return c >= 1.f ? 255 : c <= 0.f ? 0 : (int)(c * 255.f);
}

struct Pal { int top, bot, edge, hi; };

/* the online pages' code drawn plank: the DS plank's wood, bevel, border and grain (the same look hal/menu_online_draw.cpp draws) */
void plank(uint32_t *dst, int w, int h, int X, int Y, int W, int H, const Pal &pa)
{
    const int r = clampi(H * 14 / 100, 2, 40);
    const int bw = clampi(H * 7 / 100, 1, 12);
    const int bev = clampi(H / 9, 2, 14);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const int c = rr_cov(x, y, W, H, r);
            if (!c) continue;
            const int px = X + x, py = Y + y;
            if (px < 0 || py < 0 || px >= w || py >= h) continue;
            uint32_t col;
            const int ix = x - bw, iy = y - bw;
            const bool border = x < bw || y < bw || x >= W - bw || y >= H - bw || rr_cov(x - bw, y - bw, W - 2 * bw, H - 2 * bw, r > bw ? r - bw : 1) == 0;
            if (border) col = pa.edge;
            else {
                col = lerp3(pa.top, pa.bot, y, H);
                if (iy < bev) col = mix(col, pa.hi, 90);
                else if (y >= H - bw - bev) col = mix(col, pa.edge, 70);
                if (ix < bev / 2) col = mix(col, pa.hi, 50);
                if (x >= W - bw - bev / 2) col = mix(col, pa.edge, 45);
                if (((y * 7 + (x >> 3) * 3) % 23) == 0) col = mix(col, pa.edge, 28);
            }
            uint32_t *p = dst + (size_t)py * w + px;
            *p = c >= 255 ? (0xFF000000u | (col & 0xFFFFFF)) : mix(*p, col & 0xFFFFFF, c);
        }
    }
}

/* centred label on a plank: two passes (a lighter offset shadow under the dark lettering), shrunk to fit */
void label(uint32_t *dst, int w, int h, int X, int Y, int W, int H, const char *s, float cap_frac)
{
    if (!ui_text_ready()) return;
    float cap = cap_frac * (float)H;
    float em = cap / 0.75f;
    const float tw = ui_text_width(s, em), maxw = 0.86f * (float)W;
    if (tw > maxw) { em *= maxw / tw; cap = ui_text_cap_height(em); }
    const UiSurface sf = { dst, w, w, h };
    const int clip[4] = { X + 2, Y + 2, X + W - 2, Y + H - 2 };
    const float x = (float)X + ((float)W - ui_text_width(s, em)) * 0.5f;
    const float by = (float)Y + ((float)H - cap) * 0.5f + cap;
    const float sh = em * 0.07f > 1.f ? em * 0.07f : 1.f;
    ui_text_draw(&sf, x + sh * 0.6f, by + sh, s, em, 0xEBA546, 255, clip);
    ui_text_draw(&sf, x, by, s, em, 0x7A3A28, 255, clip);
}

void build(int w, int h)
{
    P.px.assign((size_t)w * h, 0xFF000000u);
    uint32_t *d = P.px.data();
    /* the plain background: the online mode's green, a very soft vertical ramp */
    for (int y = 0; y < h; ++y) {
        const uint32_t c = lerp3(0x2A7B52, 0x1D5638, y, h);
        for (int x = 0; x < w; ++x) d[(size_t)y * w + x] = c;
    }
    char title[48];
    if (P.server > 0) snprintf(title, sizeof title, "Joining server %d...", P.server);
    else snprintf(title, sizeof title, "Joining...");
    int W = clampi(w * 66 / 100, 80, h * 150 / 100);
    int H = W * 26 / 100;
    if (H > h * 30 / 100) { H = h * 30 / 100; }
    const int X = (w - W) / 2, Y = h * 36 / 100 - H / 2;
    const Pal gold = { 0xF6C060, 0xD99A38, 0x6A3A18, 0xFFE9A8 };
    plank(d, w, h, X, Y, W, H, gold);
    label(d, w, h, X, Y, W, H, title, 0.30f);
    /* "Press B to cancel" and the BACK plank */
    if (ui_text_ready()) {
        const float em = (float)H * 0.30f;
        const UiSurface sf = { d, w, w, h };
        const char *hint = "Press B to cancel";
        const float tw = ui_text_width(hint, em);
        ui_text_draw(&sf, ((float)w - tw) * 0.5f, (float)(Y + H) + (float)H * 0.62f, hint, em, 0xFFF3D0, 255, 0);
    }
    const int BW = W * 40 / 100, BH = H * 62 / 100;
    const int BX = (w - BW) / 2, BY = Y + H + H * 90 / 100;
    const Pal orange = { 0xF58838, 0xD2641E, 0x6A2E0C, 0xFFB878 };
    plank(d, w, h, BX, BY, BW, BH, orange);
    label(d, w, h, BX, BY, BW, BH, "BACK", 0.34f);
    P.bx = BX; P.by = BY; P.bw = BW; P.bh = BH;
    P.pw = w; P.ph = h;
    fprintf(stderr, "[joinflow] plate %dx%d built; BACK plank at %d,%d size %dx%d\n", w, h, BX, BY, BW, BH);
}

int do_cancel()
{
    const char *why = P.cancel == 2 ? "the walk did not reach the adventure in time" : "the player cancelled";
    fprintf(stderr, "[joinflow] cancel (%s): clearing the join environment and starting the solo title (%u frames in)\n", why, P.frames);
    port::online_join_clear_env();
#ifndef PORT_ROM_CLEAN
    /* SM64DS_JOIN_QUIET_CANCEL_FILE=<0..2>: the solo title that follows presses that file (proof knob, compiled out of the kit) */
    if (P.test_file >= 0) {
        char v[2] = { (char)('0' + P.test_file), 0 };
        SetEnvironmentVariableA("SM64DS_BOOT_FILE", v);
    }
#endif
#ifndef PORT_ROM_CLEAN
    /* a proof run reading back every picture (SM64DS_JOINFLOW_OFFSCREEN): the solo title that follows writes its own pictures, solo_N, not over pic_N */
    {
        char nm[512];
        if (GetEnvironmentVariableA("SM64DS_PRESENT_OFFSCREEN_BMP", nm, sizeof nm) && nm[0]) {
            std::string t(nm);
            const size_t at = t.find("pic_%d");
            if (at != std::string::npos) { t.replace(at, 6, "solo_%d"); SetEnvironmentVariableA("SM64DS_PRESENT_OFFSCREEN_BMP", t.c_str()); }
        }
    }
#endif
    port_join_quiet_cancelled = 1;
    const int ok = port_title_front_door_hook ? port_title_front_door_hook() : 0;
    const unsigned long err = ok ? 0 : GetLastError();
    if (ok) fprintf(stderr, "[joinflow] the solo title was started; this process is quitting\n");
    else fprintf(stderr, "[joinflow] the solo title could not be started (win32 %lu); quitting anyway\n", err);
    fflush(stderr);
    return 1;                                /* the plate stays up on the last picture until the process is gone */
}

int tick()
{
    if (!P.up) return 0;
    if (!P.said) {
        P.said = 1;
        fprintf(stderr, "[joinflow] quiet entry: the Joining plate covers every picture until the adventure is entered (server %d)\n", P.server);
        fflush(stderr);
    }
    ++P.frames;
    sd_host_mute(1);
    if (port_join_quiet_cancelled) return 1;
    if (!P.cancel && GetTickCount64() - P.t0 > kGiveUpMs) P.cancel = 2;
#ifndef PORT_ROM_CLEAN
    if (!P.cancel && P.test_at >= 0 && (int)P.frames == P.test_at) {
        if (P.test_how == 0) port_join_plate_input(0, 1);
        else if (P.test_how == 1) port_join_plate_input(1, 0);
        else port_join_plate_click(P.bx + P.bw / 2, P.by + P.bh / 2);
        fprintf(stderr, "[joinflow] test cancel injected at frame %u (%s)\n", P.frames, P.test_how == 0 ? "B" : P.test_how == 1 ? "Esc" : "click on BACK");
    }
#endif
    if (P.cancel) return do_cancel();
    return 0;
}

void ended(const char *why)
{
    if (!P.up || port_join_quiet_cancelled) return;
    P.up = 0;
    sd_host_mute(0);
    fprintf(stderr, "[joinflow] plate down after %u frames: %s\n", P.frames, why ? why : "");
    fflush(stderr);
}

int boot()
{
    const char *e = getenv("SM64DS_JOIN_QUIET");
    if (e && *e) {
        P.up = 1;
        P.server = atoi(e);
        P.t0 = GetTickCount64();
        P.test_at = -1;
        P.test_file = -1;
        SetEnvironmentVariableA("SM64DS_JOIN_QUIET", nullptr);           /* never inherited by this process's own successors */
        sd_host_mute(1);
#ifndef PORT_ROM_CLEAN
        if (const char *t = getenv("SM64DS_JOIN_QUIET_CANCEL")) {
            P.test_at = atoi(t);
            const char *c = strchr(t, ':');
            P.test_how = (c && !_stricmp(c + 1, "esc")) ? 1 : (c && !_stricmp(c + 1, "click")) ? 2 : 0;
        }
        if (const char *f = getenv("SM64DS_JOIN_QUIET_CANCEL_FILE")) P.test_file = atoi(f);
#endif
    }
    port_join_plate_up_hook = port_join_plate_up;
    port_join_quiet_tick_hook = tick;
    port_join_quiet_end_hook = ended;
    return 1;
}
const int g_seated = boot();

}  // namespace

extern "C" int port_join_plate_up(void) { return P.up; }

extern "C" const uint32_t *port_join_plate_picture(int w, int h)
{
    if (!P.up || w < 32 || h < 32) return nullptr;
    if (P.pw != w || P.ph != h || P.px.empty()) build(w, h);
    return P.px.data();
}

extern "C" int port_join_plate_click(int cx, int cy)
{
    if (!P.up) return 0;
    if (!P.cancel && cx >= P.bx && cx < P.bx + P.bw && cy >= P.by && cy < P.by + P.bh) {
        P.cancel = 1;
        fprintf(stderr, "[joinflow] BACK clicked\n");
    }
    return 1;
}

extern "C" void port_join_plate_input(int esc_down, int b_down)
{
    if (!P.up) return;
    /* a key or button already down when the plate came up cannot cancel until it has been let go */
    if (!P.armed) {
        if (!esc_down && !b_down) P.armed = 1;
        P.prev_esc = esc_down; P.prev_b = b_down;
        return;
    }
    if (!P.cancel && ((esc_down && !P.prev_esc) || (b_down && !P.prev_b))) {
        P.cancel = 1;
        fprintf(stderr, "[joinflow] %s pressed\n", esc_down && !P.prev_esc ? "Esc" : "B");
    }
    P.prev_esc = esc_down; P.prev_b = b_down;
}

extern "C" int port_join_plate_fast(void)
{
    if (!P.up || port_join_quiet_cancelled) return 0;
#ifndef PORT_ROM_CLEAN
    static int off = -1;
    if (off < 0) { const char *e = getenv("SM64DS_JOIN_QUIET_PACED"); off = (e && *e) ? 1 : 0; }
    if (off) return 0;
#endif
    return (P.frames & 3u) < (unsigned)kFastKeep;
}

extern "C" void port_join_settings_dump(int cw, int ch, int fullscreen, int maximised, int hand_sized)
{
    static int done;
    if (done) return;
    done = 1;
    char winbuf[32];
    if (cw > 0 && ch > 0) snprintf(winbuf, sizeof winbuf, "%dx%d", cw, ch); else snprintf(winbuf, sizeof winbuf, "minimised");
    char bundle[260] = "", arr[512] = "";
    host_setting_enabled_mods(bundle, sizeof bundle, arr, sizeof arr);
    {   /* the launcher writes settings.json pretty printed: keep the array on this one line */
        char *w = arr;
        for (const char *r = arr; *r; ++r)
            if ((unsigned char)*r > 0x20) *w++ = *r;
        *w = 0;
    }
    std::string ids;
    for (int i = 0; i < port_mods_count(); ++i) { if (i) ids += ","; ids += port_mods_id(i); }
    fprintf(stderr,
            "[joinflow] SETTINGS process=%s window=%s fullscreen=%d maximised=%d hand_sized=%d frame_rate=%d smooth_motion=%d render_scale=%d "
            "hd_textures=%d smooth_models=%d texture_filter=%d anti_aliasing=%d present_backend=%d present_filter=%d vsync=%d renderer=%d "
            "one_screen_menus=%d hud_layout=%d volume=%d draw_distance=%d camera_mode=%d start_fullscreen=%d adventure_ghosts=%d name_tags=%d "
            "improved_minimap=%d save_menu_on_top=%d character_strip=%d keep_lives=%d skip_menu=%d skip_intro=%d aspect_env=%s dual_screen_env=%s "
            "mods_in_file=%s mods_in_effect=%d[%s] multiplayer=%d\n",
            getenv("SM64DS_ADVENTURE") ? "adventure" : "solo", winbuf, fullscreen, maximised, hand_sized,
            host_setting_frame_rate(), host_setting_smooth_motion(), host_setting_render_scale(), host_setting_hd_textures(),
            host_setting_smooth_models(), host_setting_texture_filter(), host_setting_anti_aliasing(), host_setting_present_backend(),
            host_setting_present_filter(), host_setting_vsync(), host_setting_renderer(), host_setting_one_screen_menus(), host_setting_hud_layout(),
            host_setting_volume(), host_setting_draw_distance_pct(), host_setting_camera_mode(), host_setting_start_fullscreen(),
            host_setting_adventure_ghosts(), host_setting_name_tags(), host_setting_improved_minimap(), host_setting_save_menu_on_top(),
            host_setting_character_strip(), host_setting_keep_lives(), getenv("SM64DS_SKIP_MENU") ? 1 : 0, getenv("SM64DS_SKIP_INTRO") ? 1 : 0,
            getenv("SM64DS_ASPECT") ? getenv("SM64DS_ASPECT") : "-", getenv("SM64DS_DUAL_SCREEN") ? getenv("SM64DS_DUAL_SCREEN") : "-",
            arr[0] ? arr : "-", port_mods_count(), ids.c_str(), port_mods_multiplayer());
    fflush(stderr);
}
