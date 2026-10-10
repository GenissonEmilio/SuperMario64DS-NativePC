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
 * 2. THE HAND-OVER. Three textures, one lock-free mailbox (g_latest, one
 *    interlocked exchange per side): the game owns g_main_idx, the presenter
 *    owns g_pres_idx, the mailbox holds the third, with a flag bit that says
 *    "newer than what the presenter has". Publishing swaps the game's texture
 *    into the mailbox and takes whatever was in it back; taking does the same
 *    for the presenter. Neither side ever waits for the other and a texture is
 *    never written while it is being shown, with no mutex at all. The GPU side
 *    is ordered by the one immediate context: the game's draw is submitted
 *    before the publish, the presenter's copy after the take, so the copy sees
 *    the whole picture; and the game's next draw into a texture the presenter
 *    has finished with is submitted after that copy.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal/presenter.h"

#ifndef DXGI_STATUS_OCCLUDED
#define DXGI_STATUS_OCCLUDED ((HRESULT)0x087A0001L)
#endif
#define PRES_STILL_DRAWING ((HRESULT)0x887A000AL)

namespace {

enum { NS = 3, NEWBIT = 0x100 };

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
Slot g_slot[NS];
int g_w, g_h;
int g_mode;                  /* 1 Present(1), 0 timer grid */
double g_period_ms = 6.944;
long long g_qpf;

volatile long g_latest;      /* slot index, | NEWBIT when the presenter has not taken it */
int g_main_idx, g_pres_idx;  /* each touched by one side only */
volatile long g_stop;
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

/* ---- statistics, shared with the old path ------------------------------- */

struct Ev { long long t; float age; unsigned char nw; };
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

extern "C" void presenter_stat_note(long long t_pub, int new_pic)
{
    if (!presenter_stats_on()) return;
    const long long t = qpc();
    if (g_ev_n >= g_ev_cap) return;
    Ev &e = g_ev[g_ev_n++];
    e.t = t;
    e.nw = (unsigned char)(new_pic ? 1 : 0);
    e.age = (float)(t_pub ? ms_since(t_pub, t) : 0.0);
    if (!g_ev_t0) g_ev_t0 = t;
    if (g_dump)
        fprintf(g_dump, "%.3f %d %.3f\n", ms_since(g_ev_t0, t), new_pic ? 1 : 0,
                (double)e.age);
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
unsigned long long g_drops;
volatile long long g_phase;           /* QPC of the thread's last turn (its take of a picture, one per refresh) */

void release_ring()
{
    for (int i = 0; i < NS; ++i) {
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
    for (int i = 0; i < NS; ++i) {
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

DWORD WINAPI thread_main(LPVOID)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    const long long period = (long long)(g_period_ms * (double)g_qpf / 1000.0);
    long long next = qpc();
    long long last_start = 0, last_gen = -1;
    long long shown_gen = 0, shown_t_pub = 0, shown_serial = 0;
    int have = 0, occluded = 0;
    while (!g_stop) {
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
        /* take the newest finished picture, if there is one; otherwise the
           picture on show stays */
        const long v = g_latest;
        if (v & NEWBIT) {
            const long old = _InterlockedExchange(&g_latest, (long)g_pres_idx);
            g_pres_idx = (int)(old & 0xff);
            Slot &s = g_slot[g_pres_idx];
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
            have = 1;
        }
        if (!have) {
            Sleep(1);
            continue;
        }
        last_start = qpc();
        g_phase = last_start;
        ID3D11Texture2D *bb = 0;
        HRESULT hr = g_sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bb);
        if (FAILED(hr) || !bb) { _InterlockedExchange(&g_err, hr ? (long)hr : (long)E_FAIL); break; }
        g_ctx2->CopyResource(bb, g_shown);
        if (presenter_hash_on())
            presenter_hash_log("show", shown_serial,
                               presenter_hash_texture(g_dev2, g_ctx2, bb, 1));
        bb->Release();
        hr = g_sc->Present(g_mode == 1 ? 1 : 0, 0);
        if (hr == PRES_STILL_DRAWING) hr = S_OK;
        if (FAILED(hr)) { _InterlockedExchange(&g_err, (long)hr); break; }
        occluded = (hr == DXGI_STATUS_OCCLUDED);
        const int fresh = (shown_gen != last_gen);
        last_gen = shown_gen;
        presenter_stat_note(shown_t_pub, fresh);
    }
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
    if (g_ctx2) { g_ctx2->Release(); g_ctx2 = 0; }
    if (g_dev2) { g_dev2->Release(); g_dev2 = 0; }
}

extern "C" int presenter_begin(void *dev, void *ctx, void *swapchain, int w,
                               int h, int interval_mode, double grid_hz)
{
    if (g_running) return 1;
    if (!presenter_enabled() || !dev || !ctx || !swapchain || !g_dev2 || w <= 0 || h <= 0)
        return 0;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_qpf = f.QuadPart;
    g_dev = (ID3D11Device *)dev;
    g_ctx = (ID3D11DeviceContext *)ctx;
    g_sc = (IDXGISwapChain *)swapchain;
    if (!make_ring(w, h)) {
        fprintf(stderr, "[presenter] the picture ring could not be shared between the two devices; the old present path stays\n");
        return 0;
    }
    g_w = w;
    g_h = h;
    g_mode = interval_mode ? 1 : 0;
    if (grid_hz < 24.0) grid_hz = 60.0;
    g_period_ms = 1000.0 / grid_hz;
    g_main_idx = 0;
    g_pres_idx = 1;
    g_latest = 2;
    g_main_key = 0;
    g_main_held = 0;
    g_stop = 0;
    g_err = 0;
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
        fprintf(stderr, "[presenter] thread started: %dx%d, %s, grid %.2f ms\n", w, h,
                g_mode ? "Present(1)" : "timer grid, Present(0)", g_period_ms);
    return 1;
}

extern "C" void presenter_end(void)
{
    if (!g_running) return;
    _InterlockedExchange(&g_stop, 1);
    if (g_thread) {
        WaitForSingleObject(g_thread, 2000);
        CloseHandle(g_thread);
        g_thread = 0;
    }
    if (g_timer) { CloseHandle(g_timer); g_timer = 0; }
    if (g_main_held) {
        g_slot[g_main_idx].km1->ReleaseSync(g_main_key);
        g_main_held = 0;
    }
    release_ring();
    g_running = 0;
    if (presenter_stats_on())
        fprintf(stderr, "[presenter] game thread published %lld picture(s), %lld distinct, %llu dropped (ring busy)\n", g_serial, g_gen, g_drops);
    stat_flush();
}

/* The game thread takes the texture it will draw the next picture into: its
   keyed mutex is acquired (immediately in practice: the mailbox only hands it
   a slot the presenter is done with, or one that holds a picture nobody will
   show, which is acquired with that picture's key). Null = drop this picture. */
extern "C" void *presenter_target(void)
{
    if (!g_running) return 0;
    Slot &s = g_slot[g_main_idx];
    if (!g_main_held) {
        if (s.km1->AcquireSync(g_main_key, 8) != S_OK) { ++g_drops; return 0; }
        g_main_held = 1;
    }
    return (void *)s.rtv;
}

extern "C" void presenter_publish(int repeat)
{
    if (!g_running || !g_main_held) return;
    const long long t = qpc();
    if (!repeat || !g_gen) { ++g_gen; g_gen_t = t; }
    Slot &s = g_slot[g_main_idx];
    s.gen = g_gen;
    s.t_pub = g_gen_t;
    s.serial = ++g_serial;
    if (presenter_hash_on())
        presenter_hash_log("main", s.serial, presenter_hash_texture(g_dev, g_ctx, s.tex, 0));
    g_pub_w = g_w;
    g_pub_h = g_h;
    s.km1->ReleaseSync(1);          /* the picture is whole: the presenter may take it */
    g_main_held = 0;
    _InterlockedExchange(&g_published, g_published + 1);
    const long old = _InterlockedExchange(&g_latest, (long)(g_main_idx | NEWBIT));
    g_main_idx = (int)(old & 0xff);
    g_main_key = (old & NEWBIT) ? 1 : 0;   /* a picture that was never shown still holds key 1 */
}

extern "C" int presenter_have_picture(int w, int h)
{
    return g_running && g_published > 0 && g_pub_w == w && g_pub_h == h && !g_err;
}

extern "C" long presenter_error(void) { return g_err; }

#endif
