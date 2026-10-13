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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
extern unsigned char data_0209caa0[];
extern void *data_0209f394[];
extern unsigned char data_0209f250;
extern unsigned char data_0209f21c;
extern int data_0209f2f8;
int port_adventure_hub_opening_held(void);
int port_adventure_ghost_drawpos(int slot, int out[3]);
short Vec3_HorzAngle(const void *a, const void *b);
void *_ZN8dActor_c4NextEPKS_(const void *prev);
}

namespace port {
namespace {

enum : unsigned { kBeatGatherMs = 250, kBeatMs = 500 };
enum { kWarmFrames = 4 };

struct Ctl {
    bool init = false;
    bool on = false;
    bool host = false;
    bool released = false;
    bool at_first_frame = false;   // this console reached the file's first level frame
    bool late_at_first = false;    // ...and the GO had already been given then (COOPSPAWN9: a late joiner)
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
    int warm = 0;   // frames ticked since the closing load, before the end barrier freezes the tick
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
        static ULONGLONG gone_since;
        if (comms_hub_seat_info(C.host_seat, &si) && !si.present) {
            // a console that stalled for a few seconds drops every seat for a
            // moment and gets them straight back: only a seat that stays gone counts
            if (!gone_since) gone_since = tick();
            if (tick() - gone_since >= 1500ULL) {
                LOG("the host's seat %d is gone", C.host_seat);
                leave_host_left();
            }
        } else {
            gone_since = 0;
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
    // 15 s: a guest launched right after the host hears nothing while the
    // host loads the ROM (it only answers once its title runs).
    const int hear_ms = env_int("SM64DS_COOPBOOT_HEAR_MS", 15000);
    const ULONGLONG t0 = tick();
    while (!C.state_heard) {
        comms_hub_pump();
        if ((int)(tick() - t0) >= hear_ms) {
            char why[64];
            std::snprintf(why, sizeof why, "no word from the host in %d s", hear_ms / 1000);
            guest_fail(why);
        }
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
    // A gate beat can be the host's last pump before it loads the ROM, so the
    // save is only served once the host is running (seen: 30 s on a loaded
    // box). The host is known to exist here, so wait for it generously.
    const int fetch_ms = env_int("SM64DS_COOPBOOT_FETCH_MS", 45000);
    if (!coop_save_fetch(fetch_ms)) {
        char why[64];
        std::snprintf(why, sizeof why, "the host did not send the save in %d s", fetch_ms / 1000);
        guest_fail(why);
    }
}

// The state of the title/menu words, for calibrating nothing at run time: kept
// out. (No title hold; see the banner.)

}  // namespace

// ---- public ------------------------------------------------------------------
bool coop_boot_active() { return C.on; }
bool coop_boot_released() { return C.released; }
bool coop_boot_holding() { return C.holding; }
bool coop_boot_ticking() { return !C.on || (C.released && !C.holding && port_adventure_hub_opening_held() == 0); }
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


namespace port {
namespace {
// ---------------------------------------------------------------------------
// COOPSPAWN9 (run mp1, items 3 and 4 of the 10-13 field report): WHERE EVERYONE STANDS AFTER THE OPENING.
//
// The opening is a one-player cutscene on every console. Its hand-over (LakituBro wakes the player, talks, and after the
// opening-seen bit the ROM walks the body on to the castle path) asks the ROM's ClosestPlayer for the body it works with,
// and coop replaces that answer with a group-wide pick: with four bodies seated some other actor of the hand-over was
// not always given the local one, so some consoles walked their body WEST to the hedge maze while the others walked on
// to the castle path (measured: out COOPSPAWN9 base_open vs base_tgtoff; the LakituBro talk itself already answered the
// local body, [cotgt] use ... cls=235). So the replacement stays OFF from the opening's start until its seen bit
// (port_coop_boot_opening_hold, read by coop_target.cpp): every console runs the hand-over on its own body exactly as one
// player does.
//
// At the seen bit the host stays where it stands and every guest moves ITS OWN body to its place on the 96-unit ring (the
// table of port_vs_extra_start in hal/level_boot.cpp, copied here because that function is file-local) around the host's
// body as the follower draws it: ring slot = my rank among the guests by hub seat. Nothing goes on the wire. If the host's
// pose is not near mine within 2 s the guest takes the ring around its own body. (The bit comes while the bodies still
// stand where the hand-over woke them, a few hundred units apart at most; the walk on to the castle is the player's.)
//
// A LATE JOINER (the GO was already given when it reached the file's first frame) has no opening: it waits for the host's
// body to show in its own level (hub presence + the follower's drawn pose, at most 4 s) and lands on the same ring around
// it. No host in the level, or no pose: it stays where the level put it.
// ---------------------------------------------------------------------------
struct SpawnState {
    bool inited = false;
    bool bit_prev = false;
    bool late = false;
    bool late_done = false;
    ULONGLONG late_since = 0;
    bool log = false;
    unsigned frame = 0;
    unsigned placed = 0;
    // after the opening
    int phase = 0;                 // 0 before the bit, 1 bit seen / waiting for the host's pose, 3 done
    unsigned bit_frame = 0;
} S9;

bool seen_bit() { return (data_0209caa0[8] & 0x80) != 0; }

char *spawn_body() {
    const int me = (int)data_0209f250;
    return (me >= 0 && me < 16 && me < (int)data_0209f21c) ? (char *)data_0209f394[me] : nullptr;
}

int rd_i32(const char *a, int off) { int v; std::memcpy(&v, a + off, 4); return v; }
void wr_i32(char *a, int off, int v) { std::memcpy(a + off, &v, 4); }

double hdist(const int *a, const int *b) {
    const double dx = ((double)a[0] - b[0]) / 4096.0, dz = ((double)a[2] - b[2]) / 4096.0;
    return std::sqrt(dx * dx + dz * dz);
}

// my place among the guests: hub seats other than the host's, counted from 1 (the host itself stays on the centre, rank 0)
int my_rank() {
    const int seat = my_seat();
    if (seat < 0 || seat > 3) return -1;
    if (C.host) return 0;
    int r = 1;
    for (int t = 0; t < seat; ++t) if (t != C.host_seat) ++r;
    return r;
}

// the ring of port_vs_extra_start for slots 1..3 (radius 96, bearings 0, 72, 144 degrees), 20.12 offsets
void ring_offset(int slot, int out[2]) {
    static const short kCos[5] = {4096, 1266, -3313, -3313, 1266};
    static const short kSin[5] = {0, 3895, 2408, -2408, -3895};
    out[0] = out[1] = 0;
    if (slot < 1 || slot > 5) return;
    out[0] = 96 * (int)kCos[slot - 1];
    out[1] = 96 * (int)kSin[slot - 1];
}

// Move my own body to cx,cz (+ the ring for my rank), keeping the centre's height; 20.12.
void spawn_move(const char *why, int cx, int cy, int cz) {
    char *b = spawn_body();
    const int rank = my_rank();
    if (!b || rank < 0) return;
    static const bool place_off = std::getenv("SM64DS_COOPSPAWN_PLACE") && std::getenv("SM64DS_COOPSPAWN_PLACE")[0] == '0';   // proof control
    if (place_off) { LOG("SPAWN %s: placement switched off (SM64DS_COOPSPAWN_PLACE=0)", why); return; }
    int off[2];
    ring_offset(rank, off);
    const int tx = cx + off[0], tz = cz + off[1];
    const int ox = rd_i32(b, 0x5c), oy = rd_i32(b, 0x60), oz = rd_i32(b, 0x64);
    wr_i32(b, 0x5c, tx);
    wr_i32(b, 0x60, cy);
    wr_i32(b, 0x64, tz);
    wr_i32(b, 0x68, tx);       // last frame's position too, so no sweep from the old spot
    wr_i32(b, 0x6c, cy);
    wr_i32(b, 0x70, tz);
    wr_i32(b, 0x98, 0);        // no horizontal speed carried over
    ++S9.placed;
    LOG("SPAWN %s: seat %d (rank %d) placed (%d,%d,%d) -> (%d,%d,%d) around centre (%d,%d,%d)", why, my_seat(), rank, ox >> 12,
        oy >> 12, oz >> 12, tx >> 12, cy >> 12, tz >> 12, cx >> 12, cy >> 12, cz >> 12);
}

// item 4 evidence: where LakituBro stands and which way he faces compared with the way to my body (SM64DS_COOPSPAWN_LOG)
void spawn_log_lakitu(const char *b) {
    for (const char *a = (const char *)_ZN8dActor_c4NextEPKS_(nullptr); a; a = (const char *)_ZN8dActor_c4NextEPKS_(a)) {
        unsigned short cls; std::memcpy(&cls, a + 0x0c, 2);
        if (cls != 235) continue;
        short yaw; std::memcpy(&yaw, a + 0x8e, 2);
        const short to_me = Vec3_HorzAngle(a + 0x5c, b + 0x5c);
        const int d = (int)(short)(yaw - to_me);
        LOG("SPAWN lakitu f=%u pos=(%d,%d,%d) yaw=%d toMe=%d diff=%d me=(%d,%d,%d)", S9.frame, rd_i32(a, 0x5c) >> 12,
            rd_i32(a, 0x60) >> 12, rd_i32(a, 0x64) >> 12, (int)yaw, (int)to_me, d < 0 ? -d : d, rd_i32(b, 0x5c) >> 12,
            rd_i32(b, 0x60) >> 12, rd_i32(b, 0x64) >> 12);
        return;
    }
}

bool host_in_my_level(int dp[3]) {
    HubPresence p;
    return C.host_seat >= 0 && hub_presence_get(C.host_seat, &p) && p.in_level() && p.level == (int)data_0209f2f8 &&
           port_adventure_ghost_drawpos(C.host_seat, dp) != 0;
}

void spawn_frame() {
    if (!S9.inited) {
        S9.inited = true;
        S9.log = env_on("SM64DS_COOPSPAWN_LOG");
        S9.bit_prev = seen_bit();
        S9.late = !C.host && C.late_at_first && S9.bit_prev && !C.intro_seen;
        S9.late_since = tick();
        if (S9.late) LOG("SPAWN late joiner: waits for the host's body to place itself on the ring");
    }
    ++S9.frame;
    char *b = spawn_body();
    if (!b) return;
    const bool bit = seen_bit();
    int me[3] = {rd_i32(b, 0x5c), rd_i32(b, 0x60), rd_i32(b, 0x64)};
    if (S9.log && C.intro_seen && !bit && (S9.frame % 20) == 0) spawn_log_lakitu(b);
    if (S9.log && C.intro_seen && !bit && (S9.frame % 10) == 0) {
        short ang; std::memcpy(&ang, b + 0x8e, 2);
        LOG("SPAWN body f=%u pos=(%d,%d,%d) spd=%d ang=%d st=%08x pause=%02x", S9.frame, me[0] >> 12, me[1] >> 12, me[2] >> 12,
            rd_i32(b, 0x98), (int)ang, (unsigned)rd_i32(b, 0x370), (unsigned)(unsigned char)b[0x13]);
    }
    if (S9.log && (bit || !C.intro_seen) && (S9.frame % 120) == 0) {
        // this console's view of every seat: my own body exactly, a friend at the pose the follower drew him at
        char v[256]; int w = 0;
        for (int s = 0; s < 4; ++s) {
            int dp[3];
            if (s == my_seat()) { dp[0] = me[0]; dp[1] = me[1]; dp[2] = me[2]; }
            else if (!port_adventure_ghost_drawpos(s, dp)) continue;
            w += std::snprintf(v + w, sizeof v - (size_t)w, " s%d%s=(%d,%d,%d)", s, s == my_seat() ? "*" : "", dp[0] >> 12, dp[1] >> 12, dp[2] >> 12);
        }
        LOG("SPAWN view f=%u level=%d%s", S9.frame, (int)data_0209f2f8, v);
    }

    // ---- after the opening: at the seen bit, a guest takes its place on the ring around the host's body (as the follower
    // draws it, when that is near mine), or around its own body when the host's pose has not come within 2 s
    if (C.intro_seen && S9.phase == 0 && bit && !S9.bit_prev) {
        S9.phase = 1;
        S9.bit_frame = S9.frame;
        LOG("SPAWN opening seen bit set on this console at (%d,%d,%d)", me[0] >> 12, me[1] >> 12, me[2] >> 12);
        if (C.host) { S9.phase = 3; LOG("SPAWN host: stays here as the centre of the ring"); }
    }
    S9.bit_prev = bit;
    if (S9.phase == 1) {
        int dp[3];
        if (host_in_my_level(dp) && hdist(dp, me) < 1500.0) {
            S9.phase = 3;
            spawn_move("after the opening", dp[0], dp[1], dp[2]);
        } else if (S9.frame - S9.bit_frame > 120) {
            S9.phase = 3;
            LOG("SPAWN no host pose near me within 2 s: ring around my own body");
            spawn_move("after the opening (own body)", me[0], me[1], me[2]);
        }
    }

    // ---- a late joiner
    if (S9.late && !S9.late_done) {
        const bool give_up = tick() - S9.late_since > 4000ULL;
        int dp[3];
        if (host_in_my_level(dp)) {
            S9.late_done = true;
            spawn_move("late joiner", dp[0], dp[1], dp[2]);
        } else if (give_up) {
            S9.late_done = true;
            LOG("SPAWN late joiner: no host body in this level within 4 s, staying where the level put me");
        }
    }
}
}  // namespace
}  // namespace port

// The level loop's hold. 1 = skip this frame's game tick.
extern "C" int port_coop_boot_frame(void) {
    using namespace port;
    if (!C.on) return 0;
    if (!C.at_first_frame) {
        C.at_first_frame = true;
        C.late_at_first = C.released;
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
    static bool said_end;
    const char *why = nullptr;
    if (!C.released) { hold = true; why = "start barrier"; }
    else if (C.arrived && !C.closed) {
        // The load's first frames must tick (the camera, the actors and the
        // draw lists are only valid after a few ticks; a frozen first frame
        // faulted the renderer), so the freeze starts after kWarmFrames.
        if (C.warm < kWarmFrames) ++C.warm;
        else { hold = true; why = "opening end barrier"; }
    } else if (C.arrived && C.closed && !said_end) {
        said_end = true;
        if (!C.holding) LOG("RELEASED (end barrier closed before the freeze)");
    }
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
    static bool said_release;
    if (hold != C.holding) {
        C.holding = hold;
        LOG(hold ? "HOLD: %s" : "RELEASED (first tick now)%s", hold ? why : "");
        if (!hold) { said_release = true; if (C.arrived) said_end = true; }
    }
    if (!hold && C.released) {
        if (!said_release) {
            said_release = true;
            LOG("RELEASED (first tick now)%s", " never held");
        }
        static int ticked;
        ++ticked;
        if (ticked == 1 || ticked == 120 || ticked == 600)
            LOG("ticked frame %d since the release", ticked);
    }
    if (!hold) spawn_frame();
    return hold ? 1 : 0;
}

// COOPSPAWN9: 1 while this console is in a new file's opening (its start until the opening-seen bit), when
// hal/coop_target.cpp leaves the ROM's own ClosestPlayer in place so the hand-over runs on the local body.
extern "C" int port_coop_boot_opening_hold(void) {
    using namespace port;
    if (!C.on) return 0;
    static const bool off = std::getenv("SM64DS_COOPSPAWN_HOLD") && std::getenv("SM64DS_COOPSPAWN_HOLD")[0] == '0';   // proof control
    if (off) return 0;
    if (port_adventure_hub_opening_held() != 0) return 1;
    return (C.intro_seen && !(data_0209caa0[8] & 0x80)) ? 1 : 0;
}
