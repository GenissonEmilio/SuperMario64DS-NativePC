// See hal/pcore.h.
#include "pcore.h"

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

namespace {

// ThreadInformationClass / ProcessInformationClass values (processthreadsapi.h)
const int kThreadPowerThrottling = 3;      // ThreadPowerThrottling
const int kProcessPowerThrottling = 4;     // ProcessPowerThrottling
const unsigned kThrottleExecSpeed = 0x1;   // *_POWER_THROTTLING_EXECUTION_SPEED
const unsigned kThrottleIgnoreTimer = 0x4; // PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION

struct Throttle { unsigned Version, ControlMask, StateMask; };

typedef BOOL (WINAPI *SetInfo_t)(HANDLE, int, LPVOID, DWORD);
typedef BOOL (WINAPI *GetSysCpuSets_t)(void *, ULONG, PULONG, HANDLE, ULONG);
typedef BOOL (WINAPI *SetThreadCpuSets_t)(HANDLE, const ULONG *, ULONG);
typedef HANDLE (WINAPI *AvSet_t)(const wchar_t *, DWORD *);
typedef BOOL (WINAPI *AvRevert_t)(HANDLE);

bool g_booted = false;
bool g_off = false;
SetInfo_t g_set_thread_info = 0;
SetThreadCpuSets_t g_set_cpu_sets = 0;
AvSet_t g_av_set = 0;
AvRevert_t g_av_revert = 0;
std::vector<ULONG> g_pset;   // fastest-class cpu set ids; empty when homogeneous
int g_p_count = 0, g_e_count = 0, g_classes = 0;

struct Tls { HANDLE mm; bool in; };
__declspec(thread) Tls t_tls = {0, false};

void probe_cpu_sets()
{
    HMODULE k = GetModuleHandleA("kernel32.dll");
    if (!k) return;
    GetSysCpuSets_t get = (GetSysCpuSets_t)GetProcAddress(k, "GetSystemCpuSetInformation");
    g_set_cpu_sets = (SetThreadCpuSets_t)GetProcAddress(k, "SetThreadSelectedCpuSets");
    if (!get) return;
    ULONG len = 0;
    get(0, 0, &len, GetCurrentProcess(), 0);
    if (len < 32 || len > (1u << 20)) return;
    std::vector<unsigned char> buf(len);
    if (!get(buf.data(), len, &len, GetCurrentProcess(), 0)) return;
    // SYSTEM_CPU_SET_INFORMATION: Size u32, Type u32 (0 = CpuSet), then
    // CPU_SET { Id u32 +8, Group u16 +12, LogicalProcessorIndex u8 +14,
    // CoreIndex u8 +15, LastLevelCacheIndex u8 +16, NumaNodeIndex u8 +17,
    // EfficiencyClass u8 +18, ... }. Read by offset so no newer SDK is needed.
    struct One { ULONG id; int cls; };
    std::vector<One> sets;
    int maxc = 0;
    for (ULONG off = 0; off + 32 <= len;) {
        const unsigned char *p = buf.data() + off;
        ULONG sz = *(const ULONG *)p, ty = *(const ULONG *)(p + 4);
        if (sz < 32) break;
        if (ty == 0) {
            One o;
            o.id = *(const ULONG *)(p + 8);
            o.cls = p[18];
            sets.push_back(o);
            if (o.cls > maxc) maxc = o.cls;
        }
        off += sz;
    }
    if (sets.empty()) return;
    int seen[256] = {0}, nc = 0;
    for (size_t i = 0; i < sets.size(); ++i)
        if (!seen[sets[i].cls]) { seen[sets[i].cls] = 1; ++nc; }
    g_classes = nc;
    for (size_t i = 0; i < sets.size(); ++i) {
        if (sets[i].cls == maxc) {
            ++g_p_count;
            if (nc > 1) g_pset.push_back(sets[i].id);
        } else {
            ++g_e_count;
        }
    }
}

void pcore_atexit(void) { pcore_thread_leave(); }

} // namespace

int pcore_active(void) { return g_booted && !g_off; }

void pcore_thread_enter(PcoreRole role)
{
    if (!pcore_active() || t_tls.in) return;
    t_tls.in = true;
    int thr = 0, mm = 0, sets = 0;
    if (g_set_thread_info) {
        Throttle t = {1, kThrottleExecSpeed, 0};   // this thread opts out of execution-speed throttling
        thr = g_set_thread_info(GetCurrentThread(), kThreadPowerThrottling, &t, sizeof t) ? 1 : 0;
    }
    if (role != PCORE_WORKER) {
        if (g_av_set) {
            DWORD idx = 0;
            t_tls.mm = g_av_set(L"Games", &idx);
            mm = t_tls.mm ? 1 : 0;
        }
        if (!g_pset.empty() && g_set_cpu_sets)
            sets = g_set_cpu_sets(GetCurrentThread(), g_pset.data(), (ULONG)g_pset.size()) ? (int)g_pset.size() : 0;
    }
    static const char *names[] = {"logic", "presenter", "worker"};
    static volatile LONG worker_lines = 0;
    if (role != PCORE_WORKER || InterlockedIncrement(&worker_lines) == 1)
        fprintf(stderr, "[cpu] thread %s: throttle-off %s, mmcss %s, cpusets %d\n", names[role],
                thr ? "yes" : "no", role == PCORE_WORKER ? "-" : (mm ? "yes" : "no"), sets);
}

void pcore_thread_leave(void)
{
    if (t_tls.mm && g_av_revert) g_av_revert(t_tls.mm);
    t_tls.mm = 0;
    t_tls.in = false;
}

void pcore_boot(void)
{
    if (g_booted) return;
    g_booted = true;
    const char *e = getenv("SM64DS_PCORE");
    g_off = e && e[0] == '0' && !e[1];
    HMODULE k = GetModuleHandleA("kernel32.dll");
    SetInfo_t set_proc = k ? (SetInfo_t)GetProcAddress(k, "SetProcessInformation") : 0;
    if (g_off) {
        // today's behaviour: opt out of timer-resolution throttling only
        int ok = 0;
        if (set_proc) {
            Throttle t = {1, kThrottleIgnoreTimer, 0};
            ok = set_proc(GetCurrentProcess(), kProcessPowerThrottling, &t, sizeof t) ? 1 : 0;
        }
        fprintf(stderr, "[cpu] SM64DS_PCORE=0: old behaviour (timer-resolution opt-out %s only)\n", ok ? "applied" : "failed");
        return;
    }
    probe_cpu_sets();
    g_set_thread_info = k ? (SetInfo_t)GetProcAddress(k, "SetThreadInformation") : 0;
    int proc = 0;
    if (set_proc) {
        Throttle t = {1, kThrottleExecSpeed | kThrottleIgnoreTimer, 0};
        proc = set_proc(GetCurrentProcess(), kProcessPowerThrottling, &t, sizeof t) ? 1 : 0;
    }
    /* a loaded box starves the logic thread of CPU (rare 0.2 to 1 s freezes); above-normal keeps its ticks on time.
       Resolved at run time so the import set does not change. */
    int prio = 0;
    if (k) {
        typedef BOOL (WINAPI *SetPrio_t)(HANDLE, DWORD);
        SetPrio_t sp = (SetPrio_t)GetProcAddress(k, "SetPriorityClass");
        if (sp) prio = sp(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS) ? 1 : 0;
    }
    if (HMODULE av = LoadLibraryA("avrt.dll")) {
        g_av_set = (AvSet_t)GetProcAddress(av, "AvSetMmThreadCharacteristicsW");
        g_av_revert = (AvRevert_t)GetProcAddress(av, "AvRevertMmThreadCharacteristics");
        if (!g_av_set || !g_av_revert) g_av_set = 0;
    }
    fprintf(stderr, "[cpu] hybrid %s (efficiency classes %d, fastest class %d / others %d cpu sets), process throttle-off %s, priority %s, mmcss %s, pinning %s\n",
            g_classes > 1 ? "yes" : "no", g_classes, g_p_count, g_e_count, proc ? "yes" : "no",
            prio ? "above-normal" : "normal",
            g_av_set ? "available" : "absent",
            g_pset.empty() ? "none (homogeneous)" : "fastest-class sets for logic + presenter");
    atexit(pcore_atexit);
    pcore_thread_enter(PCORE_LOGIC);
}

#else
void pcore_boot(void) {}
void pcore_thread_enter(PcoreRole) {}
void pcore_thread_leave(void) {}
int pcore_active(void) { return 0; }
#endif
