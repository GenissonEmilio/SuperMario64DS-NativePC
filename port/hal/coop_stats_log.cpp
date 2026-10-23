// COOP SYNC STATS LINE, ALWAYS ON. Run mp1, lane MOVERS9 (COOPSCOUT9A items 10 / 14 / 24, step 1).
//
// Every 60 seconds in a coop room (and once at process exit) this writes a block of [syncstat] lines to stderr, so a field log answers
// "which classes drifted, which queue dropped what, how often did the owner flip" without a special run:
//   [syncstat] hdr   seat, level / act, group, uptime
//   [syncstat] move  the coop_actors counters (sent / received / applied / dropped by reason)
//   [syncstat] cls   one line per class seen: placed copies, records sent, applied, pinned, owner flips, and the pin distance
//                    (avg / max world units the slave pin had to move the local copy = the per-class desync number; 0 = the local
//                    Behavior agrees with the owner)
//   [syncstat] hub   evictions per datagram kind (ghost ident chat voice ... coop world, coop control) and in the mover queue
//   [syncstat] spawn / sw / boss / pop / world   the counters of the other coop lanes
// INERT outside a coop room (nothing is printed). SM64DS_COOP_STATS=0 switches it off; SM64DS_COOP_STATS_SEC=N changes the period.
// PORT_HOST_ABI: reads counters only, writes nothing in the game.

#include "coop_stats_log.h"
#include "coop_actors.h"
#include "coop_boss.h"
#include "coop_spawn.h"
#include "coop_switch.h"
#include "coop_world.h"
#include "comms_hub.h"
#include "star_popup.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace port {
namespace {

bool g_on = true, g_env = false;
unsigned g_period = 60000, g_last = 0, g_first = 0;
bool g_printed_any = false;

const char *const kKindName[kHubKinds + 1] = {
    "ghost", "ident", "chat", "voice", "arenastate", "arenaobj", "arenaref", "arenaclaim",
    "presence", "partystar", "partyhit", "coopworld", "coopctl", "event", "moverq" };

void emit(const char *why, unsigned now) {
    int lv = -1, ac = 0;
    coop_group_key(&lv, &ac);
    std::fprintf(stderr, "[syncstat] hdr why=%s t=%us seat=%d level=%d act=%d members=%#x owner=%d\n", why,
                 (now - g_first) / 1000u, comms_hub_my_seat(), lv, ac, coop_group_member_mask(), coop_group_owner());
    {
        const CoopMoveStats s = coop_move_stats();
        std::fprintf(stderr,
            "[syncstat] move ents=%llu sent=%llu/%llu snap=%llu/%llu rx=%llu/%llu applied w%llu e%llu band%llu skip%llu clsmis%llu "
            "snapapp%llu owner_ch=%llu stale=%llu thr=%llu slv=%llu drop bad%llu group%llu member%llu far%llu val%llu\n",
            s.ents, s.batches_sent, s.recs_sent, s.snap_batches_sent, s.snap_recs_sent, s.batches_rx, s.recs_rx, s.applied_write,
            s.applied_ease, s.in_band, s.skipped_state, s.cls_mismatch, s.snap_applied, s.owner_changes, s.owner_stale, s.throttled,
            s.slaved, s.drop_bad, s.drop_group, s.drop_member, s.drop_far, s.drop_val);
    }
    {
        CoopClassStat cs[128];
        const int n = coop_move_class_stats(cs, 128);
        std::sort(cs, cs + n, [](const CoopClassStat &a, const CoopClassStat &b) { return a.pin_big > b.pin_big; });
        for (int i = 0; i < n; ++i)
            std::fprintf(stderr, "[syncstat] cls %u %s placed=%u pub=%u applied=%u pinned=%u pin_avg=%.1f pin_max=%.0f pin_over40=%u owner_ch=%u held_pins=%u\n",
                         cs[i].id, cs[i].name, cs[i].ents_max, cs[i].pub, cs[i].applied, cs[i].pin_n, cs[i].pin_avg, cs[i].pin_max,
                         cs[i].pin_big, cs[i].owner_ch, cs[i].held_pins);
    }
    {
        std::fprintf(stderr, "[syncstat] hub drops");
        for (int k = 0; k <= (int)kHubKinds; ++k) {
            const unsigned long long d = comms_hub_queue_drops(k);
            if (d) std::fprintf(stderr, " %s=%llu", kKindName[k], d);
        }
        const CommsHubStats h = comms_hub_stats();
        std::fprintf(stderr, " | total_queue=%llu cap=%llu seat=%llu stale=%llu field=%llu\n", h.drop_queue, h.drop_cap, h.drop_seat, h.drop_stale, h.drop_field);
    }
    {
        const CoopSpawnStats s = coop_spawn_stats();
        std::fprintf(stderr,
            "[syncstat] spawn sent s%llu g%llu b%llu r%llu rx s%llu g%llu b%llu r%llu applied spawn%llu block%llu adopted%llu dup%llu failed%llu gone%llu "
            "red_remote%llu star_native%llu drop bad%llu group%llu member%llu class%llu old%llu rate%llu full%llu\n",
            s.sent_spawn, s.sent_gone, s.sent_block, s.sent_red, s.recv_spawn, s.recv_gone, s.recv_block, s.recv_red, s.spawn_applied,
            s.block_applied, s.spawn_adopted, s.spawn_dup, s.spawn_failed, s.gone_applied, s.red_remote, s.star_spawn_native,
            s.drop_bad, s.drop_group, s.drop_member, s.drop_class, s.drop_old, s.drop_rate, s.drop_full);
    }
    {
        const CoopSwitchStats s = coop_switch_stats();
        std::fprintf(stderr,
            "[syncstat] sw sent bit%llu word%llu press%llu kick%llu field%llu rx bit%llu word%llu press%llu kick%llu field%llu applied bits%llu press%llu "
            "kick%llu field%llu states%llu pending%llu drop bad%llu group%llu member%llu class%llu old%llu notfound%llu\n",
            s.sent_bit, s.sent_word, s.sent_press, s.sent_kick, s.sent_field, s.recv_bit, s.recv_word, s.recv_press, s.recv_kick, s.recv_field,
            s.bits_applied, s.press_applied, s.kick_applied, s.field_applied, s.states_applied, s.states_pending,
            s.drop_bad, s.drop_group, s.drop_member, s.drop_class, s.drop_old, s.drop_notfound);
    }
    {
        const CoopBossStats s = coop_boss_stats();
        std::fprintf(stderr,
            "[syncstat] boss ticks=%llu bosses=%llu sent=%llu recs=%llu hits=%llu rx=%llu applied state%llu hp%llu fields%llu mismatch%llu hit_local%llu "
            "adopted%llu claims=%llu stars spawned%llu suppressed%llu dead_marked=%llu dead_killed=%llu drop bad%llu group%llu member%llu notauth%llu cls%llu\n",
            s.ticks, s.bosses, s.sent, s.recs_sent, s.hits_sent, s.rx, s.applied_state, s.applied_hp, s.applied_fields, s.state_mismatch, s.hit_local,
            s.hit_adopted, s.claims, s.stars_spawned, s.stars_suppressed, s.dead_marked, s.dead_killed, s.drop_bad, s.drop_group, s.drop_member,
            s.drop_notauth, s.drop_cls);
    }
    {
        const StarPopupStats s = star_popup_stats();
        std::fprintf(stderr, "[syncstat] pop tx=%llu rx=%llu shown=%llu toast=%llu exits=%llu dropped=%llu dup=%llu\n", s.tx, s.rx, s.shown, s.toast, s.exits, s.dropped, s.dup);
    }
    {
        const CoopWorldStats s = coop_world_stats();
        std::fprintf(stderr,
            "[syncstat] world sent snap%llu ack%llu owner%llu delta%llu full%llu rx hello%llu enter%llu snap%llu delta%llu full%llu bits_applied=%llu "
            "reconcile_kills=%llu first_in=%llu snap_timeouts=%llu star_collects=%llu drop bad%llu group%llu target%llu unknown_sub%llu seat%llu trust%llu\n",
            s.sent_snap, s.sent_ack, s.sent_owner, s.sent_delta, s.sent_full, s.recv_hello, s.recv_enter, s.recv_snap, s.recv_delta, s.recv_full,
            s.bits_applied, s.reconcile_kills, s.first_in, s.snap_timeouts, s.star_collects, s.drop_bad, s.drop_group, s.drop_target,
            s.drop_unknown_sub, s.drop_seat, s.drop_trust);
    }
    std::fflush(stderr);
    g_printed_any = true;
}

void at_exit() {
    if (g_on && g_printed_any) emit("exit", comms_hub_now_ms());
}

}  // namespace

void coop_stats_log_tick(unsigned now) {
    if (!g_env) {
        g_env = true;
        const char *e = std::getenv("SM64DS_COOP_STATS");
        if (e && *e == '0') g_on = false;
        if (const char *p = std::getenv("SM64DS_COOP_STATS_SEC")) {
            const unsigned v = (unsigned)std::strtoul(p, nullptr, 0);
            if (v >= 1 && v <= 3600) g_period = v * 1000u;
        }
        std::atexit(at_exit);
    }
    if (!g_on || !coop_world_active()) return;
    if (g_first == 0) { g_first = now ? now : 1; g_last = g_first; }
    if ((unsigned)(now - g_last) >= g_period) { g_last = now; emit("period", now); }
}

}  // namespace port
