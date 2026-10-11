/* The SDL3 half of the controller backend. Read pad_sdl.h first. */
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "hal/pad_sdl.h"

/* XINPUT_GAMEPAD_* spelled here, as in pad_backend.cpp, so no xinput.h is needed. */
enum {
    XB_UP = 0x0001, XB_DOWN = 0x0002, XB_LEFT = 0x0004, XB_RIGHT = 0x0008,
    XB_START = 0x0010, XB_BACK = 0x0020, XB_LTHUMB = 0x0040, XB_RTHUMB = 0x0080,
    XB_LB = 0x0100, XB_RB = 0x0200,
    XB_A = 0x1000, XB_B = 0x2000, XB_X = 0x4000, XB_Y = 0x8000
};

/* SDL gamepad button -> XInput bit. Positional (south = A): see pad_sdl.h. */
static const struct { int sdl; unsigned xb; } BTN_MAP[] = {
    {SDL_GAMEPAD_BUTTON_SOUTH, XB_A},          {SDL_GAMEPAD_BUTTON_EAST, XB_B},
    {SDL_GAMEPAD_BUTTON_WEST, XB_X},           {SDL_GAMEPAD_BUTTON_NORTH, XB_Y},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, XB_LB}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, XB_RB},
    {SDL_GAMEPAD_BUTTON_BACK, XB_BACK},        {SDL_GAMEPAD_BUTTON_START, XB_START},
    {SDL_GAMEPAD_BUTTON_LEFT_STICK, XB_LTHUMB},{SDL_GAMEPAD_BUTTON_RIGHT_STICK, XB_RTHUMB},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, XB_UP},       {SDL_GAMEPAD_BUTTON_DPAD_DOWN, XB_DOWN},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, XB_LEFT},   {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, XB_RIGHT},
};
enum { NBTN = sizeof BTN_MAP / sizeof BTN_MAP[0] };

enum { MAXPADS = 4 };
struct SPad {
    SDL_JoystickID id;
    SDL_Gamepad *g;
    unsigned vid, pid;
    char name[96];
    char type[24];
    int has_mapping;
    Uint64 act;             /* tick of the last press or push, 0 = never */
    PortPadState last;      /* for the packet counter */
    unsigned long pkt;
};
static SPad g_pads[MAXPADS];
static int g_npads;
static int g_active = -1;
static int g_inited;
static int (*g_learned)(unsigned, unsigned);
static int g_virt_only;     /* test: ignore every real pad */

static short stick(Sint16 v) { return v < -32767 ? (short)-32767 : (short)v; }
static unsigned char trigger(Sint16 v) { return v <= 0 ? 0 : (unsigned char)((int)v * 255 / 32767); }

static void read_pad(SPad *p, PortPadState *out)
{
    unsigned m = 0;
    for (int i = 0; i < NBTN; ++i)
        if (SDL_GetGamepadButton(p->g, (SDL_GamepadButton)BTN_MAP[i].sdl)) m |= BTN_MAP[i].xb;
    out->buttons = (unsigned short)m;
    out->lt = trigger(SDL_GetGamepadAxis(p->g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    out->rt = trigger(SDL_GetGamepadAxis(p->g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
    out->lx = stick(SDL_GetGamepadAxis(p->g, SDL_GAMEPAD_AXIS_LEFTX));
    out->rx = stick(SDL_GetGamepadAxis(p->g, SDL_GAMEPAD_AXIS_RIGHTX));
    /* SDL's Y grows downward, XInput's upward */
    out->ly = (short)-stick(SDL_GetGamepadAxis(p->g, SDL_GAMEPAD_AXIS_LEFTY));
    out->ry = (short)-stick(SDL_GetGamepadAxis(p->g, SDL_GAMEPAD_AXIS_RIGHTY));
    if (memcmp(&p->last.buttons, &out->buttons, sizeof *out - sizeof out->packet)) ++p->pkt;
    out->packet = p->pkt;
    p->last = *out;
}

static const char *type_name(SDL_GamepadType t)
{
    switch (t) {
    case SDL_GAMEPAD_TYPE_STANDARD: return "standard";
    case SDL_GAMEPAD_TYPE_XBOX360: return "Xbox 360";
    case SDL_GAMEPAD_TYPE_XBOXONE: return "Xbox One";
    case SDL_GAMEPAD_TYPE_PS3: return "PS3";
    case SDL_GAMEPAD_TYPE_PS4: return "PS4";
    case SDL_GAMEPAD_TYPE_PS5: return "PS5";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO: return "Switch Pro";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT: return "Joy-Con L";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: return "Joy-Con R";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return "Joy-Con pair";
    case SDL_GAMEPAD_TYPE_GAMECUBE: return "GameCube";
    default: return "unknown";
    }
}

static void open_pad(SDL_JoystickID id)
{
    for (int i = 0; i < g_npads; ++i)
        if (g_pads[i].id == id) return;
    if (g_npads >= MAXPADS || !SDL_IsGamepad(id)) return;
    if (g_virt_only && !SDL_IsJoystickVirtual(id)) return;
    const unsigned vid = SDL_GetGamepadVendorForID(id);
    const unsigned pid = SDL_GetGamepadProductForID(id);
    /* a pad the player taught a layout stays with the DirectInput backend that layout
       is written for */
    if (g_learned && vid && g_learned(vid, pid)) {
        static SDL_JoystickID said;
        if (said != id) {
            said = id;
            fprintf(stderr, "[pad] SDL: leaving %04x:%04x to DirectInput (learned layout)\n", vid, pid);
            fflush(stderr);
        }
        return;
    }
    SDL_Gamepad *g = SDL_OpenGamepad(id);
    if (!g) {
        fprintf(stderr, "[pad] SDL: could not open %04x:%04x: %s\n", vid, pid, SDL_GetError());
        fflush(stderr);
        return;
    }
    SPad *p = &g_pads[g_npads++];
    memset(p, 0, sizeof *p);
    p->id = id;
    p->g = g;
    p->vid = vid;
    p->pid = pid;
    const char *n = SDL_GetGamepadName(g);
    strncpy(p->name, n ? n : "controller", sizeof p->name - 1);
    strncpy(p->type, type_name(SDL_GetGamepadType(g)), sizeof p->type - 1);
    char *map = SDL_GetGamepadMapping(g);
    p->has_mapping = map != 0;
    SDL_free(map);
    if (g_active < 0) g_active = g_npads - 1;
}

static void close_pad(SDL_JoystickID id)
{
    for (int i = 0; i < g_npads; ++i) {
        if (g_pads[i].id != id) continue;
        SDL_CloseGamepad(g_pads[i].g);
        for (int k = i; k + 1 < g_npads; ++k) g_pads[k] = g_pads[k + 1];
        --g_npads;
        if (g_active == i) g_active = g_npads ? 0 : -1;
        else if (g_active > i) --g_active;
        return;
    }
}

static void mark_active(SDL_JoystickID id)
{
    for (int i = 0; i < g_npads; ++i)
        if (g_pads[i].id == id) { g_active = i; g_pads[i].act = SDL_GetTicks(); return; }
}

/* ---- test: a virtual pad with a real pad's vid:pid ------------------------------ */
struct VType { const char *key, *name; unsigned vid, pid; };
static const VType VTYPES[] = {
    {"xbox", "Xbox 360 Controller", 0x045e, 0x028e},
    {"ds4", "PS4 Controller", 0x054c, 0x09cc},
    {"ds5", "DualSense Wireless Controller", 0x054c, 0x0ce6},
    {"pro", "Nintendo Switch Pro Controller", 0x057e, 0x2009},
    {"8bitdo", "8BitDo Pro 2", 0x2dc8, 0x6003},
    {"generic", "Generic USB Gamepad", 0x0079, 0x0006},
};
static SDL_JoystickID g_virt_id;
static SDL_Joystick *g_virt_joy;
static char g_script_spec[1024];

static SDL_JoystickID attach_virtual(const char *key)
{
    const VType *vt = 0;
    for (size_t i = 0; i < sizeof VTYPES / sizeof VTYPES[0]; ++i)
        if (!_stricmp(key, VTYPES[i].key)) vt = &VTYPES[i];
    if (!vt) return 0;
    SDL_VirtualJoystickDesc d;
    SDL_INIT_INTERFACE(&d);
    d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    d.naxes = SDL_GAMEPAD_AXIS_COUNT;
    d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    d.vendor_id = (Uint16)vt->vid;
    d.product_id = (Uint16)vt->pid;
    d.name = vt->name;
    return SDL_AttachVirtualJoystick(&d);
}

int pad_sdl_virtual_wanted(void) { return g_virt_only; }
int pad_sdl_script_wanted(void) { return g_script_spec[0] != 0; }

/* the XInput mask on frame f, in pad_script_mask's grammar (walk_window.cpp):
   <hex>@<f0>[-<f1>][,...]; with no -<f1> the entry holds 4 frames */
static unsigned script_mask(int frame)
{
    unsigned mask = 0;
    const char *p = g_script_spec;
    while (*p) {
        char *q;
        const unsigned m = (unsigned)strtoul(p, &q, 16);
        long f0 = -1, f1 = -1;
        p = q;
        if (*p == 64) {         /* '@' */
            f0 = strtol(p + 1, &q, 10);
            p = q;
            f1 = f0 + 3;
            if (*p == 45) { f1 = strtol(p + 1, &q, 10); p = q; }    /* '-' */
        }
        if (f0 >= 0 && frame >= f0 && frame <= f1) mask |= m;
        while (*p && *p != 44) ++p;     /* ',' */
        if (*p == 44) ++p;
    }
    return mask;
}

/* a trigger at rest reads its axis minimum on a real pad; a virtual one starts at 0, which
   SDL scales to half-pulled */
static void virt_triggers_rest(void)
{
    SDL_SetJoystickVirtualAxis(g_virt_joy, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768);
    SDL_SetJoystickVirtualAxis(g_virt_joy, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
}

void pad_sdl_script_apply(int frame)
{
    if (!g_virt_joy) return;
    const unsigned m = script_mask(frame);
    for (int i = 0; i < NBTN; ++i)
        SDL_SetJoystickVirtualButton(g_virt_joy, BTN_MAP[i].sdl, (m & BTN_MAP[i].xb) != 0);
    virt_triggers_rest();
    /* the trigger pseudo-bits of hal/host_settings.h (0x10000 LT, 0x20000 RT)
       pull the matching axis all the way, so a script can press L2 / R2 */
    if (m & 0x10000u) SDL_SetJoystickVirtualAxis(g_virt_joy, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767);
    if (m & 0x20000u) SDL_SetJoystickVirtualAxis(g_virt_joy, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
    SDL_UpdateGamepads();
}

/* ---- public ------------------------------------------------------------------ */

int pad_sdl_init(int (*learned_fn)(unsigned, unsigned))
{
    if (g_inited) return 1;
    g_learned = learned_fn;
    const char *vir = getenv("SM64DS_PAD_SDL_VIRTUAL");
    if (vir && *vir) g_virt_only = 1;
    if (const char *s = getenv("SM64DS_PAD_SDL_SCRIPT"))
        strncpy(g_script_spec, s, sizeof g_script_spec - 1);
    /* the game window is not an SDL window, so SDL must not wait for one to be
       focused before it delivers pad input; and leave the system timer resolution to the
       port (SDL would otherwise ask for 1 ms for the life of the process) */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "0");
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "[pad] SDL init failed: %s\n", SDL_GetError());
        fflush(stderr);
        return 0;
    }
    g_inited = 1;
    if (g_virt_only) {
        g_virt_id = attach_virtual(vir);
        if (!g_virt_id) {
            fprintf(stderr, "[pad] SDL virtual pad '%s' failed: %s\n", vir, SDL_GetError());
        } else {
            g_virt_joy = SDL_OpenJoystick(g_virt_id);
            if (g_virt_joy) virt_triggers_rest();
            fprintf(stderr, "[pad] SDL: TEST virtual %s pad attached%s\n", vir,
                    g_script_spec[0] ? " (scripted)" : "");
        }
        fflush(stderr);
    }
    int n = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&n);
    for (int i = 0; ids && i < n; ++i) open_pad(ids[i]);
    SDL_free(ids);
    return 1;
}

int pad_sdl_pump(PortPadState *out, char *desc, int desc_n)
{
    if (!g_inited) return 0;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_GAMEPAD_ADDED: open_pad(e.gdevice.which); break;
        case SDL_EVENT_GAMEPAD_REMOVED: close_pad(e.gdevice.which); break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: mark_active(e.gbutton.which); break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (e.gaxis.value > 12000 || e.gaxis.value < -12000) mark_active(e.gaxis.which);
            break;
        default: break;
        }
    }
    if (g_active < 0 || g_active >= g_npads) return 0;
    SPad *p = &g_pads[g_active];
    read_pad(p, out);
    if (desc && desc_n > 0)
        snprintf(desc, (size_t)desc_n, "SDL: %s (%04x:%04x, %s, %s)", p->name, p->vid, p->pid,
                 p->type, p->has_mapping ? "SDL mapping" : "no SDL mapping");
    return 1;
}

int pad_sdl_count(void) { return g_npads; }

int pad_sdl_state(int slot, PortPadState *out)
{
    if (slot < 0 || slot >= g_npads) return 0;
    read_pad(&g_pads[slot], out);
    return 1;
}

void pad_sdl_quit(void)
{
    if (!g_inited) return;
    while (g_npads) close_pad(g_pads[0].id);
    if (g_virt_joy) SDL_CloseJoystick(g_virt_joy);
    if (g_virt_id) SDL_DetachVirtualJoystick(g_virt_id);
    g_virt_joy = 0;
    g_virt_id = 0;
    SDL_Quit();
    g_inited = 0;
}

/* ---- the sweep ----------------------------------------------------------------- */

static int g_sweep_bad;
static void expect(const char *what, long got, long want, int verbose)
{
    if (verbose || got != want)
        printf("    %-30s got %6ld want %6ld %s\n", what, got, want, got == want ? "ok" : "MISMATCH");
    if (got != want) ++g_sweep_bad;
}

int pad_sdl_selftest_sweep(const char *type, int verbose)
{
    g_sweep_bad = 0;
    _putenv_s("SM64DS_PAD_SDL_VIRTUAL", type);
    if (!pad_sdl_init(0)) { printf("sweep %s: SDL init failed\n", type); return 1; }
    PortPadState s;
    char desc[160];
    /* the attached device reaches us as an ADDED event or through the init enumeration */
    for (int i = 0; i < 50 && !pad_sdl_pump(&s, desc, sizeof desc); ++i) Sleep(10);
    if (!g_virt_joy || g_npads != 1) {
        printf("sweep %s: no virtual pad bound (npads %d)\n", type, g_npads);
        pad_sdl_quit();
        return 1;
    }
    printf("sweep %s: %s\n", type, desc);
    pad_sdl_pump(&s, desc, sizeof desc);
    expect("rest buttons", s.buttons, 0, verbose);
    /* every button alone */
    for (int i = 0; i < NBTN; ++i) {
        SDL_SetJoystickVirtualButton(g_virt_joy, BTN_MAP[i].sdl, true);
        SDL_UpdateGamepads();
        pad_sdl_pump(&s, desc, sizeof desc);
        char w[48];
        snprintf(w, sizeof w, "button sdl %d -> 0x%04x", BTN_MAP[i].sdl, BTN_MAP[i].xb);
        expect(w, s.buttons, (long)BTN_MAP[i].xb, verbose);
        SDL_SetJoystickVirtualButton(g_virt_joy, BTN_MAP[i].sdl, false);
    }
    /* the guide button (and the others without an XInput bit) add nothing */
    SDL_SetJoystickVirtualButton(g_virt_joy, SDL_GAMEPAD_BUTTON_GUIDE, true);
    SDL_UpdateGamepads();
    pad_sdl_pump(&s, desc, sizeof desc);
    expect("guide adds no bit", s.buttons, 0, verbose);
    SDL_SetJoystickVirtualButton(g_virt_joy, SDL_GAMEPAD_BUTTON_GUIDE, false);
    /* sticks, both ends and centre; Y is flipped */
    static const struct { int ax; const char *n; short PortPadState::*sf; int flip; } AX[] = {
        {SDL_GAMEPAD_AXIS_LEFTX, "left x", &PortPadState::lx, 0},
        {SDL_GAMEPAD_AXIS_LEFTY, "left y (SDL down+)", &PortPadState::ly, 1},
        {SDL_GAMEPAD_AXIS_RIGHTX, "right x", &PortPadState::rx, 0},
        {SDL_GAMEPAD_AXIS_RIGHTY, "right y (SDL down+)", &PortPadState::ry, 1},
    };
    for (size_t i = 0; i < sizeof AX / sizeof AX[0]; ++i) {
        const int vals[3] = {-32768, 0, 32767};
        for (int k = 0; k < 3; ++k) {
            SDL_SetJoystickVirtualAxis(g_virt_joy, AX[i].ax, (Sint16)vals[k]);
            SDL_UpdateGamepads();
            pad_sdl_pump(&s, desc, sizeof desc);
            long want = vals[k] < -32767 ? -32767 : vals[k];
            if (AX[i].flip) want = -want;
            char w[48];
            snprintf(w, sizeof w, "%s = %d", AX[i].n, vals[k]);
            expect(w, s.*(AX[i].sf), want, verbose);
        }
        SDL_SetJoystickVirtualAxis(g_virt_joy, AX[i].ax, 0);
    }
    for (int t = 0; t < 2; ++t) {
        const int ax = t ? SDL_GAMEPAD_AXIS_RIGHT_TRIGGER : SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        /* a trigger's travel is the axis's whole range: rest = minimum */
        const int vals[4] = {-32768, 0, 32767, -32768};
        const long want[4] = {0, 127, 255, 0};
        for (int k = 0; k < 4; ++k) {
            SDL_SetJoystickVirtualAxis(g_virt_joy, ax, (Sint16)vals[k]);
            SDL_UpdateGamepads();
            pad_sdl_pump(&s, desc, sizeof desc);
            char w[48];
            snprintf(w, sizeof w, "%s trigger = %d", t ? "right" : "left", vals[k]);
            expect(w, t ? s.rt : s.lt, want[k], verbose);
            expect("  other trigger stays 0", t ? s.lt : s.rt, 0, verbose);
        }
        SDL_SetJoystickVirtualAxis(g_virt_joy, ax, -32768);
    }
    printf("sweep %s: %s (%d mismatches)\n", type, g_sweep_bad ? "FAIL" : "PASS", g_sweep_bad);
    pad_sdl_quit();
    return g_sweep_bad;
}
