/* THE PLAYER PERFORMANCE LOG. See hal/perf_log.h for what the file is and
 * what the three switches do; this file is how it is written.
 *
 * TWO RULES SHAPE EVERY LINE OF CODE BELOW.
 *
 * 1. NO NEW IMPORT. This exe's import table is KERNEL32 and nothing else,
 *    and it is checked by name count and by hash on every release, because a
 *    bare import name is itself a signal a stranger's antivirus weighs (see
 *    port/release_hardening.txt, and the d3d11 load at hal/gpu_device.cpp's
 *    device acquire). So: the processor's name comes from the CPUID
 *    instruction, which is not an API call at all; the graphics card's from
 *    dxgi.dll loaded with LoadLibraryA exactly as d3d11.dll already is; the
 *    operating system's build from ntdll, which is resident in every process
 *    before main and is reached with GetModuleHandleA; the display's mode
 *    from user32, which the window code already loads the same way. Even the
 *    two kernel32 calls this file wants that the exe does not already import
 *    -- GlobalMemoryStatusEx and GetSystemPowerStatus -- are taken with
 *    GetProcAddress off the resident kernel32, for the same reason the
 *    working-set query at tests/walk_window.cpp's winapi_load is: an import
 *    is a name in the table, and the table is not allowed to move.
 *
 * 2. NOTHING MAY MOVE THE PICTURE OR THE CLOCK. The per-frame path is adds
 *    into fixed-size accumulators. The work that costs something -- the
 *    behaviour-list walk, the working-set query, the client rect, the card's
 *    totals -- is paid once per five-second bucket, and the caller is told
 *    when to pay it (port_perf_log_due) rather than paying it every frame.
 *    The one write is a buffered fprintf of about 300 bytes every five
 *    seconds, unbuffered so a crash keeps every line already written.
 */
/* fopen/getenv/snprintf, the CRT spellings this tree already uses (see the
   same define at the head of hal/pad_backend.cpp) */
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <dxgi.h>
#include <intrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal/perf_log.h"
#include "hal/host_settings.h"

/* The exe's own version, generated at configure time from the ProductVersion
 * string in res/walk_window.rc (port/CMakeLists.txt, next to the block that
 * attaches that resource) so there is exactly one place a release bumps it.
 * Empty when the game genuinely cannot know, which the contract allows. */
#ifndef PORT_GAME_VERSION
#define PORT_GAME_VERSION ""
#endif

/* hal/level_boot.cpp: the star-select time since the last call, in ms (then
   cleared). See THE STAR SELECT below. */
extern "C" double port_lvlperf_starsel_take(void);

namespace {

/* ---- the clock ---------------------------------------------------------
   The same QueryPerformanceCounter-to-milliseconds the overlay's own
   ovl_now_ms uses, spelled here because that one is file-static to the
   window harness. Same counter, same frequency, same answer. */
double perf_now_ms(void)
{
    static LARGE_INTEGER qpf;
    LARGE_INTEGER n;
    if (!qpf.QuadPart) QueryPerformanceFrequency(&qpf);
    QueryPerformanceCounter(&n);
    return n.QuadPart * 1000.0 / qpf.QuadPart;
}

FILE  *g_fp;
char   g_session[80];
int    g_header_written;
int    g_capped;
int    g_ended;
double g_t0_ms;              /* the header's own instant: t is measured here */
double g_next_t_s;           /* the nominal 5.0, 10.0, ... boundary */
double g_cap_s = 4.0 * 3600.0;

/* the bucket. THE RING holds frame spans for the percentile only; the mean
   and the maximum are running, so they are over every frame in the bucket
   whatever the frame rate. A bucket with more frames than the ring keeps its
   most recent RING_N of them for the percentile, which at any plausible
   frame rate is the whole bucket and at an implausible one is still a fair
   sample of it. */
enum { RING_N = 2048 };
double g_ring[RING_N];
int    g_ring_n;
int    g_frames;
double g_sum_frame, g_max_frame;
double g_sum_in, g_sum_cam, g_sum_sub, g_sum_ras, g_sum_blit;
double g_sum_fps, g_sum_tick, g_sum_cpu;
double g_sum_gms, g_sum_gpct, g_sum_gread;
int    g_bucket_gpu_on;

/* ---- THE STAR SELECT IS NOT A SLOW FRAME (run mp1, lane PROBES1) -------
   A course entered from the castle runs the star select INSIDE one level
   frame (hal/level_change.cpp's scene interlude: the whole host frame loops
   there, paced, until the player has chosen). On the frame clock that is one
   frame of 1.4 to 5.9 s, and the card's and the tick counter's half-second
   windows that straddle it divide their totals by the seconds a person spent
   choosing. Every course entry therefore looked like a hitch in a report, with
   a tick rate of about 26 and a card load of 45 to 112 percent, and none of
   it was the game working. Three things below keep it out:
     - the interlude's own time is subtracted from the frame that held it
       (g_adj_*; the pump can read one frame's span more than once before the
       next frame closes, so the adjusted value is remembered by the raw one);
     - the rate-derived fields (fps, tick rate, cpu, the card's three) skip the
       frames until the rate windows have turned over (g_taint_until_ms), and
       are averaged over the frames that did count (g_nrate / g_ntick), so a
       bucket that holds a course entry still says what the game ran at;
     - the milliseconds taken out go on the sample as "starselMs" (written only
       when non-zero, so every other line is byte for byte what it was).
   The tick rate is also not allowed to go negative: the ROM frame counter is
   rebased when the opening hands over to the first level, the half-second
   window that straddles it divides a negative step, and the report read -10
   to -160 ticks a second at the start of every adventure. A negative window
   is skipped the same way. */
double g_starsel_ms;            /* taken out of this bucket */
double g_pending_excl;          /* handed over, not yet taken off a frame */
double g_taint_until_ms;        /* perf_now_ms() the rate windows are clean from */
double g_adj_raw = -1.0;        /* the raw frame span that was adjusted ... */
double g_adj_val;               /* ... and what it was adjusted to */
int    g_nrate, g_ntick;        /* frames that counted for the rate fields */
double g_last_fps, g_last_tick, g_last_cpu;
double g_last_gms, g_last_gpct, g_last_gread;

/* the snapshot fields, kept from the last frame that filled them */
int      g_level;
int      g_tris, g_actors;
unsigned g_mem_kb;
int      g_win_w = -1, g_win_h = -1;
unsigned long long g_drop_base;
int                g_drop_seeded;
unsigned long long g_bucket_dropped;

/* ---- THE BOOT TIME (run perf2, lane AUDIOBOOT) ------------------------
   How long the player waited, measured from the process's own creation time
   so the loader and the CRT start-up count too. Two figures: bootMs on the
   header line (launch to the first frame handed in here, which for a player
   is the title's first picture) and levelBootMs on a one-off "boot" line
   (launch to the first frame of a course). A box whose audio device took
   25 s to open is how this was found; the report had no way to show it.

   THE FIRST LEVEL FRAME needs the level field, which the caller fills only on
   frames port_perf_log_due() asks for. So until a course frame has been
   seen, due() says yes on every frame, and g_due_real remembers whether the
   bucket actually asked: the snapshot fields are taken only on those frames,
   exactly as before, so every sample line is what it was. The extra cost is
   the snapshot reads on title and menu frames until the first course frame,
   and nothing after it. Both clocks come off the resident kernel32 by
   GetProcAddress, rule 1 above. */
double g_boot_ms = -1.0;
double g_level_boot_ms = -1.0;
int    g_level_boot_level;
int    g_poll_level = 1;
int    g_due_real;

typedef BOOL (WINAPI *GetProcessTimes_t)(HANDLE, LPFILETIME, LPFILETIME,
                                         LPFILETIME, LPFILETIME);
typedef VOID (WINAPI *GetSystemTimeFt_t)(LPFILETIME);

double ms_since_launch(void)
{
    static GetProcessTimes_t gpt;
    static GetSystemTimeFt_t now;
    static int looked;
    if (!looked) {
        looked = 1;
        HMODULE k = GetModuleHandleA("kernel32.dll");
        if (k) {
            gpt = (GetProcessTimes_t)GetProcAddress(k, "GetProcessTimes");
            now = (GetSystemTimeFt_t)GetProcAddress(k,
                                          "GetSystemTimePreciseAsFileTime");
            if (!now)
                now = (GetSystemTimeFt_t)GetProcAddress(k,
                                          "GetSystemTimeAsFileTime");
        }
    }
    if (!gpt || !now) return -1.0;
    FILETIME c, e, kt, u, n;
    if (!gpt(GetCurrentProcess(), &c, &e, &kt, &u)) return -1.0;
    now(&n);
    const unsigned long long c64 =
        ((unsigned long long)c.dwHighDateTime << 32) | c.dwLowDateTime;
    const unsigned long long n64 =
        ((unsigned long long)n.dwHighDateTime << 32) | n.dwLowDateTime;
    return n64 > c64 ? (double)(n64 - c64) / 10000.0 : 0.0;
}

/* ---- ASCII JSON, by hand ----------------------------------------------
   No library, and no string from this process's environment or filesystem
   ever reaches it. The only free text is hardware model names, which are
   filtered to printable ASCII here so a card whose name carries a trademark
   sign cannot make the file non-ASCII. */
void json_str(FILE *f, const char *s)
{
    fputc('"', f);
    for (; s && *s; ++s) {
        const unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { fputc('\\', f); fputc(c, f); }
        else if (c >= 0x20 && c < 0x7f) fputc(c, f);
        else fputc('?', f);
    }
    fputc('"', f);
}

void key_str(FILE *f, const char *k, const char *v)
{
    fprintf(f, ",\"%s\":", k);
    json_str(f, v);
}

/* every millisecond, rate and percentage figure carries one decimal, which is
   the resolution the report is read at and keeps a line about 300 bytes */
void key_f1(FILE *f, const char *k, double v)
{
    if (!(v > -1e12 && v < 1e12)) v = 0.0;   /* a NaN never reaches the file */
    fprintf(f, ",\"%s\":%.1f", k, v);
}

void key_i(FILE *f, const char *k, long long v)
{
    fprintf(f, ",\"%s\":%lld", k, v);
}

/* ---- the machine, asked once ------------------------------------------ */

struct HwInfo {
    char cpu_name[64];
    int  cpu_cores;
    int  ram_mb;
    char gpu_name[128];
    int  gpu_vram_mb;
    unsigned gpu_vendor, gpu_device;
    char gpu_driver[40];
    char os_version[32];
    int  on_battery;
    int  disp_w, disp_h, disp_hz;
    double query_ms;
};

void hw_cpu(HwInfo &h)
{
    int r[4] = {0, 0, 0, 0};
    __cpuid(r, 0x80000000);
    if ((unsigned)r[0] >= 0x80000004u) {
        char buf[49];
        for (int i = 0; i < 3; ++i) {
            __cpuid(r, 0x80000002 + i);
            memcpy(buf + i * 16, r, 16);
        }
        buf[48] = 0;
        /* the string is space-padded on most parts; trim both ends */
        const char *b = buf;
        while (*b == ' ') ++b;
        size_t n = strlen(b);
        while (n && (b[n - 1] == ' ' || b[n - 1] == '\t')) --n;
        if (n >= sizeof h.cpu_name) n = sizeof h.cpu_name - 1;
        memcpy(h.cpu_name, b, n);
        h.cpu_name[n] = 0;
    }
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    h.cpu_cores = si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 0;
}

typedef BOOL(WINAPI *GlobalMemoryStatusEx_t)(MEMORYSTATUSEX *);
typedef BOOL(WINAPI *GetSystemPowerStatus_t)(SYSTEM_POWER_STATUS *);
typedef LONG(WINAPI *RtlGetVersion_t)(void *);
typedef BOOL(WINAPI *EnumDisplaySettingsA_t)(LPCSTR, DWORD, DEVMODEA *);
typedef HRESULT(WINAPI *CreateDXGIFactory1_t)(REFIID, void **);

/* ntdll's own version record, spelled here rather than including winternl.h,
   so the dynamic lookup is this file's only dependency on it. */
struct PortOsVersion {
    ULONG size, major, minor, build, platform;
    WCHAR csd[128];
};

void hw_kernel32(HwInfo &h)
{
    HMODULE k = GetModuleHandleA("kernel32.dll");
    if (!k) return;
    GlobalMemoryStatusEx_t gms =
        (GlobalMemoryStatusEx_t)GetProcAddress(k, "GlobalMemoryStatusEx");
    if (gms) {
        MEMORYSTATUSEX ms;
        memset(&ms, 0, sizeof ms);
        ms.dwLength = sizeof ms;
        if (gms(&ms))
            h.ram_mb = (int)(ms.ullTotalPhys / (1024ull * 1024ull));
    }
    GetSystemPowerStatus_t ps =
        (GetSystemPowerStatus_t)GetProcAddress(k, "GetSystemPowerStatus");
    if (ps) {
        SYSTEM_POWER_STATUS s;
        memset(&s, 0, sizeof s);
        if (ps(&s)) {
            if (s.ACLineStatus == 0) h.on_battery = 1;
            else if (s.ACLineStatus == 1) h.on_battery = 0;
        }
    }
}

void hw_os(HwInfo &h)
{
    /* ntdll is mapped into every process by the loader before this exe's own
       entry point runs, so this is a handle lookup and not a load. */
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    if (!nt) return;
    RtlGetVersion_t rgv = (RtlGetVersion_t)GetProcAddress(nt, "RtlGetVersion");
    if (!rgv) return;
    PortOsVersion v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    if (rgv(&v) == 0)
        snprintf(h.os_version, sizeof h.os_version - 1, "%lu.%lu.%lu",
                  (unsigned long)v.major, (unsigned long)v.minor,
                  (unsigned long)v.build);
}

void hw_display(HwInfo &h)
{
    /* the window code loads user32 with LoadLibraryA after io_init and keeps
       the handle privately; by the time this runs the module is resident, so
       a handle lookup finds it and the LoadLibraryA is the fallback for a run
       that somehow has no window yet. Nothing here is created or changed: one
       read of the primary display's current mode. */
    HMODULE u = GetModuleHandleA("user32.dll");
    if (!u) u = LoadLibraryA("user32.dll");
    if (!u) return;
    EnumDisplaySettingsA_t eds =
        (EnumDisplaySettingsA_t)GetProcAddress(u, "EnumDisplaySettingsA");
    if (!eds) return;
    DEVMODEA dm;
    memset(&dm, 0, sizeof dm);
    dm.dmSize = sizeof dm;
    if (eds(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
        h.disp_w = (int)dm.dmPelsWidth;
        h.disp_h = (int)dm.dmPelsHeight;
        h.disp_hz = (int)dm.dmDisplayFrequency;
    }
}

void hw_gpu(HwInfo &h)
{
    /* THE SAME RULE THE RENDERER KEEPS. dxgi.dll is loaded by name and its
       one entry point taken by name, so no dxgi or dxguid lib is linked and
       the import table does not move. Every failure below leaves the fields
       at "" and 0 and the header still writes: a machine with no DXGI is
       exactly the machine whose report is worth having. */
    HMODULE dx = GetModuleHandleA("dxgi.dll");
    if (!dx) dx = LoadLibraryA("dxgi.dll");
    if (!dx) return;
    CreateDXGIFactory1_t mk =
        (CreateDXGIFactory1_t)GetProcAddress(dx, "CreateDXGIFactory1");
    if (!mk) return;
    IDXGIFactory1 *fac = 0;
    if (FAILED(mk(__uuidof(IDXGIFactory1), (void **)&fac)) || !fac) return;
    IDXGIAdapter1 *ad = 0;
    if (SUCCEEDED(fac->EnumAdapters1(0, &ad)) && ad) {
        DXGI_ADAPTER_DESC1 d;
        memset(&d, 0, sizeof d);
        if (SUCCEEDED(ad->GetDesc1(&d))) {
            unsigned i = 0;
            for (; i + 1 < sizeof h.gpu_name && d.Description[i]; ++i)
                h.gpu_name[i] =
                    (char)(d.Description[i] < 128 ? d.Description[i] : '?');
            h.gpu_name[i] = 0;
            /* DedicatedVideoMemory is a SIZE_T and this is a 32-bit
               process, so a card with more memory than a pointer can count
               reports a clipped figure (a 12 GB card answers 3072 MB here).
               It is a floor, not a total, and the triage table reads it as
               one; the card's NAME is the field that identifies it. */
            h.gpu_vram_mb =
                (int)(d.DedicatedVideoMemory / (1024ull * 1024ull));
            h.gpu_vendor = d.VendorId;
            h.gpu_device = d.DeviceId;
        }
        /* the user-mode driver's version, which is what a "update your
           driver" answer needs and what the adapter will tell anyone who
           asks whether it supports being a Direct3D device. */
        LARGE_INTEGER umd;
        umd.QuadPart = 0;
        if (SUCCEEDED(ad->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd)) &&
            umd.QuadPart)
            snprintf(h.gpu_driver, sizeof h.gpu_driver - 1, "%u.%u.%u.%u",
                      (unsigned)HIWORD(umd.HighPart),
                      (unsigned)LOWORD(umd.HighPart),
                      (unsigned)HIWORD(umd.LowPart),
                      (unsigned)LOWORD(umd.LowPart));
        ad->Release();
    }
    fac->Release();
    /* the module handle is left alone deliberately: releasing the factory and
       the adapter gives back everything this query holds, and unloading dxgi
       out from under a driver that has started its own threads is a known way
       to crash on the way past. The renderer would load it again anyway. */
}

void hw_gather(HwInfo &h)
{
    const double t0 = perf_now_ms();
    memset(&h, 0, sizeof h);
    h.on_battery = -1;
    h.disp_w = h.disp_h = h.disp_hz = -1;
    hw_cpu(h);
    hw_kernel32(h);
    hw_os(h);
    hw_display(h);
    hw_gpu(h);
    h.query_ms = perf_now_ms() - t0;
}

/* ---- the header line ---------------------------------------------------
   WRITTEN ON THE FIRST FRAME, NOT AT OPEN, and that is the one place this
   module departs from the obvious reading of the contract. The file is
   created and its name claimed at the flight recorder's own open point, as
   the contract says; the machine QUERY waits for the first frame.

   Why: the recorder opens before ntr::io_init, and io_init reserves the DS
   hardware windows at their fixed addresses (0x02000000 upward). Loading
   dxgi and, behind it, a vendor's user-mode driver before that reservation
   puts somebody else's mappings in the address space first -- which is
   exactly the failure the window harness already avoids by loading user32
   late (tests/walk_window.cpp, the comment over struct WinApi). A telemetry
   line is not worth a boot. By the first frame io_init has its addresses,
   the window exists, so its client size is real, and nothing has been
   written to the file yet, so the header is still line 1. */
void perf_write_header(int win_w, int win_h)
{
    HwInfo h;
    hw_gather(h);
    fprintf(g_fp, "{\"type\":\"header\"");
    key_str(g_fp, "gameVersion", PORT_GAME_VERSION);
    key_str(g_fp, "sessionId", g_session);
    key_str(g_fp, "cpuName", h.cpu_name);
    key_i(g_fp, "cpuCores", h.cpu_cores);
    key_i(g_fp, "ramMb", h.ram_mb);
    key_str(g_fp, "gpuName", h.gpu_name);
    key_i(g_fp, "gpuVramMb", h.gpu_vram_mb);
    key_i(g_fp, "gpuVendorId", (long long)h.gpu_vendor);
    key_i(g_fp, "gpuDeviceId", (long long)h.gpu_device);
    key_str(g_fp, "gpuDriver", h.gpu_driver);
    key_str(g_fp, "osVersion", h.os_version);
    key_i(g_fp, "onBattery", h.on_battery);
    key_i(g_fp, "displayW", h.disp_w);
    key_i(g_fp, "displayH", h.disp_h);
    key_i(g_fp, "displayHz", h.disp_hz);
    key_i(g_fp, "windowW", win_w);
    key_i(g_fp, "windowH", win_h);
    key_i(g_fp, "resolutionScale", host_setting_render_scale());
    fprintf(g_fp, ",\"aspect\":%.7f", host_setting_aspect());
    key_i(g_fp, "renderer", host_setting_renderer());
    key_i(g_fp, "textureFilter", host_setting_texture_filter());
    key_i(g_fp, "antiAliasing", host_setting_anti_aliasing());
    key_i(g_fp, "smoothModels", host_setting_smooth_models());
    key_i(g_fp, "presentBackend", host_setting_present_backend());
    key_i(g_fp, "vsync", host_setting_vsync());
    key_i(g_fp, "frameRateTarget", host_setting_frame_rate());
    key_f1(g_fp, "bootMs", g_boot_ms);
    fprintf(g_fp, "}\n");
    /* one line on the flight recorder, so a play log says the report exists
       and what the machine query cost, and nothing else all session */
    fprintf(stderr, "[perflog] header written, hardware query %.1f ms\n",
            h.query_ms);
}

void bucket_reset(void)
{
    g_frames = 0;
    g_ring_n = 0;
    g_sum_frame = g_max_frame = 0.0;
    g_sum_in = g_sum_cam = g_sum_sub = g_sum_ras = g_sum_blit = 0.0;
    g_sum_fps = g_sum_tick = g_sum_cpu = 0.0;
    g_sum_gms = g_sum_gpct = g_sum_gread = 0.0;
    g_bucket_gpu_on = 0;
    g_starsel_ms = 0.0;
    g_nrate = g_ntick = 0;
}

int cmp_double(const void *a, const void *b)
{
    const double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

double bucket_p95(void)
{
    if (g_ring_n <= 0) return 0.0;
    static double tmp[RING_N];
    memcpy(tmp, g_ring, (size_t)g_ring_n * sizeof tmp[0]);
    qsort(tmp, (size_t)g_ring_n, sizeof tmp[0], cmp_double);
    int i = (int)(0.95 * (g_ring_n - 1) + 0.5);
    if (i < 0) i = 0;
    if (i >= g_ring_n) i = g_ring_n - 1;
    return tmp[i];
}

void perf_write_sample(double t_s)
{
    const double n = g_frames > 0 ? (double)g_frames : 1.0;
    /* the rate-derived fields average over the frames whose rate windows were
       clean (THE STAR SELECT IS NOT A SLOW FRAME, above); a bucket with none
       repeats the last good figure rather than writing a made-up zero */
    if (g_nrate > 0) {
        g_last_fps = g_sum_fps / g_nrate;
        g_last_cpu = g_sum_cpu / g_nrate;
        g_last_gms = g_sum_gms / g_nrate;
        g_last_gpct = g_sum_gpct / g_nrate;
        g_last_gread = g_sum_gread / g_nrate;
    }
    if (g_ntick > 0) g_last_tick = g_sum_tick / g_ntick;
    fprintf(g_fp, "{\"type\":\"sample\"");
    key_f1(g_fp, "t", t_s);
    key_i(g_fp, "level", g_level);
    key_f1(g_fp, "fps", g_last_fps);
    key_f1(g_fp, "tickRate", g_last_tick);
    key_f1(g_fp, "frameMs", g_sum_frame / n);
    key_f1(g_fp, "frameMsP95", bucket_p95());
    key_f1(g_fp, "frameMsMax", g_max_frame);
    key_f1(g_fp, "inputMs", g_sum_in / n);
    key_f1(g_fp, "cameraMs", g_sum_cam / n);
    key_f1(g_fp, "submitMs", g_sum_sub / n);
    key_f1(g_fp, "rasterMs", g_sum_ras / n);
    key_f1(g_fp, "blitMs", g_sum_blit / n);
    key_f1(g_fp, "cpuPct", g_last_cpu);
    key_i(g_fp, "gpuOn", g_bucket_gpu_on);
    key_f1(g_fp, "gpuMsPerTick", g_last_gms);
    key_f1(g_fp, "gpuPct", g_last_gpct);
    key_f1(g_fp, "gpuReadMs", g_last_gread);
    key_i(g_fp, "gpuDropped", (long long)g_bucket_dropped);
    key_i(g_fp, "tris", g_tris);
    key_i(g_fp, "actors", g_actors);
    key_i(g_fp, "memKb", g_mem_kb);
    key_i(g_fp, "windowW", g_win_w);
    key_i(g_fp, "windowH", g_win_h);
    /* only on a bucket that held a star select, so every other line is what
       it always was */
    if (g_starsel_ms > 0.0) key_f1(g_fp, "starselMs", g_starsel_ms);
    fprintf(g_fp, "}\n");
}

/* the one-off boot line: the first course frame, once a session */
void perf_write_boot(void)
{
    fprintf(g_fp, "{\"type\":\"boot\"");
    key_f1(g_fp, "t", g_header_written ? (perf_now_ms() - g_t0_ms) / 1000.0
                                       : 0.0);
    key_i(g_fp, "level", g_level_boot_level);
    key_f1(g_fp, "bootMs", g_boot_ms);
    key_f1(g_fp, "levelBootMs", g_level_boot_ms);
    fprintf(g_fp, "}\n");
}

/* The id the file is named with. The recorder's own name is
   playlog/play_<id>.log, so <id> is what sits between "play_" and ".log";
   taking it from there rather than re-deriving it from the clock is what
   makes the pair exact even when the recorder had to walk a collision
   suffix. */
void session_from_playlog(const char *path)
{
    const char *base = path;
    for (const char *p = path; *p; ++p)
        if (*p == '/' || *p == '\\') base = p + 1;
    if (strncmp(base, "play_", 5) == 0) base += 5;
    size_t n = strlen(base);
    if (n > 4 && strcmp(base + n - 4, ".log") == 0) n -= 4;
    if (n >= sizeof g_session) n = sizeof g_session - 1;
    memcpy(g_session, base, n);
    g_session[n] = 0;
}

/* A run with no flight recorder has no id to borrow, so it makes one the same
   way the recorder makes its own: local time to the second, and the process
   id when that name is already taken. Only reachable under
   SM64DS_PERFLOG_FORCE, i.e. never in a player's run. */
void session_invent(void)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(g_session, sizeof g_session - 1, "%04u%02u%02u_%02u%02u%02u",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    char probe[160];
    snprintf(probe, sizeof probe - 1, "playlog/perf-%s.jsonl", g_session);
    HANDLE claim = CreateFileA(probe, GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (claim != INVALID_HANDLE_VALUE) { CloseHandle(claim); return; }
    if (GetLastError() != ERROR_FILE_EXISTS) return;
    snprintf(g_session, sizeof g_session - 1, "%04u%02u%02u_%02u%02u%02u_%lu",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
              (unsigned long)GetCurrentProcessId());
}

void perf_close_atexit(void) { port_perf_log_close(); }

}  /* namespace */

extern "C" void port_perf_log_open(const char *playlog_path)
{
    if (g_fp) return;
    if (getenv("SM64DS_NO_PERFLOG")) return;
    if (!playlog_path || !*playlog_path || strcmp(playlog_path, "off") == 0) {
        /* no flight recorder this run: only a test asks for a file anyway */
        if (!getenv("SM64DS_PERFLOG_FORCE")) return;
        CreateDirectoryA("playlog", NULL);
        session_invent();
    } else {
        CreateDirectoryA("playlog", NULL);
        session_from_playlog(playlog_path);
    }
    if (!g_session[0]) return;
    {
        char name[160];
        snprintf(name, sizeof name - 1, "playlog/perf-%s.jsonl", g_session);
        g_fp = fopen(name, "wb");
        if (!g_fp) return;
        /* unbuffered: every line already written survives a hard crash, which
           is the case the file exists for */
        setvbuf(g_fp, NULL, _IONBF, 0);
    }
    {
        const char *cap = getenv("SM64DS_PERFLOG_CAP_SECONDS");
        if (cap) {
            const double v = atof(cap);
            if (v > 0.0) g_cap_s = v;
        }
    }
    bucket_reset();
    atexit(perf_close_atexit);
}

extern "C" int port_perf_log_on(void)
{
    return (g_fp && !g_capped && !g_ended) ? 1 : 0;
}

extern "C" int port_perf_log_due(void)
{
    if (!port_perf_log_on()) return 0;
    if (!g_header_written)                  /* the first frame writes line 1 */
        g_due_real = 1;
    else
        g_due_real = (perf_now_ms() - g_t0_ms) >= g_next_t_s * 1000.0 ? 1 : 0;
    /* until the first course frame, every frame reports its level (the boot
       time banner above); g_due_real keeps the bucket's own snapshot rule */
    return (g_due_real || g_poll_level) ? 1 : 0;
}

extern "C" void port_perf_log_frame(const struct PortPerfFrame *f)
{
    if (!port_perf_log_on() || !f) return;

    if (!g_header_written) g_boot_ms = ms_since_launch();
    int boot_line = 0;
    if (f->snapshot && g_poll_level && f->level >= 0) {
        g_poll_level = 0;
        g_level_boot_level = f->level;
        g_level_boot_ms = g_header_written ? ms_since_launch() : g_boot_ms;
        boot_line = 1;
    }

    if (f->snapshot && g_due_real) {
        g_level = f->level;
        if (f->tris >= 0) g_tris = f->tris;
        if (f->actors >= 0) g_actors = f->actors;
        if (f->mem_kb) g_mem_kb = f->mem_kb;
        if (f->window_w >= 0) g_win_w = f->window_w;
        if (f->window_h >= 0) g_win_h = f->window_h;
        if (!g_drop_seeded) { g_drop_base = f->gpu_dropped_total; g_drop_seeded = 1; }
        g_bucket_dropped = f->gpu_dropped_total >= g_drop_base
                               ? f->gpu_dropped_total - g_drop_base
                               : 0;
        g_drop_base = f->gpu_dropped_total;
    }

    if (!g_header_written) {
        g_header_written = 1;
        perf_write_header(f->window_w, f->window_h);
        g_t0_ms = perf_now_ms();
        g_next_t_s = 5.0;
        bucket_reset();
        if (boot_line) perf_write_boot();   /* the run began in a course */
        return;
    }
    if (boot_line) perf_write_boot();

    /* THE STAR SELECT (see the banner over g_starsel_ms). The interlude's
       milliseconds are handed over by hal/level_boot.cpp the moment it ends;
       they come out of the next frame span that is long enough to hold them,
       and that frame's span stays adjusted if the pump reads it again. */
    double frame_ms = f->frame_ms;
    const double now_ms = perf_now_ms();
    {
        const double ss = port_lvlperf_starsel_take();
        if (ss > 0.0) {
            g_pending_excl += ss;
            g_starsel_ms += ss;
            g_taint_until_ms = now_ms + 650.0;   /* the 0.5 s rate windows */
        }
        if (frame_ms == g_adj_raw) {
            frame_ms = g_adj_val;
        } else if (g_pending_excl > 0.0 && frame_ms >= g_pending_excl) {
            g_adj_raw = frame_ms;
            frame_ms -= g_pending_excl;
            g_adj_val = frame_ms;
            g_pending_excl = 0.0;
        }
    }
    const int rate_ok = now_ms >= g_taint_until_ms;

    ++g_frames;
    g_sum_frame += frame_ms;
    if (frame_ms > g_max_frame) g_max_frame = frame_ms;
    g_ring[g_ring_n < RING_N ? g_ring_n : (g_frames - 1) % RING_N] = frame_ms;
    if (g_ring_n < RING_N) ++g_ring_n;
    g_sum_in += f->input_ms;
    g_sum_cam += f->camera_ms;
    g_sum_sub += f->submit_ms;
    g_sum_ras += f->raster_ms;
    g_sum_blit += f->blit_ms;
    if (rate_ok) {
        ++g_nrate;
        g_sum_fps += f->fps;
        g_sum_cpu += f->cpu_pct;
        g_sum_gms += f->gpu_ms_tick;
        g_sum_gpct += f->gpu_pct;
        g_sum_gread += f->gpu_read_ms;
        if (f->tick_rate >= 0.0) {          /* a rebased ROM counter reads <0 */
            ++g_ntick;
            g_sum_tick += f->tick_rate;
        }
    }
    if (f->gpu_on) g_bucket_gpu_on = 1;

    const double t_s = (perf_now_ms() - g_t0_ms) / 1000.0;
    if (t_s < g_next_t_s) return;

    /* THE BOUNDARY, NOT THE CLOCK. The bucket that closes here is the one
       whose nominal end is g_next_t_s, so a cap of twenty seconds keeps the
       sample AT twenty and stops after it; testing the measured t would drop
       that sample for the few microseconds it overshot by. */
    if (g_next_t_s > g_cap_s) {
        /* THE CAP. Four hours of five-second lines is about 2.9 MB, which is
           already more than a report needs; past it the file stops growing
           and says so, so a machine left running overnight does not turn a
           courtesy into a disk. */
        fprintf(g_fp, "{\"type\":\"cap\"");
        key_f1(g_fp, "t", t_s);
        key_str(g_fp, "reason", "cap");
        fprintf(g_fp, "}\n");
        g_capped = 1;
        return;
    }
    perf_write_sample(t_s);
    g_next_t_s += 5.0;
    bucket_reset();
}

extern "C" void port_perf_log_close(void)
{
    if (!g_fp || g_ended) return;
    g_ended = 1;
    if (g_header_written && !g_capped) {
        fprintf(g_fp, "{\"type\":\"end\"");
        key_f1(g_fp, "t", (perf_now_ms() - g_t0_ms) / 1000.0);
        key_str(g_fp, "reason", "exit");
        fprintf(g_fp, "}\n");
    }
    fclose(g_fp);
    g_fp = 0;
}
