// COOP BOOT: boot into the host's file together, the opening for everyone, the
// host leaving. Run mp1 wave 56, lane COOPBOOT1. Contract: hal/coop_boot.h.
// Wire: hal/coop_boot_wire.h. Inert unless SM64DS_COOP=1.
//
// WHERE THE BARRIERS SIT, AND WHY NOT AT THE TITLE.
// The title scene loop has no hold seam (and hal/title_entry.cpp is not this
// lane's), and nothing pumps the hub there. So the start barrier sits one step
// later, at the FIRST FRAME OF THE FILE'S LEVEL: by then every console has
// finished its own boot (title, file press, level load; the slow, uneven
// part). That is a better sync point than the press: GO releases the first
// tick of the grounds (or of the opening) on every console at once, whatever
// each one's load time was. The hold skips the game tick of the level loop
// (port_coop_boot_frame returns 1), so there is no input and no motion.
//
// The opening's end barrier sits at the opening's closing level load, which is
// the moment hal/comms_sync.cpp's port_adventure_hub_opening_held() goes from
// 1 to 0 (hal/level_change.cpp calls port_adventure_hub_level_seat() there).
// A console that sees it says ARRIVED and holds its first grounds tick until
// the host's CLOSE. Read-only use of that function; comms_sync.cpp is not
// touched.
//
// While a console is booting (no frame hook runs for a few seconds while the
// ROM loads) nobody pumps the hub. The host therefore waits a short while at
// its gate, answering the guests' save requests, before it goes to load.

#include "coop_boot.h"
#include "coop_boot_wire.h"
#include "coop_save.h"
#include "coop_wire.h"
#include "comms_hub.h"
#include "hub_presence.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
extern unsigned char data_0209caa0[];
int port_adventure_hub_opening_held(void);
}

namespace port {
namespace {

enum : unsigned { kBeatGatherMs = 250, kBeatMs = 500 };

struct Ctl {
    bool init = false;
    bool on = false;
    bool host = false;
    bool released = false;
    bool at_first_frame = false;   // this console reached the file's first level frame
    bool holding = false;
    bool exiting = false;
    // host
    unsigned ready_mask = 0;       // guests that said READY
    unsigned arrived_mask = 0;
    bool any_arrived = false;
    ULONGLONG host_ready_ms = 0;
    ULONGLONG first_arrive_ms = 0;
    bool close_sent = false;
    int phase = kCoopPhaseGather;
    unsigned req_seen = 0;         // seats that asked for the save (SYNB 0x20)
    ULONGLONG last_req_ms = 0;
    // guest
    int host_seat = -1;
    bool state_heard = false;
    CoopBootState hs = {0, 0, 0};
    bool host_left_msg = false;
    ULONGLONG hold_since = 0;
    // both
    bool intro_seen = false;
    bool arrived = false;
    bool closed = false;
    unsigned seq = 0;
    ULONGLONG last_beat = 0, last_ready = 0;
    ULONGLONG t_boot = 0;
    CoopBootStats st;
} C;

ULONGLONG tick() { return GetTickCount64(); }

// Wall clock in unix milliseconds: comparable between the consoles of a proof
// run on one machine; printed on every barrier line.
unsigned long long unix_ms() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned long long t = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (t - 116444736000000000ULL) / 10000ULL;
}

bool env_on(const char *n) {
    const char *e = std::getenv(n);
    return e && *e && std::strcmp(e, "0") != 0;
}
int env_int(const char *n, int dflt) {
    const char *e = std::getenv(n);
    return (e && *e) ? std::atoi(e) : dflt;
}

#define LOG(...) do { std::fprintf(stderr, "[coopboot] t=%llu ", unix_ms()); \
                      std::fprintf(stderr, __VA_ARGS__); std::fputc('\n', stderr); \
                      std::fflush(stderr); } while (0)

int my_seat() { return comms_hub_my_seat(); }
unsigned my_bit() { int s = my_seat(); return (s >= 0 && s < 4) ? (1u << s) : 0u; }
unsigned present_others() { return comms_hub_seat_mask() & 0xFu; }

bool send_sub(int sub, const CoopBootState *st) {
    unsigned char dg[32];
    int n = coop_boot_encode(sub, ++C.seq, st, dg, sizeof dg);
    return n > 0 && comms_hub_send(dg, n);
}

void send_state() {
    CoopBootState s = {C.released ? 1 : 0, C.phase, C.closed ? 1 : 0};
    if (send_sub(kCoopBootSubState, &s)) ++C.st.state_sent;
    C.last_beat = tick();
}

// ---- the popup line and the exit -------------------------------------------
// EVERY user32 / gdi32 CALL HERE IS BOUND AT RUN TIME. A new static DLL import
// anywhere in walk_window can map over the DS fixed addresses before the TLS
// callback claims them and break every run (memory: static import trap), so
// nothing in this file adds an import-table entry.
struct Gui {
    HMODULE u = nullptr, g = nullptr;
    HWND (WINAPI *CreateWindowExA_)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID) = nullptr;
    BOOL (WINAPI *PeekMessageA_)(LPMSG, HWND, UINT, UINT, UINT) = nullptr;
    BOOL (WINAPI *TranslateMessage_)(const MSG *) = nullptr;
    LRESULT (WINAPI *DispatchMessageA_)(const MSG *) = nullptr;
    BOOL (WINAPI *ShowWindow_)(HWND, int) = nullptr;
    BOOL (WINAPI *UpdateWindow_)(HWND) = nullptr;
    int (WINAPI *GetSystemMetrics_)(int) = nullptr;
    LRESULT (WINAPI *SendMessageA_)(HWND, UINT, WPARAM, LPARAM) = nullptr;
    HDC (WINAPI *GetDC_)(HWND) = nullptr;
    int (WINAPI *ReleaseDC_)(HWND, HDC) = nullptr;
    BOOL (WINAPI *PrintWindow_)(HWND, HDC, UINT) = nullptr;
    BOOL (WINAPI *GetWindowRect_)(HWND, LPRECT) = nullptr;
    HFONT (WINAPI *CreateFontA_)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR) = nullptr;
    HDC (WINAPI *CreateCompatibleDC_)(HDC) = nullptr;
    HBITMAP (WINAPI *CreateDIBSection_)(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD) = nullptr;
    HGDIOBJ (WINAPI *SelectObject_)(HDC, HGDIOBJ) = nullptr;
    BOOL (WINAPI *DeleteObject_)(HGDIOBJ) = nullptr;
    BOOL (WINAPI *DeleteDC_)(HDC) = nullptr;
    bool ok = false;
};
Gui G;

bool gui_load() {
    if (G.ok) return true;
    G.u = LoadLibraryA("user32.dll");
    G.g = LoadLibraryA("gdi32.dll");
    if (!G.u || !G.g) return false;
#define BIND(m, f) G.f##_ = reinterpret_cast<decltype(G.f##_)>(GetProcAddress(m, #f)); if (!G.f##_) return false
    BIND(G.u, CreateWindowExA); BIND(G.u, PeekMessageA); BIND(G.u, TranslateMessage);
    BIND(G.u, DispatchMessageA); BIND(G.u, ShowWindow); BIND(G.u, UpdateWindow);
    BIND(G.u, GetSystemMetrics); BIND(G.u, SendMessageA); BIND(G.u, GetDC);
    BIND(G.u, ReleaseDC); BIND(G.u, PrintWindow); BIND(G.u, GetWindowRect);
    BIND(G.g, CreateFontA); BIND(G.g, CreateCompatibleDC); BIND(G.g, CreateDIBSection);
    BIND(G.g, SelectObject); BIND(G.g, DeleteObject); BIND(G.g, DeleteDC);
#undef BIND
    G.ok = true;
    return true;
}

bool save_window_bmp(HWND w, const char *path) {
    RECT r;
    if (!G.GetWindowRect_(w, &r)) return false;
    const int W = r.right - r.left, H = r.bottom - r.top;
    HDC sdc = G.GetDC_(nullptr);
    HDC mdc = G.CreateCompatibleDC_(sdc);
    BITMAPINFO bi;
    std::memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void *bits = nullptr;
    HBITMAP bm = G.CreateDIBSection_(sdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool ok = false;
    if (bm && bits) {
        HGDIOBJ old = G.SelectObject_(mdc, bm);
        ok = G.PrintWindow_(w, mdc, 2 /*PW_RENDERFULLCONTENT*/) != 0;
        G.SelectObject_(mdc, old);
        if (ok) {
            BITMAPFILEHEADER fh;
            std::memset(&fh, 0, sizeof fh);
            fh.bfType = 0x4D42;
            fh.bfOffBits = sizeof fh + sizeof bi.bmiHeader;
            fh.bfSize = fh.bfOffBits + (DWORD)(W * H * 4);
            if (std::FILE *f = std::fopen(path, "wb")) {
                std::fwrite(&fh, sizeof fh, 1, f);
                std::fwrite(&bi.bmiHeader, sizeof bi.bmiHeader, 1, f);
                std::fwrite(bits, 4, (size_t)W * H, f);
                std::fclose(f);
            } else ok = false;
        }
    }
    if (bm) G.DeleteObject_(bm);
    G.DeleteDC_(mdc);
    G.ReleaseDC_(nullptr, sdc);
    return ok;
}

// One plain line on top of everything for 3 s, then the process ends. The
// proofs read it back with SM64DS_COOPBOOT_POPUP_SHOT=<bmp path>.
[[noreturn]] void leave(int code, const char *line) {
    if (C.exiting) ExitProcess((UINT)code);
    C.exiting = true;
    LOG("%s (exit code %d)", line, code);
    const bool gui = gui_load();
    HWND w = nullptr;
    if (gui) {
        const int W = 460, H = 64;
        int x = (G.GetSystemMetrics_(SM_CXSCREEN) - W) / 2;
        int y = (G.GetSystemMetrics_(SM_CYSCREEN) - H) / 2;
        w = G.CreateWindowExA_(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, "STATIC", line,
                               WS_POPUP | WS_VISIBLE | WS_BORDER | SS_CENTER | SS_CENTERIMAGE,
                               x, y, W, H, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
        if (w) {
            HFONT f = G.CreateFontA_(26, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
            if (f) G.SendMessageA_(w, WM_SETFONT, (WPARAM)f, TRUE);
            G.ShowWindow_(w, SW_SHOWNOACTIVATE);
            G.UpdateWindow_(w);
        }
    }
    const ULONGLONG t0 = tick();
    bool shot = false;
    while (tick() - t0 < 3000) {
        if (gui) {
            MSG m;
            while (G.PeekMessageA_(&m, nullptr, 0, 0, PM_REMOVE)) { G.TranslateMessage_(&m); G.DispatchMessageA_(&m); }
            if (w && !shot && tick() - t0 > 400) {
                shot = true;
                if (const char *s = std::getenv("SM64DS_COOPBOOT_POPUP_SHOT")) {
                    bool ok = save_window_bmp(w, s);
                    LOG("popup picture %s", ok ? "saved" : "FAILED");
                }
            }
        }
        Sleep(15);
    }
    std::fflush(stderr);
    ExitProcess((UINT)code);
}

void leave_host_left() { leave(kCoopExitHostLeft, "The host left. Room closed."); }

// ---- receive -----------------------------------------------------------------
void drop_role(int seat, int sub) {
    ++C.st.dropped_role;
    static int said;
    if (said < 24) { ++said; LOG("DROP seat %d: SYNB 0x%02x is not for this role", seat, sub); }
}

void handle_mine(int seat, const unsigned char *d, int len) {
    const int sub = coop_boot_validate(d, len);
    if (sub < 0) { ++C.st.dropped_bad; LOG("DROP seat %d: malformed SYNB 0x%02x len %d", seat, len > 6 ? d[6] : 0, len); return; }
    if (seat < 0 || seat > 3 || seat == my_seat()) { ++C.st.dropped_bad; return; }
    switch (sub) {
    case kCoopBootSubReady:
        if (!C.host) { drop_role(seat, sub); return; }
        ++C.st.ready_recv;
        if (!(C.ready_mask & (1u << seat))) LOG("READY from seat %d", seat);
        C.ready_mask |= 1u << seat;
        break;
    case kCoopBootSubArrived:
        if (!C.host) { drop_role(seat, sub); return; }
        ++C.st.arrived_recv;
        if (!(C.arrived_mask & (1u << seat))) LOG("ARRIVED from seat %d", seat);
        C.arrived_mask |= 1u << seat;
        if (!C.any_arrived) { C.any_arrived = true; C.first_arrive_ms = tick(); }
        break;
    case kCoopBootSubState: {
        if (C.host) { drop_role(seat, sub); return; }
        if (C.host_seat < 0) { C.host_seat = seat; LOG("host is seat %d", seat); }
        if (seat != C.host_seat) { drop_role(seat, sub); return; }
        CoopBootState s;
        if (!coop_boot_decode_state(d, len, &s)) { ++C.st.dropped_bad; return; }
        ++C.st.state_recv;
        const bool was_go = C.released;
        C.state_heard = true;
        C.hs = s;
        if (s.closed && !C.closed) { C.closed = true; LOG("CLOSE (state beat)"); }
        if (s.go && !was_go && C.at_first_frame) {
            C.released = true;
            LOG("GO heard");
        } else if (s.go && !was_go) {
            // a late joiner: the start already happened; no barrier for it
            C.released = true;
            LOG("GO already given (late joiner): no start barrier");
        }
        break;
    }
    case kCoopBootSubClose:
        if (C.host || seat != C.host_seat) { drop_role(seat, sub); return; }
        ++C.st.close_recv;
        if (!C.closed) LOG("CLOSE heard");
        C.closed = true;
        break;
    case kCoopBootSubHostLeft:
        if (C.host || seat != C.host_seat) { drop_role(seat, sub); return; }
        ++C.st.hostleft_recv;
        C.host_left_msg = true;
        LOG("HOST_LEFT heard");
        break;
    default: break;
    }
}

// SYNB ARRIVES IN TWO WAYS. COOPWORLD1 (hal/coop_world.cpp) owns the one hub
// handler for the kind and fans a datagram out by sub-type to whoever called
// coop_ctl_register; it installs that handler at the first level frame. Until
// then (the gate and the title) THIS file holds a stand-in handler that does
// the same fan-out for the two ranges it needs, so the save fetch and the
// barriers work before any level exists. Both paths end in the same two
// functions below.
void rebuild_and_run(int seat, int sub, const unsigned char *payload, int plen, bool save) {
    // the sub-type handlers want the whole datagram: rebuild the 12-byte head
    unsigned char dg[kHubCoopMaxDatagram];
    if (plen < 0 || plen + kHubCoopHeaderBytes > (int)sizeof dg) { ++C.st.dropped_bad; return; }
    HubCoopHead h;
    std::memset(&h, 0, sizeof h);
    h.tag = kHubTagCoopCtl; h.version = 1; h.seat = (uint8_t)seat; h.subtype = (uint8_t)sub;
    h.payload_len = (uint16_t)plen;
    std::memcpy(dg, &h, sizeof h);
    if (plen) std::memcpy(dg + kHubCoopHeaderBytes, payload, (size_t)plen);
    const int len = kHubCoopHeaderBytes + plen;
    if (save) {
        if (sub == 0x20 && seat >= 0 && seat < 4) { C.req_seen |= 1u << seat; C.last_req_ms = tick(); }
        coop_save_handle(seat, dg, len);
    } else {
        handle_mine(seat, dg, len);
    }
}

void sub_save(int seat, int sub, const unsigned char *p, int plen, void *) { rebuild_and_run(seat, sub, p, plen, true); }
void sub_boot(int seat, int sub, const unsigned char *p, int plen, void *) { rebuild_and_run(seat, sub, p, plen, false); }

void gate_on_ctl(int seat, const unsigned char *d, int len, void *) {
    if (!d || len < kHubCoopHeaderBytes) { ++C.st.dropped_bad; return; }
    const int sub = d[6];
    const unsigned char *p = d + kHubCoopHeaderBytes;
    const int plen = len - kHubCoopHeaderBytes;
    if (sub >= 0x20 && sub <= 0x2f) rebuild_and_run(seat, sub, p, plen, true);
    else if (coop_boot_sub_mine(sub)) rebuild_and_run(seat, sub, p, plen, false);
    else ++C.st.dropped_world;   // a world sub-type before any level group exists
}

// ---- host exit ---------------------------------------------------------------
void host_atexit() {
    if (!C.host || C.exiting || comms_hub_state() != kHubLive) return;
    for (int i = 0; i < 3; ++i) {
        if (send_sub(kCoopBootSubHostLeft, nullptr)) ++C.st.hostleft_sent;
        Sleep(40);
    }
    LOG("host exiting: HOST_LEFT sent %u times", C.st.hostleft_sent);
}

// ---- proof knobs ------------------------------------------------------------
// SM64DS_COOPBOOT_FORGE=<mode>[,<mode>...]: once, after the release, send one
// hand-made datagram per mode. The receivers must drop it (and say so).
void run_forge() {
    static bool done;
    if (done) return;
    const char *e = std::getenv("SM64DS_COOPBOOT_FORGE");
    done = true;
    if (!e || !*e) return;
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s", e);
    for (char *m = std::strtok(buf, ","); m; m = std::strtok(nullptr, ",")) {
        unsigned char dg[64];
        unsigned char pl[16] = {0};
        int plen = 4, sub = kCoopBootSubReady;
        if (!std::strcmp(m, "badlen")) plen = 8;
        else if (!std::strcmp(m, "reserved")) pl[0] = 1;
        else if (!std::strcmp(m, "state_from_guest")) { sub = kCoopBootSubState; pl[0] = 1; pl[1] = 3; pl[2] = 1; }
        else if (!std::strcmp(m, "unknown_subtype")) sub = 0x3a;
        else if (!std::strcmp(m, "hostleft_from_guest")) sub = kCoopBootSubHostLeft;
        else if (!std::strcmp(m, "ready_from_host")) sub = kCoopBootSubReady;
        else if (!std::strcmp(m, "state_pad")) { sub = kCoopBootSubState; pl[3] = 9; }
        else continue;
        int n = hub_coop_encode(kHubTagCoopCtl, sub, ++C.seq, pl, plen, dg, sizeof dg);
        bool ok = n > 0 && comms_hub_send(dg, n);
        LOG("FORGE %s sent=%d len=%d", m, ok ? 1 : 0, n);
    }
}

// ---- the per-frame engine ------------------------------------------------------
void check_host_gone() {
    if (C.host || C.exiting) return;
    if (C.host_left_msg) leave_host_left();
    if (C.host_seat >= 0 && comms_hub_state() == kHubLive) {
        HubSeatInfo si;
        if (comms_hub_seat_info(C.host_seat, &si) && !si.present) {
            LOG("the host's seat %d is gone", C.host_seat);
            leave_host_left();
        }
    }
}

void service() {
    comms_hub_pump();
    check_host_gone();
    const ULONGLONG now = tick();
    if (comms_hub_state() != kHubLive) {
        // a host with no live hub plays alone: no one to wait for
        if (C.host && C.at_first_frame && !C.released &&
            (comms_hub_state() != kHubPairing || now - C.host_ready_ms > 8000)) {
            C.released = true;
            LOG("GO: no live hub, playing alone");
        }
        return;
    }
    if (C.host) {
        const unsigned others = present_others();
        const unsigned beat = (!C.released && others) ? kBeatGatherMs : kBeatMs;
        if (now - C.last_beat >= beat) send_state();
        if (C.at_first_frame && !C.released) {
            if (coop_boot_start_due(others, C.ready_mask, true, (unsigned)(now - C.host_ready_ms))) {
                C.released = true;
                if (C.phase == kCoopPhaseGather) C.phase = kCoopPhasePlaying;
                LOG("GO: members present %x ready %x after %u ms%s", others, C.ready_mask,
                    (unsigned)(now - C.host_ready_ms),
                    (others & ~C.ready_mask) ? " (timed out, not everyone ready)" : "");
                for (int i = 0; i < 3; ++i) send_state();
            }
        }
        if (C.any_arrived && !C.close_sent) {
            const unsigned all = others | my_bit();
            if (coop_boot_close_due(all, C.arrived_mask | (C.arrived ? my_bit() : 0u), true,
                                    (unsigned)(now - C.first_arrive_ms))) {
                C.close_sent = true;
                C.closed = true;
                LOG("CLOSE: present %x arrived %x after %u ms%s", all, C.arrived_mask | my_bit(),
                    (unsigned)(now - C.first_arrive_ms),
                    (all & ~(C.arrived_mask | my_bit())) ? " (timed out)" : "");
                for (int i = 0; i < 3; ++i) {
                    if (send_sub(kCoopBootSubClose, nullptr)) ++C.st.close_sent;
                    send_state();
                }
            }
        }
    } else if (C.at_first_frame && !C.released && now - C.last_ready >= 250) {
        if (send_sub(kCoopBootSubReady, nullptr)) ++C.st.ready_sent;
        C.last_ready = now;
    }
}

void gate() {
    // The host's gate: stay and answer save requests until every present
    // guest has asked (and a short quiet), at most SM64DS_COOPBOOT_GATE_MS.
    const int budget = env_int("SM64DS_COOPBOOT_GATE_MS", 3000);
    const ULONGLONG t0 = tick();
    while ((int)(tick() - t0) < budget) {
        comms_hub_pump();
        if (comms_hub_state() == kHubLive) {
            if (tick() - C.last_beat >= kBeatGatherMs) send_state();
            port_coop_save_frame();
            const unsigned others = present_others();
            if (others && (C.req_seen & others) == others && tick() - C.last_req_ms >= 500) {
                LOG("host gate: %x guest(s) served, going to load after %u ms", others, (unsigned)(tick() - t0));
                return;
            }
        }
        Sleep(10);
    }
    LOG("host gate: done after %d ms (guests present %x asked %x)", budget, present_others(), C.req_seen);
}

[[noreturn]] void guest_fail(const char *why) {
    std::fprintf(stderr, "Could not reach the host (%s)\n", why);
    LOG("%s; exiting with code %d", why, kCoopExitNoHost);
    std::fflush(stderr);
    ExitProcess(kCoopExitNoHost);
}

void guest_gate() {
    const int hear_ms = env_int("SM64DS_COOPBOOT_HEAR_MS", 3000);
    const ULONGLONG t0 = tick();
    while (!C.state_heard) {
        comms_hub_pump();
        if ((int)(tick() - t0) >= hear_ms) guest_fail("no word from the host in 3 s");
        Sleep(10);
    }
    LOG("host heard after %u ms (phase %d go %d closed %d)", (unsigned)(tick() - t0), C.hs.phase, C.hs.go, C.hs.closed);
    // A joiner must not start its own intro: wait for the host's opening to end.
    const ULONGLONG w0 = tick();
    while (C.hs.go && C.hs.phase == kCoopPhaseIntro) {
        comms_hub_pump();
        check_host_gone();
        if (tick() - w0 > 900000ULL) guest_fail("the opening never ended");
        static ULONGLONG said;
        if (tick() - said > 2000) { said = tick(); LOG("joined during the opening: waiting for it to end"); }
        Sleep(20);
    }
    if (tick() - w0 > 100) LOG("the opening ended; entering after %u ms", (unsigned)(tick() - w0));
    if (!coop_save_fetch(env_int("SM64DS_COOPBOOT_FETCH_MS", 3000))) guest_fail("the host did not send the save in 3 s");
}

// The state of the title/menu words, for calibrating nothing at run time: kept
// out. (No title hold; see the banner.)

}  // namespace

// ---- public ------------------------------------------------------------------
bool coop_boot_active() { return C.on; }
bool coop_boot_released() { return C.released; }
bool coop_boot_holding() { return C.holding; }
CoopBootStats coop_boot_stats() { return C.st; }

}  // namespace port

extern "C" void port_coop_boot_begin(void) {
    using namespace port;
    if (C.init) return;
    C.init = true;
    if (!coop_save_active()) return;
    C.on = true;
    C.host = coop_save_is_host();
    C.t_boot = tick();
    // 1. THE ENV CONTRACT: a coop room boots to the file, always; the opening
    // is never suppressed on one console only.
    _putenv_s("SM64DS_SKIP_MENU", "1");
    _putenv_s("SM64DS_BOOT_FILE", "0");
    _putenv("SM64DS_SKIP_INTRO=");
    LOG("coop boot: role=%s, SKIP_MENU=1 BOOT_FILE=0 forced, SKIP_INTRO cleared", C.host ? "host" : "guest");
    // 2. THE ONE SYNB HANDLER
    coop_save_set_external_dispatch(true);
    hub_on_kind(kHubCoopCtl, gate_on_ctl, nullptr);
    const bool r1 = coop_ctl_register(0x20, 0x2f, sub_save, nullptr);
    const bool r2 = coop_ctl_register(kCoopBootSubLo, kCoopBootSubHi, sub_boot, nullptr);
    LOG("SYNB ranges registered with the world core: 0x20-0x2f %d, 0x30-0x3f %d", r1 ? 1 : 0, r2 ? 1 : 0);
    if (C.host) {
        coop_save_prepare();
        std::atexit(host_atexit);
        gate();
    } else {
        guest_gate();
    }
    // PROOF KNOB: a slow machine. Sit here (still pumping, so beats and save
    // answers go on) before the ROM loads.
    if (const int d = env_int("SM64DS_COOPBOOT_TEST_DELAY_MS", 0)) {
        LOG("test delay %d ms before the boot", d);
        const ULONGLONG t0 = tick();
        while ((int)(tick() - t0) < d) {
            comms_hub_pump();
            if (C.host && comms_hub_state() == kHubLive) {
                if (tick() - C.last_beat >= kBeatMs) send_state();
                port_coop_save_frame();
            }
            Sleep(10);
        }
    }
}

extern "C" int port_coop_boot_scene_frame(void) {
    using namespace port;
    if (!C.on) return 0;
    // No hold here (see the banner): keep pumping so the host answers a late
    // guest's save request and keeps its beat while the title runs.
    comms_hub_pump();
    check_host_gone();
    if (C.host && comms_hub_state() == kHubLive) {
        if (tick() - C.last_beat >= kBeatMs) send_state();
        port_coop_save_frame();
    }
    return 0;
}

// The level loop's hold. 1 = skip this frame's game tick.
extern "C" int port_coop_boot_frame(void) {
    using namespace port;
    if (!C.on) return 0;
    if (!C.at_first_frame) {
        C.at_first_frame = true;
        C.host_ready_ms = tick();
        C.last_ready = 0;
        LOG("%s at the file's first frame: %s", C.host ? "host" : "guest",
            C.released ? "go already given" : "READY");
        if (C.host) { C.phase = kCoopPhaseGather; send_state(); }
    }
    // the opening: held while the one-player seat is on; leaving that is the
    // closing level load
    const bool held = port_adventure_hub_opening_held() != 0;
    if (held) {
        if (!C.intro_seen) {
            C.intro_seen = true;
            LOG("the opening is playing here");
        }
        if (C.host && C.phase != kCoopPhaseIntro) C.phase = kCoopPhaseIntro;
    } else if (C.intro_seen && !C.arrived) {
        C.arrived = true;
        if (const int slow = env_int("SM64DS_COOPBOOT_TEST_SLOW_MS", 0)) {
            LOG("test: a slow console, %d ms before it announces", slow);
            const ULONGLONG t0 = tick();
            while ((int)(tick() - t0) < slow) { comms_hub_pump(); Sleep(10); }
        }
        const ULONGLONG now = tick();
        LOG("ARRIVED at the opening's closing load");
        if (C.host) {
            C.arrived_mask |= my_bit();
            if (!C.any_arrived) { C.any_arrived = true; C.first_arrive_ms = now; }
        } else {
            if (send_sub(kCoopBootSubArrived, nullptr)) ++C.st.arrived_sent;
            C.hold_since = now;
        }
    }
    if (C.host && C.phase == kCoopPhaseIntro && !held && (data_0209caa0[8] & 0x80)) {
        C.phase = kCoopPhaseSettled;
        LOG("host: the opening is over (seen bit set); phase settled");
    }
    if (C.host && C.phase == kCoopPhasePlaying && !C.intro_seen && !held && (data_0209caa0[8] & 0x80)) {
        C.phase = kCoopPhaseSettled;
    }
    service();
    if (C.released) run_forge();
    bool hold = false;
    const char *why = nullptr;
    if (!C.released) { hold = true; why = "start barrier"; }
    else if (C.arrived && !C.closed) { hold = true; why = "opening end barrier"; }
    if (hold && !C.host) {
        // a guest never waits for ever on a host that is still alive but silent
        static ULONGLONG since;
        if (!C.holding) since = tick();
        if (tick() - since > 30000ULL) {
            LOG("hold released after 30 s without the host's word (%s)", why);
            C.released = true;
            C.closed = true;
            hold = false;
        }
    }
    if (hold != C.holding) {
        C.holding = hold;
        LOG(hold ? "HOLD: %s" : "RELEASED (first tick now)%s", hold ? why : "");
    }
    if (!hold && C.released) {
        static int ticked;
        ++ticked;
        if (ticked == 1 || ticked == 120 || ticked == 600)
            LOG("ticked frame %d since the release", ticked);
    }
    return hold ? 1 : 0;
}
