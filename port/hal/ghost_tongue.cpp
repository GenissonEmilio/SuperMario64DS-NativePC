// POSE9: see ghost_tongue.h. The logic is the VS puppet's V8 block, moved here
// unchanged so the adventure ghost follower (comms_sync.cpp gw_follow) can run
// the same calls.
#include "ghost_tongue.h"

#include <cstdio>

extern "C" {
void func_ov002_020d71ec(char *p, int arg);
void func_ov002_020d71a0(char *p);
extern unsigned char data_ov002_0210a34c[];
extern unsigned char data_ov002_0210a084[];
extern unsigned char data_ov002_0210eb88[];
extern unsigned data_ov002_0210a6d4[];       // St_Hurt_Init's eight hurt animation ids
}

namespace port {
namespace ghost_tongue {
namespace {
enum : unsigned { kRetractAnim = 0x6c };
unsigned g_id[2] = {0x6e, 0x6f};
}  // namespace

// The body anim ids of the two tongue anims are the first two words of a table
// in the overlay (func_ov002_020d71ec's arr[0..1]); 0x6c is the pull back.
// Ready = the ids make sense and the head files the ROM call reads exist, so
// the call cannot read a null file.
bool ready() {
    static int ok = -1;
    if (ok >= 0) return ok != 0;
    const unsigned *w = (const unsigned *)data_ov002_0210a34c;
    const unsigned *h = (const unsigned *)data_ov002_0210a084;
    ok = 1;
    if (w[0] < 0x11u || w[0] >= 0x72u || w[1] < 0x11u || w[1] >= 0x72u) ok = 0;
    for (int i = 0; ok && i < 2; ++i) {
        const unsigned p = h[i];
        if (!p || !*(const unsigned *)(p + 4)) ok = 0;
    }
    if (!*(const unsigned *)(data_ov002_0210eb88 + 4)) ok = 0;
    if (ok) { g_id[0] = w[0]; g_id[1] = w[1]; }
    std::fprintf(stderr, "[puppet] tongue anims %u %u + %u: %s\n", g_id[0], g_id[1],
                 (unsigned)kRetractAnim, ok ? "head files present" : "NOT USABLE here, tongue off");
    return ok != 0;
}

int arg_for(unsigned anim) {
    if (anim == g_id[0]) return 0;
    if (anim == g_id[1]) return 1;
    if (anim == kRetractAnim) return 2;
    return -1;
}

int select(State &st, char *body, unsigned anim, bool is_yoshi, bool *started) {
    if (started) *started = false;
    int targ = -1;
    if (is_yoshi && ready()) targ = arg_for(anim);
    if (targ != st.arg) {
        if (targ >= 0) {
            func_ov002_020d71ec(body, targ);
            ++st.starts;
            if (started) *started = true;
        } else if (st.arg >= 0) {
            func_ov002_020d71a0(body);
            ++st.stops;
        }
        st.arg = targ;
    }
    return st.arg;
}

int follow(State &st, char *body, int want_f, int *head_frames) {
    if (st.arg < 0) return -1;
    char *hm = *(char **)(body + 0x160);
    if (!hm) return -1;
    const int hframes = (*(int *)(hm + 0x54) & ~(int)0xc0000000) >> 12;
    if (head_frames) *head_frames = hframes;
    if (hframes <= 0) return -1;
    *(int *)(hm + 0x58) = (want_f % hframes) << 12;
    ++st.frames;
    return *(int *)(hm + 0x58) >> 12;
}

void forget(State &st) { st.arg = -1; }

bool is_hurt_anim(unsigned anim) {
    if (anim < 0x11u || anim >= 0x72u) return false;
    for (int i = 0; i < 8; ++i)
        if (data_ov002_0210a6d4[i] == anim) return true;
    return false;
}

unsigned blink_step(Blink &b, unsigned anim, bool *started) {
    if (started) *started = false;
    const bool hurt = is_hurt_anim(anim);
    if (hurt && !b.prev_hurt) {
        b.left = 0x24;
        ++b.starts;
        if (started) *started = true;
    }
    b.prev_hurt = hurt;
    b.prev_anim = anim;
    if (b.left <= 0) return 0;
    const unsigned v = (unsigned)b.left--;    // 0x24 down to 1, bit 0 alternates every pass
    ++b.shown;
    return v;
}

void blink_forget(Blink &b) { b.left = 0; b.prev_hurt = false; }

}  // namespace ghost_tongue
}  // namespace port
