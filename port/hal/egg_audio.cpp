/* Lane EGG1: THE TITLE'S SMALL-LOGO CLIP (a mouse-only secret on the main page).

   A mouse click on the small logo at the top left of the title's main page plays one short recorded clip
   through the game's own sound output, once. hal/menu_recipe_title.cpp decides that a click landed on the
   logo (mouse only, never the pad or the keyboard, never a DS touch); this file plays the clip.

   THE CLIP is a file next to the exe, sfx/oh_nice_computer.wav (any plain PCM / float WAV the sound mods'
   reader takes: hal/sdat/sd_audio.cpp). It is read and decoded the first time it is asked for. A missing or
   unreadable file is one log line and no sound, never a failure: the port carries no art bytes of its own,
   so the kit ships the file beside the exe and a build without it simply stays quiet.

   THE MIX. The clip is added to the game's mixed stereo block by the mixer's PRE-volume hook
   (sd_mix_set_pre_volume_render, hal/sdat/mixer.cpp): after the DS mixer, before the host master volume. So
   it plays at the game's volume, the Volume setting at 0 (or SM64DS_VOLUME=0) silences it with everything
   else, and the .wav dump records it. It is its own voice, outside the DS channels and the music streams, so
   the menu's own sounds never stop or steal it. It is resampled linearly from the file's rate to the mix
   rate.

   ONE AT A TIME. A request while the clip is sounding (or already asked for) is refused: no overlap, no
   restart. The request flag and the busy flag are atomics; the mixer clears busy when the last frame went
   out. It is local: nothing here touches the network, so online nobody else hears it. */
#include "hal/egg_audio.h"
#include "hal/sdat/sdat.h"
#include "hal/sdat/sd_audio.h"

#include <atomic>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

void sd_mix_set_pre_volume_render(void (*fn)(sd_s16 *dst, int frames, int rate));      /* hal/sdat/mixer.cpp */

namespace {

std::vector<sd_s16> g_pcm;          /* the whole clip, stereo frames at the file's own rate */
int g_rate;                         /* the file's rate */
int g_state;                        /* 0 not tried, 1 loaded, -1 missing or unreadable */
std::atomic<int> g_req{0};          /* a start is waiting for the next render block */
std::atomic<int> g_busy{0};         /* asked for or sounding */

/* mixer side (the render hook's own state) */
int g_live;
double g_pos, g_step;
unsigned long long g_out;
int g_peak;

std::string clip_path()
{
    std::string dir;
#if defined(_WIN32)
    char exe[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        dir.assign(exe, n);
        const size_t s = dir.find_last_of("\\/");
        dir = s == std::string::npos ? std::string() : dir.substr(0, s + 1);
    }
#endif
    return dir + "sfx/oh_nice_computer.wav";
}

void load()
{
    if (g_state) return;
    g_state = -1;
    const std::string p = clip_path();
    FILE *f = fopen(p.c_str(), "rb");
    if (!f) { fprintf(stderr, "[egg] clip missing: sfx/oh_nice_computer.wav next to the exe (no sound)\n"); return; }
    std::vector<sd_u8> d;
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz > 0 && sz < 16 * 1024 * 1024) {
        d.resize((size_t)sz);
        if (fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
    }
    fclose(f);
    std::string why;
    SdSource *src = d.empty() ? nullptr : sd_source_open(d.data(), d.size(), false, why);
    if (!src) { fprintf(stderr, "[egg] clip unreadable: %s (no sound)\n", d.empty() ? "empty or too big" : why.c_str()); return; }
    src->loop = false;
    sd_s16 blk[1024 * 2];
    int got;
    while ((got = src->read(blk, 1024)) > 0) g_pcm.insert(g_pcm.end(), blk, blk + got * 2);
    g_rate = src->info.rate;
    delete src;
    if (g_pcm.size() < 4 || g_rate <= 0) { g_pcm.clear(); fprintf(stderr, "[egg] clip holds no audio (no sound)\n"); return; }
    g_state = 1;
    fprintf(stderr, "[egg] clip loaded: %u frames at %d Hz (%.2f s)\n", (unsigned)(g_pcm.size() / 2), g_rate,
            (double)(g_pcm.size() / 2) / g_rate);
}

/* THE RENDER HOOK. Runs on the thread that renders the mix, once per block. */
void egg_render(sd_s16 *dst, int frames, int rate)
{
    if (!g_live) {
        if (!g_req.exchange(0)) return;
        g_live = 1;
        g_pos = 0.0;
        g_step = rate > 0 ? (double)g_rate / (double)rate : 1.0;
        g_out = 0;
        g_peak = 0;
        fprintf(stderr, "[egg] clip start: mixing at %d Hz (step %.4f)\n", rate, g_step);
    }
    const long long n = (long long)(g_pcm.size() / 2);
    sd_s16 *o = dst;
    for (int k = 0; k < frames; ++k, o += 2) {
        const long long i = (long long)g_pos;
        if (i >= n - 1) {
            g_live = 0;
            fprintf(stderr, "[egg] clip end: %llu frames mixed, peak %d (before the master volume)\n", g_out, g_peak);
            g_busy.store(0);
            return;
        }
        const double fr = g_pos - (double)i;
        for (int c = 0; c < 2; ++c) {
            const double a = g_pcm[(size_t)i * 2 + c], b = g_pcm[(size_t)(i + 1) * 2 + c];
            const int v = (int)(a + (b - a) * fr);
            if (v > g_peak) g_peak = v;
            else if (-v > g_peak) g_peak = -v;
            const int s = o[c] + v;
            o[c] = (sd_s16)(s < -32768 ? -32768 : (s > 32767 ? 32767 : s));
        }
        g_pos += g_step;
        ++g_out;
    }
}

}  // namespace

extern "C" int egg_audio_busy(void)
{
    return g_busy.load() ? 1 : 0;
}

extern "C" int egg_audio_play(void)
{
    if (g_busy.load()) return 0;
    load();
    if (g_state != 1) return -1;
    static int hooked;
    if (!hooked) { hooked = 1; sd_mix_set_pre_volume_render(egg_render); }
    g_busy.store(1);
    g_req.store(1);
    return 1;
}

/* The mouse's left button and the cursor in the foreground window's client pixels: 0 up, 1 down (the point is set), -1 down but the point
   could not be read. user32 is resolved by name at the first call, after the DS regions are reserved (walk_window's rule for user32). The
   caller (hal/menu_recipe_title.cpp) checks that the foreground window is this one. */
extern "C" int egg_mouse_read(int *cx, int *cy)
{
#if defined(_WIN32)
    typedef SHORT (WINAPI *KeyFn)(int);
    typedef BOOL (WINAPI *PosFn)(LPPOINT);
    typedef HWND (WINAPI *FgFn)(void);
    typedef BOOL (WINAPI *StcFn)(HWND, LPPOINT);
    static KeyFn key;
    static PosFn pos;
    static FgFn fg;
    static StcFn stc;
    static int bound;
    if (!bound) {
        bound = 1;
        if (HMODULE u = GetModuleHandleA("user32.dll")) {
            key = (KeyFn)GetProcAddress(u, "GetAsyncKeyState");
            pos = (PosFn)GetProcAddress(u, "GetCursorPos");
            fg = (FgFn)GetProcAddress(u, "GetForegroundWindow");
            stc = (StcFn)GetProcAddress(u, "ScreenToClient");
        }
    }
    if (!key || !(key(VK_LBUTTON) & 0x8000)) return 0;
    POINT p;
    HWND h = fg ? fg() : nullptr;
    if (!pos || !stc || !h || !pos(&p) || !stc(h, &p)) return -1;
    *cx = (int)p.x;
    *cy = (int)p.y;
    return 1;
#else
    (void)cx; (void)cy;
    return 0;
#endif
}
