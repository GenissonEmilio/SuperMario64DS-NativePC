/* THE PRESENTER THREAD. See hal/presenter.h for what it is and why.
 *
 * ================= THE DESIGN, AND THE REASONS FOR EACH CHOICE ============
 *
 * 1. WHO DRAWS WHAT. The game thread keeps drawing the finished picture
 *    through hal/gpu_present.cpp's quad (fit, filter, black bars, the card's
 *    composite, the upload for the software renderer): that is the only code
 *    that knows the source, so it stays the only code that draws. It draws
 *    into a render-target texture of this file's ring instead of into the
 *    swap chain's back buffer. The presenter thread does ONE CopyResource
 *    (that texture into the back buffer) and ONE Present per refresh.
 *    Chosen over "the presenter draws the quad too" because that would need
 *    the source texture kept alive and untouched until the presenter is
 *    done with it, and the sources (the upload texture, the card's composite
 *    targets) are rewritten by the very next picture.
 *
 * 2. THE HAND-OVER, AND THE CLOCK OF THE PICTURES (run hunt8, lane
 *    PRESENTER1B). A ring of textures (5 to 12, by picture size) and a queue
 *    of the published ones in the order they were made, guarded by a spin lock
 *    held for a few instructions (never across a GPU call or a wait). Every
 *    picture SmoothMotion makes carries the display moment it is FOR (its
 *    target: the tick's start plus the blend's alpha times the tick's length,
 *    in the game thread's QPC). The game thread may draw the rest of a tick's
 *    pictures at once, ahead of their moments. At each of its turns the
 *    presenter takes the queued picture whose target is closest to the
 *    turn's moment less the display delay D, never one more than half a
 *    refresh early, never one older than what is on show; the rest it passes
 *    over. The first presenter showed the newest picture of each turn, which
 *    threw away the in-between pictures of every burst after a long tick (98
 *    to 116 distinct pictures a second against 131 to 141 on the old path).
 *    A picture with no moment (a menu, a scene, a tick picture drawn at
 *    once) is shown at the next turn and everything older is dropped.
 *    SM64DS_PRESENTER_TIMED=0 puts the newest-wins rule and a ring of three
 *    back.
 *
 *    THE DELAY D. During the 7 to 14 ms the game thread spends in a tick, the
 *    tick's own pictures do not exist yet, so a refresh in that stretch can
 *    only repeat the last picture, and the catch-up after it is a double
 *    step: the 144 Hz picture judders at the tick rate. Showing the pictures
 *    D later than their moments lets those refreshes show the end of the tick
 *    before. D follows the tick work (the second largest of the last 32
 *    ticks, plus half a millisecond) and is capped at one refresh by default
 *    (SM64DS_PRESENTER_DELAY_CAP_MS raises it; SM64DS_PRESENTER_DELAY_MS pins
 *    it). Each step of D is at most 0.1 ms per refresh, so nothing visibly
 *    shifts.
 *
 *    The GPU side is ordered by the one immediate context: the game's draw is
 *    submitted before the publish, the presenter's copy after the take, so the
 *    copy sees the whole picture; a texture is never written while it is
 *    being copied (the keyed mutex between the two devices).
 *
 * 3. THE DEVICE LOCK. The immediate context is not free-threaded, so the
 *    device is put in multithread-protected mode at start. Present is called
 *    by the presenter alone. If Present were to hold the device lock while it
 *    waits for the display it would stall the game thread's card calls; the
 *    measurements in out/PRESENTER1 say whether it does (it does not).
 *
 * 4. THE CLOCK. VSync on: the presenter calls Present(1) back to back, with a
 *    floor of 0.75 refresh between two calls so that a window the compositor
 *    does not pace (covered, minimised, remote) cannot make it spin. VSync
 *    off: a high-resolution waitable timer grid at the refresh rate (or the
 *    FrameRate key when set), then Present(0). DXGI_STATUS_OCCLUDED (covered
 *    or minimised) switches the wait to one refresh per loop.
 *
 * 5. FAILURE. The thread never calls the game's fall-back itself; it stores
 *    the HRESULT and stops, and the game thread's next present sees it,
 *    ends the thread and goes back to GDI as it always has.
 *
 * 6. RESIZE / WINDOW CHANGE / EXIT. The game thread ends the presenter before
 *    ResizeBuffers or releasing the swap chain and starts it again after (a
 *    join is at most one refresh), so no back-buffer reference or texture size
 *    ever outlives its swap chain.
 */
#if defined(_WIN32)

#include <windows.h>
#include <d3d11.h>
#include <d3d11_4.h>
#include <dxgi.h>
#include <intrin.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal/presenter.h"
#include "hal/pcore.h"

#ifndef DXGI_STATUS_OCCLUDED
#define DXGI_STATUS_OCCLUDED ((HRESULT)0x087A0001L)
#endif
#define PRES_STILL_DRAWING ((HRESULT)0x887A000AL)

namespace {

enum { NSMAX = 12 };
const double RING_BUDGET = 96.0 * 1024.0 * 1024.0;    /* bytes of ring textures at most (never fewer than 5 slots) */
const double DEFAULT_DELAY_MS = 0.0;
const long STEP_US = 100;                             /* adaptive D moves at most this much per refresh */

struct Slot {
    ID3D11Texture2D *tex;          /* on the game thread's device: the picture is drawn into it */
    ID3D11RenderTargetView *rtv;
    IDXGIKeyedMutex *km1;          /* its keyed mutex as the game thread's device sees it */
    ID3D11Texture2D *tex2;         /* the same memory opened on the presenter's own device */
    IDXGIKeyedMutex *km2;
    int key;                       /* the key the game thread must acquire it with (0 free, 1 holds an unshown picture) */
    long long gen;                 /* which distinct picture this is */
    long long t_pub;               /* QPC when that picture was complete */
    long long serial;              /* publish ordinal */
    long long target;              /* QPC of the display moment this picture is for (0 = none: show at once) */
    int state;                     /* 0 free, 1 the game thread is drawing it, 2 queued for the presenter */
    float cam[3];                  /* test: the eye this picture was made from */
};

typedef HANDLE (WINAPI *CreateWaitableTimerExW_t)(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);

ID3D11Device *g_dev;            /* the game thread's device and context */
ID3D11DeviceContext *g_ctx;
ID3D11Device *g_dev2;           /* the presenter's own device: it owns the swap chain */
ID3D11DeviceContext *g_ctx2;
ID3D11Texture2D *g_shown;       /* the picture on show, on g_dev2 */
int g_main_held;                /* the game thread holds the keyed mutex of g_slot[g_main_idx] */
HMODULE g_d3d11;
IDXGISwapChain *g_sc;
Slot g_slot[NSMAX];
int g_ns = 3;                  /* ring size in use */
int g_q[NSMAX];                /* queued slots, oldest first */
int g_qn;
volatile long g_ql;            /* spin lock over g_q and the slot states (held for a few instructions) */
long long g_ptarget;           /* the target of the next publish (set by the game thread) */
long long g_last_target;       /* the newest timed target published so far */
volatile long g_delay_us;      /* the display delay D now: refresh k shows the picture made for the moment k minus D */
volatile long g_goal_us;       /* where D is heading (adaptive mode) */
volatile long g_goal_work_us, g_goal_ready_us;   /* its two parts: the tick's work, the first in-between's readiness (MENUPACE1) */
long g_cap_us;                 /* adaptive D never goes above this */
int g_cap_pinned;              /* SM64DS_PRESENTER_DELAY_CAP_MS set: the cap stays (FrameRate live, run hunt8, lane FPSLIVE1) */
int g_fixed_delay;             /* SM64DS_PRESENTER_DELAY_MS set: D is that value and stays */
double g_work[32];             /* the last ticks' own work, milliseconds */
int g_work_n, g_work_at;
int g_timed = -1;              /* 1 unless SM64DS_PRESENTER_TIMED=0 */
int g_qmax;                    /* test: deepest queue seen */
int g_w, g_h;
int g_mode;                  /* 1 Present(1), 0 timer grid */
double g_period_ms = 6.944;
volatile long g_period_us = 6944;   /* the same period for the thread, one atomic word (run hunt8, lane FPSLIVE1) */
long long g_qpf;

int g_main_idx;              /* the slot the game thread is drawing (game thread only) */
volatile long g_stop;
int g_flush = 1;              /* SM64DS_PRESENTER_FLUSH=0 turns the flush off (A/B only) */
long long g_flush_qpc, g_flush_n;
int g_leaked;                 /* a stuck thread still owns the ring: nothing is freed or restarted */
volatile long g_err;
volatile long g_published;   /* publishes so far (any picture) */
volatile long g_pub_w, g_pub_h;
long long g_gen, g_gen_t, g_serial;
HANDLE g_thread;
HANDLE g_timer;
int g_running;

int g_en = -1, g_stats = -1, g_hash = -1;
FILE *g_hash_main, *g_hash_show;

long long qpc()
{
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return c.QuadPart;
}
double ms_since(long long a, long long b) { return (double)(b - a) * 1000.0 / (double)g_qpf; }

/* run hunt8, lane MENUPACE1: SM64DS_SLOT_TRACE=1 also logs what the presenter did with each picture (trace only, inert otherwise): a "[pres]"
   line per publish (its target against now) and per refresh (the moment it stands for, the picture taken, the pictures it dropped). The
   absolute stamp is the QPC clock in ms, the same one walk_window's "[slot]" lines carry as "at". Written at exit. */
static char *g_pt_buf;
static volatile long g_pt_n;
static const long PT_CAP = 16L << 20;
static int g_pt_on = -1;
static void pt_flush(void)
{
    if (g_pt_buf) { const long n = g_pt_n; fwrite(g_pt_buf, 1, (size_t)(n < PT_CAP ? n : PT_CAP), stderr); fflush(stderr); g_pt_n = 0; }
}
static int pt_enabled(void)
{
    if (g_pt_on < 0) {
        const char *e = getenv("SM64DS_SLOT_TRACE");
        g_pt_on = (e && e[0] == '1') ? 1 : 0;
        if (g_pt_on) { g_pt_buf = (char *)malloc((size_t)PT_CAP); atexit(pt_flush); }
    }
    return g_pt_on > 0 && g_pt_buf;
}
static void pt_line(const char *fmt, ...)
{
    if (!pt_enabled()) return;
    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = _vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    const long at = _InterlockedExchangeAdd(&g_pt_n, (long)n);
    if (at + n > PT_CAP) return;
    memcpy(g_pt_buf + at, tmp, (size_t)n);
}
static double pt_abs(long long q) { return (double)q * 1000.0 / (double)g_qpf; }

/* ---- statistics, shared with the old path ------------------------------- */

struct Ev { long long t; float age; unsigned char nw; float cam[3]; long long serial; float tg; };
float g_pcam[3];                 /* test: set by the game thread for the next picture */
Ev *g_ev;
int g_ev_n, g_ev_cap = 1 << 18;
int g_ev_printed;
long long g_ev_t0, g_last_print;
FILE *g_dump;
int g_stat_exit_set;

int cmpd(const void *a, const void *b)
{
    const double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

void stat_summary(int i0, int i1, const char *tag)
{
    if (i1 - i0 < 3) return;
    const int n = i1 - i0;
    double *iv = (double *)malloc(sizeof(double) * n * 2);
    if (!iv) return;
    double *ag = iv + n;
    int ni = 0, na = 0, nnew = 0, within = 0;
    for (int i = i0 + 1; i < i1; ++i) {
        const double d = ms_since(g_ev[i - 1].t, g_ev[i].t);
        iv[ni++] = d;
        if (d >= g_period_ms - 1.0 && d <= g_period_ms + 1.0) ++within;
    }
    for (int i = i0; i < i1; ++i)
        if (g_ev[i].nw) { ++nnew; ag[na++] = g_ev[i].age; }
    qsort(iv, ni, sizeof(double), cmpd);
    if (na) qsort(ag, na, sizeof(double), cmpd);
    const double secs = ms_since(g_ev[i0].t, g_ev[i1 - 1].t) / 1000.0;
    fprintf(stderr, "[presenter] %s %.1fs: presents %d (%.1f/s) new pictures %d "
            "(%.1f/s) interval ms p50 %.2f p95 %.2f within %.2f+-1: %.1f%% "
            "age ms p50 %.2f p95 %.2f\n", tag, secs, n,
            secs > 0 ? n / secs : 0.0, nnew, secs > 0 ? nnew / secs : 0.0,
            iv[ni / 2], iv[(ni * 95) / 100], g_period_ms,
            ni ? 100.0 * within / ni : 0.0, na ? ag[na / 2] : 0.0,
            na ? ag[(na * 95) / 100] : 0.0);
    free(iv);
}

void stat_flush()
{
    if (!g_ev) return;
    if (g_ev_n - g_ev_printed > 3) {
        stat_summary(g_ev_printed, g_ev_n, "last");
        g_ev_printed = g_ev_n;
    }
    if (g_ev_n > 3) stat_summary(0, g_ev_n, "run total");
    if (g_dump) fflush(g_dump);
}

void stat_exit() { stat_flush(); }

}  // namespace

extern "C" int presenter_enabled(void)
{
    if (g_en < 0) {
        const char *e = getenv("SM64DS_PRESENTER");
        g_en = (e && e[0] == '0' && !e[1]) ? 0 : 1;
    }
    return g_en;
}

extern "C" int presenter_stats_on(void)
{
    if (g_stats < 0) {
        const char *e = getenv("SM64DS_PRESENTER_LOG");
        g_stats = (e && atoi(e) > 0) ? 1 : 0;
        if (g_stats) {
            LARGE_INTEGER f;
            QueryPerformanceFrequency(&f);
            g_qpf = f.QuadPart;
            g_ev = (Ev *)calloc((size_t)g_ev_cap, sizeof(Ev));
            if (!g_ev) g_stats = 0;
            const char *d = getenv("SM64DS_PRESENTER_DUMP");
            if (g_stats && d && *d) g_dump = fopen(d, "w");
            if (g_stats && !g_stat_exit_set) {
                g_stat_exit_set = 1;
                atexit(stat_exit);
            }
        }
    }
    return g_stats;
}

extern "C" void presenter_note_cam(const float eye[3])
{
    g_pcam[0] = eye[0];
    g_pcam[1] = eye[1];
    g_pcam[2] = eye[2];
}

static void stat_note_ex(long long t_pub, int new_pic, const float *cam, long long serial, long long target);

extern "C" void presenter_stat_note(long long t_pub, int new_pic)
{
    stat_note_ex(t_pub, new_pic, g_pcam, 0, 0);
}

static void stat_note_ex(long long t_pub, int new_pic, const float *cam, long long serial, long long target)
{
    if (!presenter_stats_on()) return;
    const long long t = qpc();
    if (g_ev_n >= g_ev_cap) return;
    Ev &e = g_ev[g_ev_n++];
    e.t = t;
    e.nw = (unsigned char)(new_pic ? 1 : 0);
    e.age = (float)(t_pub ? ms_since(t_pub, t) : 0.0);
    e.cam[0] = cam[0];
    e.cam[1] = cam[1];
    e.cam[2] = cam[2];
    e.serial = serial;
    e.tg = (float)(target ? ms_since(target, t) : 0.0);
    if (!g_ev_t0) g_ev_t0 = t;
    if (g_dump)
        fprintf(g_dump, "%.3f %d %.3f %.3f %.3f %.3f %lld %.3f\n", ms_since(g_ev_t0, t), new_pic ? 1 : 0,
                (double)e.age, (double)cam[0], (double)cam[1], (double)cam[2], serial, (double)e.tg);
    if (!g_last_print) g_last_print = t;
    if (ms_since(g_last_print, t) >= 5000.0) {
        g_last_print = t;
        stat_summary(g_ev_printed, g_ev_n, "5s");
        g_ev_printed = g_ev_n;
        if (g_dump) fflush(g_dump);
    }
}

/* ---- hash probe (tests only) -------------------------------------------- */

extern "C" int presenter_hash_on(void)
{
    if (g_hash < 0) {
        const char *e = getenv("SM64DS_PRESENTER_HASH");
        g_hash = (e && *e) ? 1 : 0;
        if (g_hash) {
            char p[512];
            _snprintf(p, sizeof p, "%s.main", e);
            p[sizeof p - 1] = 0;
            g_hash_main = fopen(p, "w");
            _snprintf(p, sizeof p, "%s.show", e);
            p[sizeof p - 1] = 0;
            g_hash_show = fopen(p, "w");
            if (!g_hash_main || !g_hash_show) g_hash = 0;
        }
    }
    return g_hash;
}

extern "C" void presenter_hash_log(const char *tag, long long serial, unsigned hash)
{
    FILE *f = tag[0] == 'm' ? g_hash_main : g_hash_show;
    if (f) { fprintf(f, "%lld %08x\n", serial, hash); }
}

extern "C" unsigned presenter_hash_texture(void *dev, void *ctx, void *tex, int who)
{
    ID3D11Device *d = (ID3D11Device *)dev;
    ID3D11DeviceContext *c = (ID3D11DeviceContext *)ctx;
    ID3D11Texture2D *t = (ID3D11Texture2D *)tex;
    D3D11_TEXTURE2D_DESC td;
    t->GetDesc(&td);
    static ID3D11Texture2D *stage[2];     /* one per calling thread */
    static UINT sw[2], sh[2];
    who &= 1;
    if (!stage[who] || sw[who] != td.Width || sh[who] != td.Height) {
        if (stage[who]) { stage[who]->Release(); stage[who] = 0; }
        D3D11_TEXTURE2D_DESC sd = td;
        sd.Usage = D3D11_USAGE_STAGING;
        sd.BindFlags = 0;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        sd.MiscFlags = 0;
        if (FAILED(d->CreateTexture2D(&sd, 0, &stage[who]))) return 0;
        sw[who] = td.Width;
        sh[who] = td.Height;
    }
    c->CopyResource(stage[who], t);
    D3D11_MAPPED_SUBRESOURCE m;
    memset(&m, 0, sizeof m);
    if (FAILED(c->Map(stage[who], 0, D3D11_MAP_READ, 0, &m)) || !m.pData) return 0;
    unsigned h = 2166136261u;
    for (UINT y = 0; y < td.Height; ++y) {
        const unsigned *p = (const unsigned *)((const char *)m.pData + (size_t)y * m.RowPitch);
        for (UINT x = 0; x < td.Width; ++x) h = (h ^ (p[x] & 0x00ffffffu)) * 16777619u;
    }
    c->Unmap(stage[who], 0);
    return h;
}

/* ---- the ring and the thread -------------------------------------------- */

namespace {

typedef HRESULT (WINAPI *D3D11CreateDevice_t)(IDXGIAdapter *, D3D_DRIVER_TYPE,
                                              HMODULE, UINT,
                                              const D3D_FEATURE_LEVEL *, UINT,
                                              UINT, ID3D11Device **,
                                              D3D_FEATURE_LEVEL *,
                                              ID3D11DeviceContext **);
int g_dev2_failed;
volatile long g_shown_serial_dbg;
int g_main_key;                    /* the key to acquire g_slot[g_main_idx] with */
unsigned long long g_drops, g_dropped;
volatile long long g_phase;           /* QPC of the thread's last turn (its take of a picture, one per refresh) */

void qlock()
{
    while (_InterlockedCompareExchange(&g_ql, 1, 0) != 0) YieldProcessor();
}
void qunlock() { _InterlockedExchange(&g_ql, 0); }

void release_ring()
{
    for (int i = 0; i < g_ns; ++i) {
        Slot &s = g_slot[i];
        if (s.km2) { s.km2->Release(); s.km2 = 0; }
        if (s.tex2) { s.tex2->Release(); s.tex2 = 0; }
        if (s.km1) { s.km1->Release(); s.km1 = 0; }
        if (s.rtv) { s.rtv->Release(); s.rtv = 0; }
        if (s.tex) { s.tex->Release(); s.tex = 0; }
    }
    if (g_shown) { g_shown->Release(); g_shown = 0; }
    if (g_ctx2) g_ctx2->Flush();
}

bool make_ring(int w, int h)
{
    for (int i = 0; i < g_ns; ++i) {
        Slot &s = g_slot[i];
        D3D11_TEXTURE2D_DESC d;
        memset(&d, 0, sizeof d);
        d.Width = (UINT)w;
        d.Height = (UINT)h;
        d.MipLevels = 1;
        d.ArraySize = 1;
        d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        d.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        IDXGIResource *res = 0;
        HANDLE handle = 0;
        bool ok = SUCCEEDED(g_dev->CreateTexture2D(&d, 0, &s.tex)) &&
                  SUCCEEDED(g_dev->CreateRenderTargetView(s.tex, 0, &s.rtv)) &&
                  SUCCEEDED(s.tex->QueryInterface(__uuidof(IDXGIKeyedMutex), (void **)&s.km1)) &&
                  SUCCEEDED(s.tex->QueryInterface(__uuidof(IDXGIResource), (void **)&res)) &&
                  SUCCEEDED(res->GetSharedHandle(&handle)) && handle &&
                  SUCCEEDED(g_dev2->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), (void **)&s.tex2)) &&
                  SUCCEEDED(s.tex2->QueryInterface(__uuidof(IDXGIKeyedMutex), (void **)&s.km2));
        if (res) res->Release();
        if (!ok) {
            release_ring();
            return false;
        }
        s.key = 0;
        s.state = 0;
        s.target = 0;
        s.gen = 0;
        s.t_pub = 0;
        s.serial = 0;
    }
    D3D11_TEXTURE2D_DESC d;
    memset(&d, 0, sizeof d);
    d.Width = (UINT)w;
    d.Height = (UINT)h;
    d.MipLevels = 1;
    d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    if (FAILED(g_dev2->CreateTexture2D(&d, 0, &g_shown))) {
        release_ring();
        return false;
    }
    return true;
}

/* Wait until QPC `target`: the hi-res timer for the bulk, a yielding spin for the
   last stretch. Returns early when the stop flag is set. */
void wait_until(long long target)
{
    for (;;) {
        if (g_stop) return;
        const long long now = qpc();
        const double left = ms_since(now, target);
        if (left <= 0.0) return;
        if (left > 1.0 && g_timer) {
            double w = left - 0.6;
            if (w > 20.0) w = 20.0;
            LARGE_INTEGER due;
            due.QuadPart = -(LONGLONG)(w * 10000.0);
            if (SetWaitableTimer(g_timer, &due, 0, 0, 0, FALSE))
                WaitForSingleObject(g_timer, 40);
            else
                Sleep(1);
        } else if (left > 1.0) {
            Sleep(1);
        } else {
            YieldProcessor();
            Sleep(0);
        }
    }
}

/* Test knobs (off unless set), to drill the two failure paths:
   SM64DS_PRESENTER_TEST_DROPFAIL=N  the Nth drop of a picture behaves as if
       its AcquireSync(1,100) timed out.
   SM64DS_PRESENTER_TEST_HANG=N:MS   the Nth Present hangs for MS milliseconds
       (the thread ignores the stop flag meanwhile, like a stuck driver call). */
int test_old()
{
    static int v = -1;
    if (v < 0) { const char *e = getenv("SM64DS_PRESENTER_TEST_OLD"); v = e && e[0] == '1'; }
    return v;
}

int test_fail_drop()
{
    static int want = -2, seen = 0;
    if (want == -2) { const char *e = getenv("SM64DS_PRESENTER_TEST_DROPFAIL"); want = e ? atoi(e) : 0; }
    return want > 0 && ++seen == want;
}

void test_hang_present()
{
    static int at = -2, ms = 0, seen = 0;
    if (at == -2) {
        const char *e = getenv("SM64DS_PRESENTER_TEST_HANG");
        at = e ? atoi(e) : 0;
        const char *c = e ? strchr(e, ':') : 0;
        ms = c ? atoi(c + 1) : 0;
    }
    if (at > 0 && ++seen == at) {
        fprintf(stderr, "[presenter] TEST: Present %d hangs for %d ms\n", at, ms);
        Sleep((DWORD)ms);
    }
}

DWORD WINAPI thread_main(LPVOID)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    pcore_thread_enter(PCORE_PRESENTER);   /* PCORE1 */
    long long period = (long long)((double)g_period_us * (double)g_qpf / 1e6);
    long long next = qpc();
    long long last_start = 0, last_gen = -1;
    long long shown_gen = 0, shown_t_pub = 0, shown_serial = 0, shown_target = 0;
    float shown_cam[3] = {0, 0, 0};
    int have = 0, occluded = 0;
    while (!g_stop) {
        /* FrameRate is live (run hunt8, lane FPSLIVE1): the grid follows
           presenter_set_grid_hz. `next` is left where it is, so the first turn
           after a change waits out at most one OLD period and the new grid
           takes over from there. */
        period = (long long)((double)g_period_us * (double)g_qpf / 1e6);
        if (g_mode == 0) {
            wait_until(next);
            next += period;
            const long long n = qpc();
            if (next < n - 4 * period) next = n + period;   /* a hitch is not a debt */
        } else if (last_start) {
            long long gap = (long long)(0.002 * (double)g_qpf);   /* floor: a window the compositor does not pace cannot make this spin */
            if (occluded) gap = period;
            wait_until(last_start + gap);
        }
        if (g_stop) break;
        /* WHICH PICTURE THIS REFRESH SHOWS. Every queued picture says which
           display moment it was made for (its target). This refresh is the
           moment `sel_t` (now, less the display delay D); it shows the queued
           picture whose target is closest to that moment, never one more than
           half a refresh ahead of it, never one older than what is on show.
           Pictures it passes over are dropped. A picture with no target
           (a menu, a scene, a tick picture drawn at once) is shown at once and
           everything older is dropped. With nothing newer the picture on show
           stays (a repeat). Timed mode off (SM64DS_PRESENTER_TIMED=0): the
           newest queued picture, the first presenter's rule. */
        const long long sel_t = qpc();
        g_phase = sel_t;
        if (!g_fixed_delay) {
            const long dl = g_goal_us - g_delay_us;
            g_delay_us += dl > STEP_US ? STEP_US : (dl < -STEP_US ? -STEP_US : dl);
        }
        const long long want = sel_t - (long long)((double)g_delay_us * (double)g_qpf / 1e6);
        const long long half = period / 2;
        int pick = -1, err_stop = 0;
        int drop[NSMAX], ndrop = 0;
        qlock();
        {
            int bi = -1;
            if (!g_timed) {
                bi = g_qn - 1;
            } else {
                for (int i = g_qn - 1; i >= 0; --i)
                    if (!g_slot[g_q[i]].target) { bi = i; break; }
                if (bi < 0) {
                    long long bd = 0;
                    for (int i = 0; i < g_qn; ++i) {
                        const long long tg = g_slot[g_q[i]].target;
                        if (tg > want + half) break;           /* the queue is in target order */
                        if (tg < shown_target) continue;       /* never backwards */
                        const long long d = tg > want ? tg - want : want - tg;
                        if (bi < 0 || d <= bd) { bi = i; bd = d; }
                    }
                    /* pictures older than what is on show can never be shown */
                    if (bi < 0) {
                        int k = 0;
                        while (k < g_qn && g_slot[g_q[k]].target < shown_target) ++k;
                        for (int i = 0; i < k; ++i) drop[ndrop++] = g_q[i];
                        for (int i = k; i < g_qn; ++i) g_q[i - k] = g_q[i];
                        g_qn -= k;
                    }
                }
            }
            if (bi >= 0) {
                for (int i = 0; i < bi; ++i) drop[ndrop++] = g_q[i];
                pick = g_q[bi];
                for (int i = bi + 1; i < g_qn; ++i) g_q[i - bi - 1] = g_q[i];
                g_qn -= bi + 1;
            }
        }
        qunlock();
        if (pt_enabled()) {
            char dl[96] = ""; int dn = 0;
            for (int i = 0; i < ndrop && dn < 80; ++i) dn += _snprintf(dl + dn, sizeof dl - dn, " %lld", g_slot[drop[i]].serial);
            pt_line("[pres] REFRESH at %.2f want %.2f pick %lld (target %.2f) dropped%s | queue %d\n", pt_abs(sel_t), pt_abs(want),
                    pick >= 0 ? g_slot[pick].serial : -1LL, pick >= 0 && g_slot[pick].target ? pt_abs(g_slot[pick].target) : 0.0, dn ? dl : " none", g_qn);
        }
        for (int i = 0; i < ndrop; ++i) {
            Slot &d = g_slot[drop[i]];
            HRESULT dhr = test_fail_drop() ? (HRESULT)WAIT_TIMEOUT : d.km2->AcquireSync(1, 100);
            if (dhr != S_OK) {
                /* Key 1 is still held by the picture: freeing the slot would
                   leave the game thread's AcquireSync(0) failing on it for good.
                   Same answer as the pick path: stop, the game thread sees the
                   error and presents from its own thread. The slot stays used. */
                if (!test_old()) {          /* TEST_OLD=1: the earlier behaviour, for the before-run of the drill */
                    _InterlockedExchange(&g_err, dhr ? (long)dhr : (long)E_FAIL);
                    err_stop = 1;
                    break;
                }
            } else {
                d.km2->ReleaseSync(0);
            }
            ++g_dropped;
            qlock();
            d.state = 0;
            qunlock();
        }
        if (err_stop) break;
        if (pick >= 0) {
            Slot &s = g_slot[pick];
            HRESULT hr = s.km2->AcquireSync(1, 100);
            if (hr != S_OK) {
                _InterlockedExchange(&g_err, hr ? (long)hr : (long)E_FAIL);
                break;
            }
            g_ctx2->CopyResource(g_shown, s.tex2);
            s.km2->ReleaseSync(0);
            shown_gen = s.gen;
            shown_t_pub = s.t_pub;
            shown_serial = s.serial;
            shown_target = s.target;
            shown_cam[0] = s.cam[0];
            shown_cam[1] = s.cam[1];
            shown_cam[2] = s.cam[2];
            have = 1;
            qlock();
            s.state = 0;
            qunlock();
        }
        if (!have) {
            Sleep(1);
            continue;
        }
        last_start = qpc();
        ID3D11Texture2D *bb = 0;
        HRESULT hr = g_sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bb);
        if (FAILED(hr) || !bb) { _InterlockedExchange(&g_err, hr ? (long)hr : (long)E_FAIL); break; }
        g_ctx2->CopyResource(bb, g_shown);
        if (presenter_hash_on())
            presenter_hash_log("show", shown_serial,
                               presenter_hash_texture(g_dev2, g_ctx2, bb, 1));
        bb->Release();
        test_hang_present();
        hr = g_sc->Present(g_mode == 1 ? 1 : 0, 0);
        if (hr == PRES_STILL_DRAWING) hr = S_OK;
        if (FAILED(hr)) { _InterlockedExchange(&g_err, (long)hr); break; }
        occluded = (hr == DXGI_STATUS_OCCLUDED);
        const int fresh = (shown_gen != last_gen);
        last_gen = shown_gen;
        stat_note_ex(shown_t_pub, fresh, shown_cam, shown_serial, shown_target);
    }
    pcore_thread_leave();   /* PCORE1 */
    return 0;
}

}  // namespace

extern "C" int presenter_running(void) { return g_running; }

/* The game thread's picture clock follows this: one picture per refresh lands
   between two takes instead of two clocks beating against each other. */
extern "C" long long presenter_phase_qpc(void) { return g_running ? g_phase : 0; }

/* The presenter's own device: the swap chain is made on it (hal/gpu_present.cpp
   asks for it before it makes one) so that the Present, which holds its device's
   lock while it queues the flip and can wait for the display, never holds the
   game thread's lock. Same adapter as the game's device; made once. */
extern "C" void *presenter_swap_device(void *dev1)
{
    if (g_dev2) return g_dev2;
    if (g_dev2_failed || !presenter_enabled() || !dev1) return 0;
    g_dev2_failed = 1;
    if (!g_d3d11) g_d3d11 = LoadLibraryA("d3d11.dll");
    D3D11CreateDevice_t cd = g_d3d11 ? (D3D11CreateDevice_t)GetProcAddress(g_d3d11, "D3D11CreateDevice") : 0;
    if (!cd) return 0;
    IDXGIDevice *dd = 0;
    IDXGIAdapter *ad = 0;
    if (FAILED(((ID3D11Device *)dev1)->QueryInterface(__uuidof(IDXGIDevice), (void **)&dd)) || !dd)
        return 0;
    const HRESULT ha = dd->GetAdapter(&ad);
    dd->Release();
    if (FAILED(ha) || !ad) return 0;
    static const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL got = (D3D_FEATURE_LEVEL)0;
    const HRESULT hr = cd(ad, D3D_DRIVER_TYPE_UNKNOWN, 0, 0, levels,
                          (UINT)(sizeof levels / sizeof levels[0]),
                          D3D11_SDK_VERSION, &g_dev2, &got, &g_ctx2);
    ad->Release();
    if (FAILED(hr) || !g_dev2 || !g_ctx2) {
        g_dev2 = 0;
        g_ctx2 = 0;
        return 0;
    }
    IDXGIDevice1 *d1 = 0;
    if (SUCCEEDED(g_dev2->QueryInterface(__uuidof(IDXGIDevice1), (void **)&d1)) && d1) {
        d1->SetMaximumFrameLatency(1);   /* at most one frame queued ahead: the picture on show is as fresh as the display allows */
        d1->Release();
    }
    g_dev2_failed = 0;
    return g_dev2;
}

/* After the swap chain is released (process exit). */
extern "C" void presenter_shutdown(void)
{
    presenter_end();
    if (g_leaked) return;           /* the stuck thread still uses them */
    if (g_ctx2) { g_ctx2->Release(); g_ctx2 = 0; }
    if (g_dev2) { g_dev2->Release(); g_dev2 = 0; }
}

extern "C" int presenter_begin(void *dev, void *ctx, void *swapchain, int w,
                               int h, int interval_mode, double grid_hz)
{
    if (g_running) return 1;
    if (g_leaked) return 0;
    if (!presenter_enabled() || !dev || !ctx || !swapchain || !g_dev2 || w <= 0 || h <= 0)
        return 0;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_qpf = f.QuadPart;
    g_dev = (ID3D11Device *)dev;
    g_ctx = (ID3D11DeviceContext *)ctx;
    g_sc = (IDXGISwapChain *)swapchain;
    if (g_timed < 0) {
        const char *e = getenv("SM64DS_PRESENTER_TIMED");
        g_timed = (e && e[0] == '0' && !e[1]) ? 0 : 1;
        g_delay_us = (long)(DEFAULT_DELAY_MS * 1000.0);
        g_goal_us = 0;
        g_fixed_delay = 0;
        g_cap_us = (long)(1000000.0 / (grid_hz < 24.0 ? 60.0 : grid_hz));   /* one refresh */
        if ((e = getenv("SM64DS_PRESENTER_DELAY_CAP_MS")) != 0 && *e) { g_cap_us = (long)(atof(e) * 1000.0); g_cap_pinned = 1; }
        if ((e = getenv("SM64DS_PRESENTER_DELAY_MS")) != 0 && *e) {
            g_delay_us = (long)(atof(e) * 1000.0);
            g_fixed_delay = 1;
        }
        if (g_cap_us < 0) g_cap_us = 0;
        if (g_cap_us > 40000) g_cap_us = 40000;
        if (g_delay_us < 0) g_delay_us = 0;
        if (g_delay_us > 40000) g_delay_us = 40000;
        g_work_n = g_work_at = 0;
    }
    /* the ring: the pictures made ahead for the rest of a tick, the display
       delay's worth, and the one being drawn. Fewer on a very large picture. */
    {
        int ns = 3;
        if (g_timed) {
            const double bytes = (double)w * (double)h * 4.0;
            ns = (int)(RING_BUDGET / (bytes > 1.0 ? bytes : 1.0));
            if (ns > NSMAX) ns = NSMAX;
            if (ns < 5) ns = 5;
        }
        g_ns = ns;
    }
    if (!make_ring(w, h)) {
        fprintf(stderr, "[presenter] the picture ring could not be shared between the two devices; the old present path stays\n");
        return 0;
    }
    g_w = w;
    g_h = h;
    g_mode = interval_mode ? 1 : 0;
    if (grid_hz < 24.0) grid_hz = 60.0;
    g_period_ms = 1000.0 / grid_hz;
    g_period_us = (long)(1000000.0 / grid_hz + 0.5);
    g_main_idx = 0;
    g_qn = 0;
    g_ql = 0;
    g_ptarget = 0;
    g_last_target = 0;
    g_main_key = 0;
    g_main_held = 0;
    g_stop = 0;
    g_err = 0;
    { const char *fe = getenv("SM64DS_PRESENTER_FLUSH"); g_flush = !(fe && fe[0] == '0' && !fe[1]); }
    g_flush_qpc = g_flush_n = 0;
    g_published = 0;
    g_pub_w = g_pub_h = 0;
    g_gen = 0;
    g_gen_t = 0;
    HMODULE k = GetModuleHandleA("kernel32.dll");
    CreateWaitableTimerExW_t cw = k ? (CreateWaitableTimerExW_t)GetProcAddress(k, "CreateWaitableTimerExW") : 0;
    g_timer = cw ? cw(0, 0, 0x2 /* HIGH_RESOLUTION */, TIMER_ALL_ACCESS) : 0;
    if (!g_timer && cw) g_timer = cw(0, 0, 0, TIMER_ALL_ACCESS);
    g_thread = CreateThread(0, 0, thread_main, 0, 0, 0);
    if (!g_thread) {
        release_ring();
        if (g_timer) { CloseHandle(g_timer); g_timer = 0; }
        return 0;
    }
    g_running = 1;
    if (presenter_stats_on())
        fprintf(stderr, "[presenter] thread started: %dx%d, %s, grid %.2f ms, %s, ring %d, delay %.2f ms\n", w, h,
                g_mode ? "Present(1)" : "timer grid, Present(0)", g_period_ms,
                g_timed ? (g_fixed_delay ? "timed" : "timed, adaptive delay") : "newest-wins", g_ns, g_delay_us / 1000.0);
    return 1;
}

/* The timer grid's rate moves while the thread runs (FrameRate is live, run
   hunt8, lane FPSLIVE1). Only the grid: the ring, the display delay and the
   swap chain stay as they are. Below 24 Hz is no answer (the start's rule). */
extern "C" void presenter_set_grid_hz(double grid_hz)
{
    if (grid_hz < 24.0) grid_hz = 60.0;
    const long us = (long)(1000000.0 / grid_hz + 0.5);
    if (us == g_period_us) return;
    g_period_ms = 1000.0 / grid_hz;
    _InterlockedExchange(&g_period_us, us);
    if (!g_cap_pinned) g_cap_us = us > 40000 ? 40000 : us;   /* one refresh, as at the start */
    if (presenter_stats_on())
        fprintf(stderr, "[presenter] grid now %.2f Hz (%.3f ms)\n", grid_hz, g_period_ms);
}

extern "C" void presenter_end(void)
{
    if (!g_running) return;
    _InterlockedExchange(&g_stop, 1);
    if (g_thread) {
        if (WaitForSingleObject(g_thread, 2000) != WAIT_OBJECT_0 && !test_old()) {
            /* The thread is stuck (a Present that does not return). It still
               touches the ring, the swap chain, the timer and its context, so
               none of them is freed: they are leaked on purpose, the swap chain
               gets an extra reference so the game thread's release leaves it
               alone, and the presenter never starts again this run. */
            fprintf(stderr, "[presenter] the thread did not stop in 2 s; its ring and swap chain are left alone (leaked) and this run presents from the game thread\n");
            g_leaked = 1;
            if (g_sc) g_sc->AddRef();
            CloseHandle(g_thread);
            g_thread = 0;
            g_main_held = 0;
            g_running = 0;
            return;
        }
        CloseHandle(g_thread);
        g_thread = 0;
    }
    if (g_timer) { CloseHandle(g_timer); g_timer = 0; }
    if (g_main_held) {
        g_slot[g_main_idx].km1->ReleaseSync(0);
        g_main_held = 0;
    }
    release_ring();
    g_running = 0;
    if (presenter_stats_on())
        fprintf(stderr, "[presenter] game thread published %lld picture(s), %lld distinct, %llu not drawn (ring full), %llu passed over by the presenter, deepest queue %d\n", g_serial, g_gen, g_drops, g_dropped, g_qmax);
    if (presenter_stats_on() && g_flush_n)
        fprintf(stderr, "[presenter] flush before publish: %lld calls, %.4f ms each\n", g_flush_n, (double)g_flush_qpc * 1000.0 / (double)g_qpf / (double)g_flush_n);
    stat_flush();
}

/* The game thread takes the texture it will draw the next picture into: its
   keyed mutex is acquired (immediately in practice: the mailbox only hands it
   a slot the presenter is done with, or one that holds a picture nobody will
   show, which is acquired with that picture's key). Null = drop this picture. */
extern "C" void *presenter_target(void)
{
    if (!g_running) return 0;
    if (!g_main_held) {
        int idx = -1;
        qlock();
        for (int i = 0; i < g_ns; ++i)
            if (g_slot[i].state == 0) { idx = i; g_slot[i].state = 1; break; }
        qunlock();
        if (idx < 0) { ++g_drops; return 0; }
        if (g_slot[idx].km1->AcquireSync(0, 8) != S_OK) {
            qlock();
            g_slot[idx].state = 0;
            qunlock();
            ++g_drops;
            return 0;
        }
        g_main_idx = idx;
        g_main_held = 1;
    }
    return (void *)g_slot[g_main_idx].rtv;
}

/* The display moment the next published picture is made for (QPC); 0 = none,
   show it at once. Consumed by that publish. */
extern "C" void presenter_set_target(long long qpc_target) { g_ptarget = qpc_target; }
extern "C" double presenter_delay_ms(void) { return g_running && g_timed ? g_delay_us / 1000.0 : 0.0; }
/* One tick's own work, in milliseconds (the game thread, once per tick). The
   delay D the presenter keeps is what lets every refresh during that work show
   a picture of the tick before; it follows the second largest of the last
   thirty-two ticks' work (plus half a millisecond), up to the cap, and moves
   by a tenth of a millisecond per refresh at most so nothing visibly shifts. */
extern "C" void presenter_note_work(double ms)
{
    if (!g_running || !g_timed || g_fixed_delay) return;
    g_work[g_work_at] = ms;
    g_work_at = (g_work_at + 1) % 32;
    if (g_work_n < 32) ++g_work_n;
    if (g_work_n < 8) return;
    double v[32];
    for (int i = 0; i < g_work_n; ++i) v[i] = g_work[i];
    qsort(v, (size_t)g_work_n, sizeof(double), cmpd);
    long goal = (long)((v[g_work_n >= 2 ? g_work_n - 2 : 0] + 0.5) * 1000.0);   /* the second largest: 31 ticks of 32 are covered */
    if (goal > g_cap_us) goal = g_cap_us;
    if (goal < 0) goal = 0;
    g_goal_work_us = goal;
    g_goal_us = goal > g_goal_ready_us ? goal : g_goal_ready_us;
}
/* run hunt8, lane MENUPACE1: WHEN THE FIRST IN-BETWEEN OF A TICK IS READY. The delay above covers the tick's own work, but the pictures
   drawn ahead for the rest of the tick are published only after the first in-between has been built too (tick work + one blend, 11 ms on
   the main menu's 30k-triangle head), so the one made for the earliest refresh of the tick was published after its display moment had
   passed; the next refresh then found it and its successor both older than the moment it showed and kept the later, and a quarter of the
   pictures drawn were never on screen (measured). `ms` is the time from the tick's start to the end of the tick's first in-between; the
   delay follows the second largest of the last thirty-two (plus half a millisecond), up to two refreshes. Only the scene path calls this
   (SM64DS_MENU_READY_DELAY=0 turns it off), so a level's delay is the one above. */
extern "C" void presenter_note_ready(double ms)
{
    if (!g_running || !g_timed || g_fixed_delay) return;
    static double r[32];
    static int at, n;
    r[at] = ms;
    at = (at + 1) % 32;
    if (n < 32) ++n;
    if (n < 8) return;
    double v[32];
    for (int i = 0; i < n; ++i) v[i] = r[i];
    qsort(v, (size_t)n, sizeof(double), cmpd);
    long goal = (long)((v[n >= 2 ? n - 2 : 0] + 0.5) * 1000.0);
    const long cap2 = 2 * (g_cap_us > 0 ? g_cap_us : 6944);
    if (goal > cap2) goal = cap2;
    if (goal < 0) goal = 0;
    g_goal_ready_us = goal;
    g_goal_us = g_goal_work_us > goal ? g_goal_work_us : goal;
}
extern "C" int presenter_timed(void) { return g_running && g_timed; }
/* Free slots in the ring (the game thread may draw ahead into them). */
extern "C" int presenter_room(void)
{
    int n = 0;
    if (!g_running) return 0;
    qlock();
    for (int i = 0; i < g_ns; ++i) if (g_slot[i].state == 0) ++n;
    qunlock();
    return n;
}

extern "C" void presenter_publish(int repeat)
{
    if (!g_running || !g_main_held) return;
    const long long t = qpc();
    if (!repeat || !g_gen) { ++g_gen; g_gen_t = t; }
    Slot &s = g_slot[g_main_idx];
    long long tg = g_timed ? g_ptarget : 0;
    g_ptarget = 0;
    if (tg == -1) {
        /* follows what is still waiting: one refresh after the last of it, or
           at once when nothing timed is waiting */
        qlock();
        const int waiting = g_qn > 0 && g_slot[g_q[g_qn - 1]].target != 0;
        qunlock();
        tg = waiting ? g_last_target + (long long)(g_period_ms * (double)g_qpf / 1000.0) : 0;
    }
    if (tg) {
        if (tg < g_last_target) tg = g_last_target;       /* targets only go forward */
        g_last_target = tg;
    }
    s.gen = g_gen;
    s.t_pub = g_gen_t;
    s.serial = ++g_serial;
    s.target = tg;
    s.cam[0] = g_pcam[0];
    s.cam[1] = g_pcam[1];
    s.cam[2] = g_pcam[2];
    pt_line("[pres] PUB serial %lld at %.2f target %s%.2f\n", s.serial, pt_abs(t), tg ? "+" : "none ", tg ? ms_since(t, tg) : 0.0);
    if (presenter_hash_on())
        presenter_hash_log("main", s.serial, presenter_hash_texture(g_dev, g_ctx, s.tex, 0));
    g_pub_w = g_w;
    g_pub_h = g_h;
    /* The keyed-mutex release is the hand-over, and the presenter's device
       reads the memory the moment it has the key: the writes are submitted to
       the card first, not left in the driver's batch. */
    if (g_flush) { const long long f0 = qpc(); g_ctx->Flush(); g_flush_qpc += qpc() - f0; ++g_flush_n; }
    s.km1->ReleaseSync(1);          /* the picture is whole: the presenter may take it */
    g_main_held = 0;
    _InterlockedExchange(&g_published, g_published + 1);
    qlock();
    s.state = 2;
    g_q[g_qn++] = g_main_idx;
    if (g_qn > g_qmax) g_qmax = g_qn;
    qunlock();
}

extern "C" int presenter_have_picture(int w, int h)
{
    return g_running && g_published > 0 && g_pub_w == w && g_pub_h == h && !g_err;
}

extern "C" long presenter_error(void) { return g_err; }

#endif
