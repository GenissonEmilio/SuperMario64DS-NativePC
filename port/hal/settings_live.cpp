/* settings_live: see settings_live.h. A thin id-based layer over the live
   setters in host_settings.cpp, plus the rebind capture state machine that
   tests/walk_window.cpp's frame loops feed. */
#include "settings_live.h"
#include "host_settings.h"

#include <stdio.h>
#include <string.h>

static const char *const RUN_NAMES[3] = { "button", "analog", "auto" };
static const char *const CAM_NAMES[3] = { "analog", "freecam", "ds" };

static const SlInfo INFO[SL_COUNT] = {
    /* keys, in HOST_KEY_ order */
    { "KeyUp",       SL_KIND_KEY, 0, 255, 0x57, 0, 1 },
    { "KeyDown",     SL_KIND_KEY, 0, 255, 0x53, 0, 1 },
    { "KeyLeft",     SL_KIND_KEY, 0, 255, 0x41, 0, 1 },
    { "KeyRight",    SL_KIND_KEY, 0, 255, 0x44, 0, 1 },
    { "KeyUpAlt",    SL_KIND_KEY, 0, 255, 0x26, 0, 1 },
    { "KeyDownAlt",  SL_KIND_KEY, 0, 255, 0x28, 0, 1 },
    { "KeyLeftAlt",  SL_KIND_KEY, 0, 255, 0x25, 0, 1 },
    { "KeyRightAlt", SL_KIND_KEY, 0, 255, 0x27, 0, 1 },
    { "KeyJump",     SL_KIND_KEY, 0, 255, 0x20, 0, 1 },
    { "KeyAttack",   SL_KIND_KEY, 0, 255, 0x58, 0, 1 },
    { "KeyCrouch",   SL_KIND_KEY, 0, 255, 0x11, 0, 1 },
    { "KeyRun",      SL_KIND_KEY, 0, 255, 0x10, 0, 1 },
    { "KeyStart",    SL_KIND_KEY, 0, 255, 0x0d, 0, 1 },
    { "KeySelect",   SL_KIND_KEY, 0, 255, 0x08, 0, 1 },
    { "KeyLook",     SL_KIND_KEY, 0, 255, 0x5a, 0, 1 },
    /* pads, in HOST_PAD_ order */
    { "PadJump",     SL_KIND_PAD, 0, HOST_PAD_MASK_MAX, 0x1000, 0, 1 },
    { "PadAttack",   SL_KIND_PAD, 0, HOST_PAD_MASK_MAX, 0x2000, 0, 1 },
    { "PadCrouch",   SL_KIND_PAD, 0, HOST_PAD_MASK_MAX, 0x20000, 0, 1 },
    { "PadRun",      SL_KIND_PAD, 0, HOST_PAD_MASK_MAX, 0x4000, 0, 1 },
    { "PadStart",    SL_KIND_PAD, 0, HOST_PAD_MASK_MAX, 0x0010, 0, 1 },
    { "PadSelect",   SL_KIND_PAD, 0, HOST_PAD_MASK_MAX, 0, 0, 1 },
    { "PadLook",     SL_KIND_PAD, 0, HOST_PAD_MASK_MAX, 0x8000, 0, 1 },
    /* the rest */
    { "RunMode",                SL_KIND_ENUM,    0, 2, 1, RUN_NAMES, 1 },
    { "StickDeadzone",          SL_KIND_PERCENT, 0, 50, 24, 0, 1 },
    { "RunThreshold",           SL_KIND_PERCENT, 30, 100, 89, 0, 1 },
    { "CameraMode",             SL_KIND_ENUM,    0, 2, 2, CAM_NAMES, 1 },
    { "SwapCameraTurnDirection", SL_KIND_BOOL,   0, 1, 0, 0, 1 },
    { "InvertCameraY",          SL_KIND_BOOL,    0, 1, 0, 0, 1 },
    { "CameraSensitivity",      SL_KIND_PERCENT, 10, 400, 100, 0, 1 },
    { "MouseSensitivity",       SL_KIND_PERCENT, 10, 400, 100, 0, 1 },
    { "MouseCapture",           SL_KIND_BOOL,    0, 1, 0, 0, 1 },
    { "Volume",                 SL_KIND_PERCENT, 0, 100, 50, 0, 1 },
    { "MinimapScale",           SL_KIND_PERCENT, 25, 400, 100, 0, 1 },
    { "ImprovedMinimap",        SL_KIND_BOOL,    0, 1, 1, 0, 0 },
};

const SlInfo *sl_info(int id)
{
    return (id >= 0 && id < SL_COUNT) ? &INFO[id] : 0;
}

int sl_get(int id)
{
    if (id >= SL_KEY_BASE && id < SL_PAD_BASE) return host_setting_key(id - SL_KEY_BASE);
    if (id >= SL_PAD_BASE && id < SL_RUN_MODE) return host_setting_pad(id - SL_PAD_BASE);
    switch (id) {
    case SL_RUN_MODE:           return host_setting_run_mode();
    case SL_STICK_DEADZONE:     return host_setting_stick_deadzone_pct();
    case SL_RUN_THRESHOLD:      return host_setting_run_threshold_pct();
    case SL_CAMERA_MODE:        return host_setting_camera_mode();
    case SL_SWAP_CAMERA_TURN:   return host_setting_swap_camera_turn();
    case SL_INVERT_CAMERA_Y:    return host_setting_invert_camera_y();
    case SL_CAMERA_SENSITIVITY: return host_setting_camera_sensitivity();
    case SL_MOUSE_SENSITIVITY:  return host_setting_mouse_sensitivity();
    case SL_MOUSE_CAPTURE:      return host_setting_mouse_capture();
    case SL_VOLUME: {
        const int v = host_setting_volume();
        return v < 0 ? INFO[SL_VOLUME].def : v;
    }
    case SL_MINIMAP_SCALE:      return (int)(host_setting_minimap_scale_value() * 100.0 + 0.5);
    case SL_IMPROVED_MINIMAP:   return host_setting_improved_minimap();
    }
    return 0;
}

int sl_set(int id, int v)
{
    if (id >= SL_KEY_BASE && id < SL_PAD_BASE) return host_setting_set_key(id - SL_KEY_BASE, v);
    if (id >= SL_PAD_BASE && id < SL_RUN_MODE) return host_setting_set_pad(id - SL_PAD_BASE, v);
    switch (id) {
    case SL_RUN_MODE:           return host_setting_set_run_mode(v);
    case SL_STICK_DEADZONE:     return host_setting_set_stick_deadzone(v);
    case SL_RUN_THRESHOLD:      return host_setting_set_run_threshold(v);
    case SL_CAMERA_MODE:        return host_setting_set_camera_mode(v);
    case SL_SWAP_CAMERA_TURN:   return host_setting_set_swap_camera_turn(v);
    case SL_INVERT_CAMERA_Y:    return host_setting_set_invert_camera_y(v);
    case SL_CAMERA_SENSITIVITY: return host_setting_set_camera_sensitivity(v);
    case SL_MOUSE_SENSITIVITY:  return host_setting_set_mouse_sensitivity(v);
    case SL_MOUSE_CAPTURE:      return host_setting_set_mouse_capture(v);
    case SL_VOLUME:             return host_setting_set_volume(v);
    case SL_MINIMAP_SCALE:
        if (v < 25 || v > 400) return 0;
        return host_setting_set_minimap_scale(v / 100.0);
    case SL_IMPROVED_MINIMAP:   return 0;   /* latched with the minimap geometry */
    }
    return 0;
}

int sl_applicable(int id)
{
    switch (id) {
    case SL_CAMERA_SENSITIVITY:
    case SL_INVERT_CAMERA_Y:
    case SL_MOUSE_SENSITIVITY:
        return host_setting_camera_mode() != 2;
    case SL_RUN_THRESHOLD:
        return host_setting_run_mode() == 1;
    }
    return 1;
}

int sl_flush(void) { return host_settings_save_flush(); }

/* ---- the rebind capture ---------------------------------------------------- */
static int g_cap_id = -1;
static int g_cap_either;          /* the F5 run row: key or pad */
static int g_cap_state = SL_CAP_IDLE;
static int g_cap_arm, g_cap_cancel;
static char g_cap_refused[96];

int sl_capture_start(int id)
{
    if (id < SL_KEY_BASE || id >= SL_RUN_MODE) return 0;
    g_cap_id = id;
    g_cap_either = 0;
    g_cap_state = SL_CAP_WAITING;
    g_cap_refused[0] = 0;
    g_cap_arm = 1;
    return 1;
}

int sl_capture_start_run(void)
{
    if (!sl_capture_start(SL_KEY_BASE + HOST_KEY_RUN)) return 0;
    g_cap_either = 1;
    return 1;
}

void sl_capture_cancel(void)
{
    if (g_cap_state != SL_CAP_WAITING) return;
    g_cap_state = SL_CAP_CANCELLED;
    g_cap_cancel = 1;
}

int sl_capture_state(void)
{
    const int s = g_cap_state;
    if (s == SL_CAP_BOUND || s == SL_CAP_CANCELLED) g_cap_state = SL_CAP_IDLE;
    return s;
}
int sl_capture_id(void) { return g_cap_state == SL_CAP_WAITING ? g_cap_id : -1; }
const char *sl_capture_refused(void) { return g_cap_refused; }
int sl_capture_active(void) { return g_cap_state == SL_CAP_WAITING; }

int sl_capture_take_arm(void)
{
    const int a = g_cap_arm;
    g_cap_arm = 0;
    return a;
}
int sl_capture_take_cancel(void)
{
    const int a = g_cap_cancel;
    g_cap_cancel = 0;
    return a;
}

static int key_reserved(int action, int vk, const char **why)
{
    if (vk >= 0x70 && vk <= 0x7b) { *why = "the F keys belong to the window"; return 1; }
    if (vk == 0x1b || vk == 0x09) { *why = "that key belongs to the window"; return 1; }
    if (vk == 0x0d && action != HOST_KEY_START) { *why = "enter drives the menus"; return 1; }
    if (vk >= 0x25 && vk <= 0x28 &&
        !(action >= HOST_KEY_UP_ALT && action <= HOST_KEY_RIGHT_ALT)) {
        *why = "the arrows drive the menus";
        return 1;
    }
    for (int i = 0; i < HOST_KEY_COUNT; ++i)
        if (i != action && host_setting_key(i) == vk) {
            *why = "already used by another control";
            return 1;
        }
    return 0;
}

static int pad_reserved(int action, unsigned bit, const char **why)
{
    /* the d-pad, A, B, BACK and the right stick's click drive the menus and
       the camera mode toggle */
    if (bit & 0x30afu) { *why = "that button drives the menus"; return 1; }
    for (int i = 0; i < HOST_PAD_COUNT; ++i)
        if (i != action && (host_setting_pad(i) & (int)bit)) {
            *why = "already used by another control";
            return 1;
        }
    return 0;
}

int sl_capture_offer_key(int vk)
{
    if (g_cap_state != SL_CAP_WAITING) return 0;
    if (g_cap_id >= SL_PAD_BASE && !g_cap_either) return 0;   /* a pad row waits for a pad */
    if (vk == 0x1b) {
        g_cap_state = SL_CAP_CANCELLED;
        return 1;
    }
    const int action = g_cap_either ? HOST_KEY_RUN : g_cap_id - SL_KEY_BASE;
    const char *why = "";
    if (key_reserved(action, vk, &why)) {
        snprintf(g_cap_refused, sizeof g_cap_refused, "%s", why);
        return 0;
    }
    g_cap_refused[0] = 0;
    host_setting_set_key(action, vk);
    g_cap_state = SL_CAP_BOUND;
    return 1;
}

int sl_capture_offer_pad(unsigned bit)
{
    if (g_cap_state != SL_CAP_WAITING || !bit) return 0;
    if (g_cap_id < SL_PAD_BASE && !g_cap_either) return 0;    /* a key row waits for a key */
    const int action = g_cap_either ? HOST_PAD_RUN : g_cap_id - SL_PAD_BASE;
    const char *why = "";
    if (pad_reserved(action, bit, &why)) {
        snprintf(g_cap_refused, sizeof g_cap_refused, "%s", why);
        return 0;
    }
    g_cap_refused[0] = 0;
    host_setting_set_pad(action, (int)bit);
    g_cap_state = SL_CAP_BOUND;
    return 1;
}
