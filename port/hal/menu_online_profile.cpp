/* menu_online_profile.cpp -- the Adventure Online name and colour popup, logic and draw (run after093, lane PROFILE1). See menu_online_profile.h.

   KEYBOARD ONLY, BY POLLING. The name is typed by reading the keys' state once per logic tick (the same gates every other reader of the keyboard
   uses: the window must have focus, or a scripted SM64DS_HOST_KEY press stands in for the key), not through WM_CHAR: nothing in the window procedure
   changes, and a scripted run can type. A press is an edge of the key state; the popup opening with a key already down (the Space or Enter that
   picked the file) types nothing. Shift gives capitals (Caps Lock inverts, as on any keyboard). Backspace arrives as the host back key; the name field
   takes it as "delete", every other row as BACK.

   W A S D are the title's default D-pad keys, so while the name field has the focus a D-pad bit that comes from one of those letters alone (its
   arrow key not down) belongs to the letter and is dropped; the arrow keys, a pad and the stick still move the focus. */
#include "menu_online.h"
#include "menu_online_profile.h"
#include "host_settings.h"
#include "ui_text.h"
#include "menu_hostimg.h"
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" int port_host_key_script(int vk);       /* tests/walk_window.cpp: SM64DS_HOST_KEY, 0 when unset */
extern "C" void port_host_keys_clear(unsigned short mask);   /* hal/scene_boot.cpp */
extern "C" void settings_page_press_sound(void);   /* hal/settings_page_hooks.cpp: the title's press sound (INPUTMATRIX1) */
extern "C" int hal_window_focused(void);           /* hal/sub_screen.cpp */

namespace {

struct Prof {
    int open;
    int pending;
    int focus;
    int colour;
    char name[20];
    int len;
    unsigned tick;
    unsigned char prev[256];
} P;

/* user32 is loaded by name, like every other reader of the keyboard in the port (tools/sdl_import_guard.py refuses a static USER32 import) */
SHORT (WINAPI *p_GetAsyncKeyState)(int);
SHORT (WINAPI *p_GetKeyState)(int);
void load_user32()
{
    static int done;
    if (done) return;
    done = 1;
    if (HMODULE u = LoadLibraryA("user32.dll")) {
        p_GetAsyncKeyState = (SHORT (WINAPI *)(int))GetProcAddress(u, "GetAsyncKeyState");
        p_GetKeyState = (SHORT (WINAPI *)(int))GetProcAddress(u, "GetKeyState");
    }
}

int key_down(int vk)
{
    if (port_host_key_script(vk)) return 1;
    load_user32();
    return p_GetAsyncKeyState && hal_window_focused() && (p_GetAsyncKeyState(vk) & 0x8000) ? 1 : 0;
}

void log_name(const char *why)
{
    fprintf(stderr, "[profile] %s: name \"%s\" colour %d (%s)\n", why, P.name, P.colour, kOnlineYoshiColors[P.colour].name);
}

void type_char(char c)
{
    if (P.len >= 16) return;
    if (c == ' ' && (P.len == 0 || P.name[P.len - 1] == ' ')) return;      /* no leading or doubled spaces: the name is stored trimmed */
    P.name[P.len++] = c;
    P.name[P.len] = 0;
}

void finish(int ok)
{
    char clean[20];
    const int pending = P.pending;
    settings_page_press_sound();      /* a button press sounds like every other title button's */
    if (ok) {
        host_online_name_clean(P.name, clean, sizeof clean);
        if (!clean[0]) snprintf(clean, sizeof clean, "%s", "Player");
        host_setting_set_online_profile(clean, P.colour);
        snprintf(P.name, sizeof P.name, "%s", clean);
        log_name("saved");
    } else {
        fprintf(stderr, "[profile] back: nothing saved\n");
    }
    P.open = 0;
    menu_online_profile_done(ok, pending);
}

}  // namespace

void profile_open(int pending_slot)
{
    memset(&P, 0, sizeof P);
    P.open = 1;
    P.pending = pending_slot;
    P.focus = PROF_F_NAME;
    const char *start = host_setting_online_name();
    if (!start[0]) start = host_setting_launcher_nick();
    if (!start[0]) start = "Player";
    snprintf(P.name, sizeof P.name, "%s", start);
    P.len = (int)strlen(P.name);
    const int c = host_setting_online_yoshi_color();
    P.colour = c >= 0 && c < HOST_ONLINE_COLORS ? c : 0;
    /* keys already down when the popup opens type nothing */
    for (int vk = 0; vk < 256; ++vk) P.prev[vk] = (unsigned char)(key_down(vk) ? 1 : 0);
    fprintf(stderr, "[profile] popup open (file %d): name \"%s\" colour %d (%s)\n", pending_slot, P.name, P.colour, kOnlineYoshiColors[P.colour].name);
}

int profile_active(void) { return P.open; }

/* THE CLOSE SWALLOW FOR PAD B. The press that closes a screen of the online menu is still down on the next frames; the file select reads that
   bit as its back arrow, so a popup or ? text closed by B used to leave the file select as well (found on the int100g exe with the ? text).
   The bit is kept out of both key words until the key is up. */
namespace { int g_b_strip; }
void profile_punch_taken(void) { g_b_strip = 1; port_host_keys_clear(0x0001); }      /* the title also reads the host key word: clear it on the edge frame too */
void profile_punch_filter(uint16_t *pressed, unsigned short *held)
{
    if (!g_b_strip) return;
    if ((*pressed | *held) & 0x0001u) {
        *pressed = (uint16_t)(*pressed & ~0x0001u);
        *held = (unsigned short)(*held & ~0x0001u);
        port_host_keys_clear(0x0001);
    } else {
        g_b_strip = 0;
    }
}
int profile_name_needed(void) { return host_setting_online_name()[0] == 0; }
const char *profile_saved_name(void) { return host_setting_online_name(); }

void profile_facts(ProfFacts *f)
{
    memset(f, 0, sizeof *f);
    f->focus = P.focus;
    f->colour = P.colour;
    f->caret = (int)((P.tick / 30) & 1);
    f->pending = P.open ? P.pending : -1;
    snprintf(f->name, sizeof f->name, "%s", P.name);
}

void profile_tick(void)
{
    if (!P.open) return;
    ++P.tick;
    /* Esc is BACK (pad B and Backspace on the other rows go through profile_back) */
    {
        const int esc = key_down(VK_ESCAPE);
        const int fresh = esc && !P.prev[VK_ESCAPE];
        P.prev[VK_ESCAPE] = (unsigned char)esc;
        if (fresh && P.tick > 8) { finish(0); return; }
    }
    if (P.focus != PROF_F_NAME) {
        for (int vk = 0; vk < 256; ++vk) P.prev[vk] = (unsigned char)(key_down(vk) ? 1 : 0);
        return;
    }
    const int shift = key_down(VK_SHIFT) ? 1 : 0;
    const int caps = (p_GetKeyState && (p_GetKeyState(VK_CAPITAL) & 1)) ? 1 : 0;
    const int upper = shift ^ caps;
    static const int kRanges[][2] = { { 'A', 'Z' }, { '0', '9' }, { VK_NUMPAD0, VK_NUMPAD9 }, { VK_SPACE, VK_SPACE } };
    int typed = 0;
    for (auto &r : kRanges)
        for (int vk = r[0]; vk <= r[1]; ++vk) {
            const int d = key_down(vk);
            const int fresh = d && !P.prev[vk];
            P.prev[vk] = (unsigned char)d;
            if (!fresh || P.tick <= 2) continue;
            char c;
            if (vk >= 'A' && vk <= 'Z') c = (char)(upper ? vk : vk + 32);
            else if (vk >= '0' && vk <= '9') c = (char)vk;
            else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) c = (char)('0' + vk - VK_NUMPAD0);
            else c = ' ';
            type_char(c);
            typed = 1;
        }
    if (typed) log_name("typed");
}

/* the pad word while the popup is up. dirs: 0x10 right, 0x20 left, 0x40 up, 0x80 down; confirm: 0x0002 (pad A, Space) and 0x0008 (Start, Enter). The
   bits it answers are the bits taken; they are taken even when nothing is done with them (the ROM's page is behind a modal). */
unsigned profile_pad(unsigned pressed, unsigned held)
{
    if (!P.open) return 0;
    const unsigned take = 0x00f0u | 0x0002u | 0x0008u;
    unsigned edge = pressed & take;
    if (P.focus == PROF_F_NAME) {
        /* a D-pad bit that is only a letter being typed (W A S D) is not a step */
        static const struct { unsigned bit; int arrow, letter; } kD[4] = { { 0x40, VK_UP, 'W' }, { 0x80, VK_DOWN, 'S' }, { 0x20, VK_LEFT, 'A' }, { 0x10, VK_RIGHT, 'D' } };
        for (auto &d : kD)
            if ((edge & d.bit) && key_down(d.letter) && !key_down(d.arrow)) edge &= ~d.bit;
        edge &= ~0x0002u;       /* Space is a typed space on the name row; pad A there does nothing */
    }
    if (!edge) return (pressed | held) & take;
    const int old_focus = P.focus;
    if (edge & 0x40) { if (P.focus > PROF_F_NAME) P.focus = P.focus == PROF_F_BACK ? PROF_F_COLOUR : P.focus - 1; }
    if (edge & 0x80) { if (P.focus < PROF_F_OK) P.focus++; }
    if (edge & 0x20) {
        if (P.focus == PROF_F_COLOUR) P.colour = (P.colour + 7) % 8;
        else if (P.focus == PROF_F_BACK) P.focus = PROF_F_OK;
    }
    if (edge & 0x10) {
        if (P.focus == PROF_F_COLOUR) P.colour = (P.colour + 1) % 8;
        else if (P.focus == PROF_F_OK) P.focus = PROF_F_BACK;
    }
    if (old_focus != P.focus || (edge & 0x30)) fprintf(stderr, "[profile] focus %d colour %d\n", P.focus, P.colour);
    if (edge & 0x0008) { if (P.tick > 8) { finish(P.focus == PROF_F_BACK ? 0 : 1); return take; } }   /* Enter / Start: the focused button; OK from every other row */
    if (edge & 0x0002) {
        if (P.tick > 8) {
            if (P.focus == PROF_F_OK) { finish(1); return take; }
            if (P.focus == PROF_F_BACK) { finish(0); return take; }
            if (P.focus == PROF_F_COLOUR) P.focus = PROF_F_OK;                               /* A on the colours settles on OK */
        }
    }
    return (pressed | held) & take;
}

int profile_touch(int sx, int sy, int edge)
{
    if (!P.open) return 0;
    if (!edge) return 1;
    auto in = [&](int x, int y, int w, int h) { return sx >= x && sx < x + w && sy >= y && sy < y + h; };
    if (in(PROF_OK_X, PROF_OK_Y, PROF_OK_W, PROF_OK_H)) { if (P.tick > 8) finish(1); return 1; }
    if (in(PROF_BACK_X, PROF_BACK_Y, PROF_BACK_W, PROF_BACK_H)) { if (P.tick > 8) finish(0); return 1; }
    if (in(PROF_FIELD_X, PROF_FIELD_Y, PROF_FIELD_W, PROF_FIELD_H)) { P.focus = PROF_F_NAME; return 1; }
    for (int i = 0; i < 8; ++i)
        if (in(PROF_SW_X + i * PROF_SW_STEP, PROF_SW_Y, PROF_SW_W, PROF_SW_H)) {
            P.colour = i;
            P.focus = PROF_F_COLOUR;
            fprintf(stderr, "[profile] colour %d picked by click\n", i);
            return 1;
        }
    return 1;
}

int profile_back(void)
{
    if (!P.open) return 0;
    if (P.tick > 2) finish(0);
    return 1;
}

int profile_backspace(void)
{
    if (!P.open) return 0;
    if (P.focus == PROF_F_NAME) {
        if (P.len > 0) { P.name[--P.len] = 0; log_name("backspace"); }
        return 1;
    }
    return profile_back();
}

/* ---- draw side ----------------------------------------------------------------------------------------------------- */
namespace {

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
inline uint32_t hex6(const char *s)
{
    uint32_t v = 0;
    for (int i = 0; i < 6; ++i) {
        const char c = s[i];
        v = v * 16 + (uint32_t)(c <= '9' ? c - '0' : (c | 32) - 'a' + 10);
    }
    return v;
}
void fill(uint32_t *dst, int stride, int w, int h, int x0, int y0, int x1, int y1, uint32_t rgb)
{
    x0 = clampi(x0, 0, w); x1 = clampi(x1, 0, w); y0 = clampi(y0, 0, h); y1 = clampi(y1, 0, h);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) dst[(size_t)y * stride + x] = 0xFF000000u | rgb;
}
void frame(uint32_t *dst, int stride, int w, int h, int x0, int y0, int x1, int y1, int t, uint32_t rgb)
{
    fill(dst, stride, w, h, x0, y0, x1, y0 + t, rgb);
    fill(dst, stride, w, h, x0, y1 - t, x1, y1, rgb);
    fill(dst, stride, w, h, x0, y0, x0 + t, y1, rgb);
    fill(dst, stride, w, h, x1 - t, y0, x1, y1, rgb);
}

}  // namespace

void menu_online_profile_draw(const MenuLayout *L, const OnlFacts *of, uint32_t *dst, int w, int h, int stride)
{
    const ProfFacts &f = of->prof;
    const int un = L->unum, ud = L->uden;
    auto X = [&](int dsx) { return w / 2 + (dsx - 128) * un / ud; };
    auto Y = [&](int dsy) { return dsy * un / ud; };
    auto S = [&](int u) { return u * un / ud; };
    /* the panel: the parchment of the other modal screens, a little taller */
    const int px = X(PROF_PANEL_X), py = Y(PROF_PANEL_Y), pw = S(PROF_PANEL_W), ph = S(PROF_PANEL_H);
    const int bw = clampi(S(3), 2, 10);
    fill(dst, stride, w, h, px, py, px + pw, py + ph, 0x8A5A2A);
    for (int y = py + bw; y < py + ph - bw; ++y) {
        const int k = (y - py) * 255 / ph;
        const uint32_t r = (0xF4 * (255 - k) + 0xE6 * k) / 255, g = (0xE4 * (255 - k) + 0xCE * k) / 255, b = (0xB8 * (255 - k) + 0x96 * k) / 255;
        fill(dst, stride, w, h, px + bw, y, px + pw - bw, y + 1, (r << 16) | (g << 8) | b);
    }
    if (!ui_text_ready()) return;
    const UiSurface sf = { dst, stride, w, h };
    const float em = 10.5f * (float)un / (float)ud;
    ui_text_draw(&sf, (float)X(PROF_FIELD_X), (float)Y(28), "Your name", em, 0x6B3A12, 255, 0);
    /* the field */
    const int fx0 = X(PROF_FIELD_X), fy0 = Y(PROF_FIELD_Y), fx1 = X(PROF_FIELD_X + PROF_FIELD_W), fy1 = Y(PROF_FIELD_Y + PROF_FIELD_H);
    fill(dst, stride, w, h, fx0, fy0, fx1, fy1, 0xFFF8E4);
    frame(dst, stride, w, h, fx0, fy0, fx1, fy1, clampi(S(f.focus == PROF_F_NAME ? 2 : 1), 1, 6), f.focus == PROF_F_NAME ? 0xD2641E : 0x8A5A2A);
    const float fem = 12.f * (float)un / (float)ud;
    const float fbase = (float)(fy0 + fy1) * 0.5f + ui_text_cap_height(fem) * 0.5f;
    const int clip[4] = { fx0 + 2, fy0 + 2, fx1 - 2, fy1 - 2 };
    const float tx = (float)(fx0 + S(6));
    ui_text_draw(&sf, tx, fbase, f.name, fem, 0x4A2208, 255, clip);
    if (f.focus == PROF_F_NAME && f.caret) {
        const int cx = (int)(tx + ui_text_width(f.name, fem)) + S(1);
        fill(dst, stride, w, h, cx, fy0 + S(4), cx + clampi(S(1), 1, 3), fy1 - S(4), 0x4A2208);
    }
    ui_text_draw(&sf, (float)X(PROF_FIELD_X), (float)Y(64), "keyboard to change", 7.5f * (float)un / (float)ud, 0x9A7A4A, 255, 0);
    ui_text_draw(&sf, (float)X(PROF_FIELD_X), (float)Y(82), "Your colour", em, 0x6B3A12, 255, 0);
    /* the swatches: body above, shoes below */
    for (int i = 0; i < 8; ++i) {
        const int x0 = X(PROF_SW_X + i * PROF_SW_STEP), y0 = Y(PROF_SW_Y), x1 = X(PROF_SW_X + i * PROF_SW_STEP + PROF_SW_W), y1 = Y(PROF_SW_Y + PROF_SW_H);
        const int ym = y0 + (y1 - y0) * 68 / 100;
        const int sel = i == f.colour;
        if (sel) frame(dst, stride, w, h, x0 - S(3), y0 - S(3), x1 + S(3), y1 + S(3), clampi(S(2), 1, 5), f.focus == PROF_F_COLOUR ? 0xD2641E : 0x4A2208);
        fill(dst, stride, w, h, x0, y0, x1, ym, hex6(kOnlineYoshiColors[i].body));
        fill(dst, stride, w, h, x0, ym, x1, y1, hex6(kOnlineYoshiColors[i].shoes));
        frame(dst, stride, w, h, x0, y0, x1, y1, 1, 0x4A2208);
    }
    if (f.focus == PROF_F_OK || f.focus == PROF_F_BACK) {
        const int bx = f.focus == PROF_F_OK ? PROF_OK_X : PROF_BACK_X, by = f.focus == PROF_F_OK ? PROF_OK_Y : PROF_BACK_Y;
        const int bw2 = f.focus == PROF_F_OK ? PROF_OK_W : PROF_BACK_W, bh2 = f.focus == PROF_F_OK ? PROF_OK_H : PROF_BACK_H;
        const int dh = S(bh2), dw = menu_online_plank_w(f.focus == PROF_F_OK ? ONL_K_OK : ONL_K_BACK, dh);
        const int cx = X(bx + bw2 / 2);
        frame(dst, stride, w, h, cx - dw / 2 - S(3), Y(by) - S(2), cx + dw / 2 + S(3), Y(by) + dh + S(2), clampi(S(2), 1, 5), 0xD2641E);
    }
    {
        const char *nm = kOnlineYoshiColors[clampi(f.colour, 0, 7)].name;
        const float cem = 9.f * (float)un / (float)ud;
        ui_text_draw(&sf, (float)X(128) - ui_text_width(nm, cem) * 0.5f, (float)Y(122), nm, cem, 0x6B3A12, 255, 0);
    }
}

namespace {
void profile_panel_cb(const MenuLayout *L, int piece, uint32_t *dst, int w, int h, int stride)
{
    (void)piece;
    const OnlFacts *f = onl_facts_of(L);
    if (!f || f->scr != ONL_SCR_PROFILE || w < 64 || h < 64) return;
    /* the veil, as the other modal screens */
    for (int y = 0; y < h; ++y) {
        uint32_t *p = dst + (size_t)y * stride;
        for (int x = 0; x < w; ++x) {
            const uint32_t c = p[x];
            p[x] = 0xFF000000u | ((((c >> 16) & 255) * 96 / 255) << 16) | ((((c >> 8) & 255) * 96 / 255) << 8) | ((c & 255) * 96 / 255);
        }
    }
    menu_online_profile_draw(L, f, dst, w, h, stride);
}

/* a transparent touch piece over a DS box (the click maps proportionally into the box), with a hotspot */
void touch_box(MenuLayout *L, int bx, int by, int bw, int bh)
{
    const int un = L->unum, ud = L->uden;
    const int fx = L->w / 2 + (bx - 128) * un / ud, fy = by * un / ud, fw = bw * un / ud, fh = bh * un / ud;
    MenuPiece pl = menu_piece_make(SRC_HOSTIMG, bx, by, bw, bh, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
    pl.lden = 1; pl.lnum = 1; pl.fw = (short)fw; pl.fh = (short)fh; pl.fx = (short)fx; pl.fy = (short)fy;
    pl.touch = 1;
    pl.nodraw = 1;
    const int ip = menu_add_piece(L, pl);
    if (ip >= 0) menu_add_hotspot(L, ip, bx, by, bw, bh, -1);
}
}  // namespace

void menu_online_profile_modal(MenuLayout *L, const OnlFacts *f)
{
    MenuPiece p = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
    p.bg = 1; p.fixdest = 1; p.fx = 0; p.fy = 0; p.fw = (short)L->w; p.fh = (short)L->h;
    p.custom = profile_panel_cb;
    menu_add_piece(L, p);
    touch_box(L, PROF_FIELD_X, PROF_FIELD_Y, PROF_FIELD_W, PROF_FIELD_H);
    for (int i = 0; i < 8; ++i) touch_box(L, PROF_SW_X + i * PROF_SW_STEP, PROF_SW_Y, PROF_SW_W, PROF_SW_H);
    const int un = L->unum, ud = L->uden;
    const int kinds[2] = { ONL_K_OK, ONL_K_BACK };
    const int bxs[2][4] = { { PROF_OK_X, PROF_OK_Y, PROF_OK_W, PROF_OK_H }, { PROF_BACK_X, PROF_BACK_Y, PROF_BACK_W, PROF_BACK_H } };
    for (int k = 0; k < 2; ++k) {
        const int dh = bxs[k][3] * un / ud, dw = menu_online_plank_w(kinds[k], dh);
        const int cx = L->w / 2 + (bxs[k][0] + bxs[k][2] / 2 - 128) * un / ud;
        const int lit = 1;      /* both planks always bright (as NAME1's modal buttons); the focus is the ring the panel draws around the box */
        const int ip = menu_online_add_plank(L, kinds[k], cx - dw / 2, bxs[k][1] * un / ud, dw, dh, bxs[k][0], bxs[k][1], bxs[k][2], bxs[k][3], lit, 0, 0, 1, 0);
        if (ip >= 0) menu_add_hotspot(L, ip, bxs[k][0], bxs[k][1], bxs[k][2], bxs[k][3], -1);
    }
}

void menu_online_add_name_plank(MenuLayout *L, int right_edge_x, int top_y, int h_px)
{
    const int dw = menu_online_plank_w(ONL_K_NAME, h_px);
    const int ip = menu_online_add_plank(L, ONL_K_NAME, right_edge_x - dw, top_y, dw, h_px, ONL_NAME_X, ONL_NAME_Y, ONL_NAME_W, ONL_NAME_H, 0, 0, 0, 1, 0);
    if (ip >= 0) menu_add_hotspot(L, ip, ONL_NAME_X, ONL_NAME_Y, ONL_NAME_W, ONL_NAME_H, -1);
}
