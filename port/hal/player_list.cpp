// THE PLAYER LIST, the model half. Run mp1 wave 56, lane PLIST1.
// What it is, where it shows and the click API: hal/player_list.h.

#include "hal/player_list.h"

#include "hal/comms_hub.h"
#include "hal/hub_presence.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
extern void *data_0209f394[];
extern unsigned char data_0209f250;
extern signed char data_0209f2f8;
extern signed char data_02092120;
extern int data_0209f220[];
extern int data_0209fc48;
}

namespace port {
namespace {

// ---------------------------------------------------------------------------
// THE STAGE CODE TABLE. Index = sublevel id (data_0209f2f8), the same ids
// SYNL carries. Several sublevels of one course share a code. course 29 (the
// castle family) never takes an " S<act>".
//   Course names for ids 29..34 and 42..43 were read out of the game's own
//   message file (data/message/msg_data_eng.bin, the course-name block that
//   follows "bowser in the sky"): course 21 "the princess's secret slide",
//   22 "the secret aquarium", 23 "? switch", 24 "the secret under the moat",
//   25 "behind the waterfall", 26 "over the rainbows", 27 "sunshine isles",
//   28 "the secret of battle fort". Courses 22, 24 and 26 also agree with the
//   ROM's stage folder names (suisou, horisoko, rainbow_mario).
// ---------------------------------------------------------------------------
struct StageRow { const char *code; bool course; };
const StageRow kStage[52] = {
    {"TST", false},  // 0  test_map
    {"CGR", false},  // 1  main_castle (grounds)
    {"C1F", false},  // 2  castle_1f
    {"CYD", false},  // 3  main_garden (courtyard)
    {"CB1", false},  // 4  castle_b1 (basement)
    {"C2F", false},  // 5  castle_2f
    {"BOB", true},   // 6  bombhei_map
    {"WF", true},    // 7  battan_king_map
    {"JRB", true},   // 8  kaizoku_irie
    {"JRB", true},   // 9  kaizoku_ship
    {"CCM", true},   // 10 snow_mt
    {"CCM", true},   // 11 snow_slider
    {"BBH", true},   // 12 teresa_house
    {"HMC", true},   // 13 cave
    {"LLL", true},   // 14 fire_land
    {"LLL", true},   // 15 fire_mt
    {"SSL", true},   // 16 desert_land
    {"SSL", true},   // 17 desert_py
    {"DDD", true},   // 18 water_land
    {"SL", true},    // 19 snow_land
    {"SL", true},    // 20 snow_kama
    {"WDW", true},   // 21 water_city
    {"TTM", true},   // 22 high_mt
    {"TTM", true},   // 23 high_slider
    {"THI", true},   // 24 tibi_deka_d
    {"THI", true},   // 25 tibi_deka_t
    {"THI", true},   // 26 tibi_deka_in
    {"TTC", true},   // 27 clock_tower
    {"RR", true},    // 28 rainbow_cruise
    {"PSS", true},   // 29 course 21 the princess's secret slide
    {"SA", true},    // 30 course 22 suisou, the secret aquarium
    {"QSW", true},   // 31 course 23 "? switch" (habatake)
    {"UTM", true},   // 32 course 24 horisoko, the secret under the moat
    {"BTW", true},   // 33 course 25 metal_switch, behind the waterfall
    {"WMR", true},   // 34 course 26 rainbow_mario, over the rainbows
    {"BDW", true},   // 35 koopa1_map
    {"BDW", true},   // 36 koopa1_boss
    {"BFS", true},   // 37 koopa2_map
    {"BFS", true},   // 38 koopa2_boss
    {"BIS", true},   // 39 koopa3_map
    {"BIS", true},   // 40 koopa3_boss
    {"---", false},  // 41 (none)
    {"SBF", true},   // 42 course 28 the secret of battle fort
    {"SI", true},    // 43 course 27 sunshine isles
    {"MKY", true},   // 44 ex_m_map  (goomboss battle, Mario's key)
    {"MKY", true},   // 45 ex_mario
    {"LKY", true},   // 46 ex_l_map  (big boo battle, Luigi's key)
    {"LKY", true},   // 47 ex_luigi
    {"WKY", true},   // 48 ex_w_map  (chief chilly challenge, Wario's key)
    {"WKY", true},   // 49 ex_wario
    {"REC", false},  // 50 playroom (Rec Room)
    {"CAS", false},  // 51 castle family, unnamed
};

const unsigned kDefaultRgb[4] = {0xE04040u, 0x40C060u, 0xE0C030u, 0x60A0F0u};

bool g_installed_log = false;
PlayerListClickFn g_click = nullptr;
PlayerListExtraFn g_extra = nullptr;

struct Rect { int seat, x, y, w, h; };
Rect g_rect[kPlistMaxRows];
int  g_nrect = 0;
int  g_ptr_x = -1, g_ptr_y = -1;
PlayerListStats g_st = {};
int  g_log = -1;
PlayerListView g_last_logged = {};
bool g_have_logged = false;

bool logging() {
    if (g_log < 0) {
        const char *e = std::getenv("SM64DS_PLIST_LOG");
        g_log = (e && *e && *e != '0') ? 1 : 0;
    }
    return g_log == 1;
}

void clean_name(const unsigned char *s, int n, char *out) {
    int o = 0;
    for (int i = 0; i < n && o < kPlistNameMax; ++i)
        out[o++] = (s[i] >= 0x20 && s[i] <= 0x7e) ? (char)s[i] : '?';
    // trim trailing spaces
    while (o > 0 && out[o - 1] == ' ') --o;
    out[o] = 0;
}

bool hexval(const char *e, unsigned *rgb) {
    if (!e || std::strlen(e) != 6) return false;
    unsigned v = 0;
    for (int i = 0; i < 6; ++i) {
        const char c = e[i];
        unsigned d;
        if (c >= '0' && c <= '9') d = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A' + 10);
        else return false;
        v = v * 16 + d;
    }
    *rgb = v;
    return true;
}

struct Place { bool in_level; int level, area, act; };

void fill_row(PlayerListRow &r, int seat, bool me, const Place &pl,
              const Place &mine) {
    r.seat = seat;
    r.me = me;
    r.in_level = pl.in_level;
    r.same_place = me || (pl.in_level && mine.in_level &&
                          pl.level == mine.level && pl.area == mine.area);
    if (pl.in_level) player_list_stage_code(pl.level, pl.act, r.code);
    else std::strcpy(r.code, "--");
    r.badge[0] = 0;
    if (g_extra) {
        const char *b = g_extra(seat);
        if (b) {
            std::strncpy(r.badge, b, kPlistBadgeMax - 1);
            r.badge[kPlistBadgeMax - 1] = 0;
        }
    }
}

bool my_place(Place *p, int *character) {
    std::memset(p, 0, sizeof *p);
    p->area = -1;
    *character = 0;
    const int me = (int)data_0209f250;
    void *a = (me >= 0 && me < 16) ? data_0209f394[me] : nullptr;
    if (!a) return false;
    p->in_level = true;
    p->level = (int)(unsigned char)data_0209f2f8;
    p->area = (int)data_02092120;
    p->act = data_0209f220[0] & 0xff;
    const unsigned c = (unsigned)(*(const int *)((const char *)a + 8) & 0xff);
    *character = c <= 3 ? (int)c : 0;
    return true;
}

void log_if_changed(const PlayerListView &v) {
    if (!logging()) return;
    bool same = g_have_logged && g_last_logged.n == v.n;
    for (int i = 0; same && i < v.n; ++i) {
        const PlayerListRow &a = g_last_logged.row[i], &b = v.row[i];
        same = a.seat == b.seat && a.character == b.character &&
               a.same_place == b.same_place && !std::strcmp(a.code, b.code) &&
               !std::strcmp(a.name, b.name);
    }
    if (same) return;
    g_last_logged = v;
    g_have_logged = true;
    std::fprintf(stderr, "[plist] rows=%d", v.n);
    for (int i = 0; i < v.n; ++i)
        std::fprintf(stderr, " | seat=%d%s name=%s char=%d code=%s %s",
                     v.row[i].seat, v.row[i].me ? "(me)" : "", v.row[i].name,
                     v.row[i].character, v.row[i].code,
                     v.row[i].same_place ? "here" : "away");
    std::fprintf(stderr, "\n");
}

}  // namespace

const char *player_list_stage_code(int level, int act, char *out) {
    if (!out) return "";
    if (level < 0 || level >= 52) {
        std::strcpy(out, "---");
        return out;
    }
    const StageRow &s = kStage[level];
    if (s.course && act >= 1 && act <= 8)
        std::snprintf(out, kPlistCodeMax, "%s S%d", s.code, act);
    else
        std::snprintf(out, kPlistCodeMax, "%s", s.code);
    return out;
}

bool player_list_active() {
    if (!comms_hub_active() || comms_hub_arena() ||
        comms_hub_state() != kHubLive)
        return false;
    const HubRoomKind k = comms_hub_room_kind();
    return k == kHubRoomParty || k == kHubRoomCoop;
}

void player_list_view(PlayerListView *out) {
    out->n = 0;
    if (!player_list_active()) return;
    if (!g_installed_log && logging()) {
        g_installed_log = true;
        std::fprintf(stderr, "[plist] active room=%d\n",
                     (int)comms_hub_room_kind());
    }
    const int my_seat = comms_hub_my_seat();
    if (my_seat < 0 || my_seat >= kHubPartySeats) return;

    Place mine;
    int mychar;
    my_place(&mine, &mychar);

    // me first
    {
        PlayerListRow &r = out->row[out->n];
        std::memset(&r, 0, sizeof r);
        char nm[kPlistNameMax + 1] = {0};
        if (const char *e = std::getenv("SM64DS_PLAYER_NAME"))
            clean_name((const unsigned char *)e, (int)std::strlen(e), nm);
        if (!nm[0]) std::snprintf(nm, sizeof nm, "PLAYER %d", my_seat + 1);
        std::strcpy(r.name, nm);
        unsigned rgb = kDefaultRgb[my_seat & 3];
        if (const char *e = std::getenv("SM64DS_PLAYER_COLOR")) hexval(e, &rgb);
        r.body_rgb = rgb;
        r.character = mychar;
        fill_row(r, my_seat, true, mine, mine);
        ++out->n;
    }
    for (int seat = 0; seat < kHubPartySeats && out->n < kPlistMaxRows;
         ++seat) {
        if (seat == my_seat) continue;
        HubSeatInfo si;
        if (!comms_hub_seat_info(seat, &si) || !si.present) continue;
        PlayerListRow &r = out->row[out->n];
        std::memset(&r, 0, sizeof r);
        r.body_rgb = kDefaultRgb[seat & 3];
        r.character = seat & 3;
        char nm[kPlistNameMax + 1] = {0};
        unsigned char id[64];
        const int n = comms_hub_latest(kHubIdent, seat, id, (int)sizeof id,
                                       nullptr);
        if (n >= (int)sizeof(HubIdentHead)) {
            HubIdentHead h;
            std::memcpy(&h, id, sizeof h);
            int nl = h.name_len;
            if (nl > kHubIdentNameMax) nl = kHubIdentNameMax;
            if ((int)sizeof h + nl > n) nl = n - (int)sizeof h;
            clean_name(id + sizeof h, nl, nm);
            r.body_rgb = ((unsigned)h.body_rgb[0] << 16) |
                         ((unsigned)h.body_rgb[1] << 8) | h.body_rgb[2];
            if (h.character <= 3) r.character = h.character;
        }
        if (!nm[0]) std::snprintf(nm, sizeof nm, "PLAYER %d", seat + 1);
        std::strcpy(r.name, nm);
        Place pl = {false, 0, -1, 0};
        HubPresence hp;
        if (hub_presence_get(seat, &hp, nullptr)) {
            pl.in_level = hp.in_level();
            pl.level = hp.level;
            pl.area = hp.area;
            pl.act = hp.act;
            if (hp.character >= 0 && hp.character <= 3)
                r.character = hp.character;
        }
        fill_row(r, seat, false, pl, mine);
        ++out->n;
    }
    log_if_changed(*out);
}

// ---------------------------------------------------------------------------
// click API
// ---------------------------------------------------------------------------
void player_list_set_click_handler(PlayerListClickFn fn) { g_click = fn; }
bool player_list_has_click_handler() { return g_click != nullptr; }
void player_list_set_row_extra(PlayerListExtraFn fn) { g_extra = fn; }

void player_list_layout_begin() { g_nrect = 0; }
void player_list_layout_row(int seat, int x, int y, int w, int h) {
    if (g_nrect >= kPlistMaxRows) return;
    g_rect[g_nrect++] = Rect{seat, x, y, w, h};
}
void player_list_set_pointer(int fb_x, int fb_y) {
    g_ptr_x = fb_x;
    g_ptr_y = fb_y;
}

int player_list_hit_test(int fb_x, int fb_y) {
    for (int i = 0; i < g_nrect; ++i) {
        const Rect &r = g_rect[i];
        if (fb_x >= r.x && fb_x < r.x + r.w && fb_y >= r.y && fb_y < r.y + r.h)
            return r.seat;
    }
    return -1;
}

bool player_list_hovered(int seat) {
    return g_click && g_ptr_x >= 0 && player_list_hit_test(g_ptr_x, g_ptr_y) == seat;
}

bool player_list_click(int fb_x, int fb_y) {
    ++g_st.clicks;
    const int seat = player_list_hit_test(fb_x, fb_y);
    if (seat < 0) return false;
    if (!g_click) {
        ++g_st.passed;
        if (logging())
            std::fprintf(stderr, "[plist] click %d,%d row seat=%d NOT consumed "
                         "(no handler)\n", fb_x, fb_y, seat);
        return false;
    }
    ++g_st.consumed;
    ++g_st.handler_calls;
    if (logging())
        std::fprintf(stderr, "[plist] click %d,%d row seat=%d CONSUMED\n",
                     fb_x, fb_y, seat);
    g_click(seat);
    return true;
}

PlayerListStats player_list_stats() { return g_st; }

}  // namespace port
