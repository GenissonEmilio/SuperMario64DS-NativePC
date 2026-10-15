// PARTY HITS: the host-side probe for hal/party_hits_core.h (lane PLAYINT1). Reads one command per line on
// stdin, writes one answer per line. party_hits_proof.py builds it with g++ and drives the rows. No game,
// no ROM, no Windows: the pure rules only. Build: g++ -std=c++11 -I port/hal port/tools/party_hits_probe.cpp
#include "party_hits_core.h"
#include "hub_presence.h"

#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>

using namespace port;
using namespace port::phit;

typedef std::map<std::string, long long> Args;

static Args parse(std::istringstream &in) {
    Args a;
    std::string t;
    while (in >> t) {
        const size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        a[t.substr(0, eq)] = std::atoll(t.c_str() + eq + 1);
    }
    return a;
}
static long long get(const Args &a, const char *k, long long d) {
    Args::const_iterator i = a.find(k);
    return i == a.end() ? d : i->second;
}

int main() {
    Judge judge;
    judge.tog.room = true;
    EatHold hold;
    EatClaimBook book;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd;
        if (!(in >> cmd)) continue;
        const Args a = parse(in);
        if (cmd == "reset") {
            judge = Judge();
            judge.tog.room = true;
            hold = EatHold();
            book = EatClaimBook();
            std::puts("ok");
        } else if (cmd == "set") {            // toggles + rule switches
            if (a.count("room")) judge.tog.room = a.at("room") != 0;
            if (a.count("interact")) judge.tog.interact = a.at("interact") != 0;
            if (a.count("damage")) judge.tog.damage = a.at("damage") != 0;
            if (a.count("check_toggle")) judge.rules.check_toggle = a.at("check_toggle") != 0;
            if (a.count("check_room")) judge.rules.check_room = a.at("check_room") != 0;
            if (a.count("check_level")) judge.rules.check_level = a.at("check_level") != 0;
            if (a.count("check_reach")) judge.rules.check_reach = a.at("check_reach") != 0;
            if (a.count("check_cooldown")) judge.rules.check_cooldown = a.at("check_cooldown") != 0;
            if (a.count("check_busy")) judge.rules.check_busy = a.at("check_busy") != 0;
            if (a.count("check_pos")) judge.rules.check_pos = a.at("check_pos") != 0;
            if (a.count("check_eat_rearm")) judge.rules.check_eat_rearm = a.at("check_eat_rearm") != 0;
            std::puts("ok");
        } else if (cmd == "judge") {
            Claim c;
            c.attacker = (int)get(a, "att", 1); c.victim = (int)get(a, "vic", 0);
            c.ikind = (unsigned)get(a, "ik", kMelee); c.counter = (unsigned)get(a, "ctr", 1);
            c.yaw = (int)get(a, "yaw", 0);
            c.x = (int)get(a, "cx", 0); c.y = (int)get(a, "cy", 0); c.z = (int)get(a, "cz", 100);
            Seen s;
            s.have_presence = get(a, "presence", 1) != 0; s.presence_age_ms = (unsigned)get(a, "page", 100);
            s.in_level = get(a, "inlevel", 1) != 0;
            s.at.level = (int)get(a, "lvl", 5); s.at.area = (int)get(a, "area", 0); s.at.act = (int)get(a, "act", 1);
            s.have_ghost = get(a, "ghost", 1) != 0; s.ghost_age_ms = (unsigned)get(a, "gage", 50);
            s.x = (int)get(a, "gx", 0); s.y = (int)get(a, "gy", 0); s.z = (int)get(a, "gz", 100);
            Me m;
            m.at.level = (int)get(a, "mlvl", 5); m.at.area = (int)get(a, "marea", 0); m.at.act = (int)get(a, "mact", 1);
            m.x = (int)get(a, "mx", 0); m.y = (int)get(a, "my", 0); m.z = (int)get(a, "mz", 0);
            m.busy = get(a, "busy", 0) != 0; m.eaten = get(a, "eaten", 0) != 0;
            Effect fx;
            const Verdict v = judge.judge(c, s, m, (unsigned)get(a, "now", 100), &fx);
            if (v == kOk)
                std::printf("verdict=ok yaw=%d speed=%d hop=%d dmg=%d squash=%d hurt=%d eat=%d\n", fx.away_yaw, fx.speed,
                            fx.hop, fx.damage_units, fx.squash_frames, fx.hurt_state ? 1 : 0, fx.eat ? 1 : 0);
            else
                std::printf("verdict=%s\n", verdict_name(v));
        } else if (cmd == "pose") {
            Pose p;
            p.state = (int)get(a, "state", 0xFF); p.prev_state = (int)get(a, "prev", 0xFF);
            p.frames_in_state = (unsigned)get(a, "frames", 0); p.vy = (int)get(a, "vy", 0);
            std::printf("ik=%d\n", attack_for_pose(p));
        } else if (cmd == "contact") {
            std::printf("contact=%d\n", attacker_contact((unsigned)get(a, "ik", 0), (int)get(a, "mx", 0), (int)get(a, "my", 0),
                                                         (int)get(a, "mz", 0), (int)get(a, "yaw", 0), (int)get(a, "tx", 0),
                                                         (int)get(a, "ty", 0), (int)get(a, "tz", 0)) ? 1 : 0);
        } else if (cmd == "hold_begin") {
            hold.begin((int)get(a, "eater", 1), (int)get(a, "yaw", 0), (unsigned)get(a, "now", 100));
            std::puts("ok");
        } else if (cmd == "hold_tick") {
            EatInput i;
            i.eater_present = get(a, "present", 1) != 0; i.me_dead_or_menu = get(a, "menu", 0) != 0;
            i.toggle_on = get(a, "toggle", 1) != 0;
            const EatStep st = hold.tick((unsigned)get(a, "now", 100), i);
            std::printf("step=%s\n", st == kEatHolding ? "holding" : st == kEatSpit ? "spit" : st == kEatAbort ? "abort" : "idle");
        } else if (cmd == "book") {
            std::string sub;
            std::istringstream in2(line); std::string c2; in2 >> c2 >> sub;
            Args b = parse(in2);
            if (sub == "sent") { book.sent((int)get(b, "victim", 0), (unsigned)get(b, "now", 100)); std::puts("ok"); }
            else if (sub == "ack") std::printf("ack=%d\n", book.on_ack((int)get(b, "seat", 0), (unsigned)get(b, "now", 100)) ? 1 : 0);
            else if (sub == "release") std::printf("release=%d\n", book.on_release((int)get(b, "seat", 0)) ? 1 : 0);
            else if (sub == "tick") { book.tick((unsigned)get(b, "now", 100)); std::printf("open=%d holding=%d\n", book.open() ? 1 : 0, book.holding() ? 1 : 0); }
        } else if (cmd == "wire") {
            // encode a SYNH the way the sender does, stamp the attacker byte the way the relay does, validate it
            // with the real validator, decode the interaction back
            unsigned char d[64];
            const int ik = (int)get(a, "ik", 0), ctr = (int)get(a, "ctr", 1), vic = (int)get(a, "vic", 2), att = (int)get(a, "att", 1);
            const int n = hub_party_hit_encode(vic, kHitShove, pack_seq((unsigned)ik, (unsigned)ctr), (int)get(a, "yaw", 0),
                                               (int)get(a, "x", 0), (int)get(a, "y", 0), (int)get(a, "z", 0), d);
            if (!n) { std::puts("encode=0"); continue; }
            d[5] = (unsigned char)att;
            const bool ok = hub_party_hit_validate(d, n);
            HubPartyHitWire w; std::memcpy(&w, d, sizeof w);
            std::printf("encode=%d validate=%d hit_kind=%d ik=%u ctr=%u\n", n, ok ? 1 : 0, w.hit_kind, seq_ikind(w.seq), seq_counter(w.seq));
        } else if (cmd == "consts") {
            std::printf("YoshiPower=%d PunchKick=%d GroundPound=%d Land=%d Jump=%d Fall=%d Dive=%d SlideKick=%d SweepKick=%d Spin=%d "
                        "LongJump=%d BackFlip=%d SideFlip=%d Hurt=%d NoControl=%d InYoshiMouth=%d\n", (int)kStYoshiPower,
                        (int)kStPunchKick, (int)kStGroundPound, (int)kStLand, (int)kStJump, (int)kStFall, (int)kStDive,
                        (int)kStSlideKick, (int)kStSweepKick, (int)kStSpin, (int)kStLongJump, (int)kStBackFlip, (int)kStSideFlip,
                        (int)kStHurt, (int)kStNoControl, (int)kStInYoshiMouth);
        } else if (cmd == "blocks") {
            std::printf("blocks=%d\n", state_blocks_hit((int)get(a, "state", 0)) ? 1 : 0);
        } else {
            std::puts("?");
        }
        std::fflush(stdout);
    }
    return 0;
}
