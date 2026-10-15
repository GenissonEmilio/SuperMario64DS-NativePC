#!/usr/bin/env python3
# PARTY HITS proof (lane PLAYINT1). Two halves:
#
#   1. HOST HALF (runs anywhere with g++): builds party_hits_probe.cpp, which wraps hal/party_hits_core.h (the
#      pure rules), and drives every row below, each with its MUST-FAIL control: the same forged claim run
#      against a build of the rules with that ONE check switched off, which must then LAND. A refusal row is
#      only evidence if its control fails (a refusal that would have happened anyway proves nothing).
#      Also pins the SYNH wire (packed seq passes the real validator, hit_kind stays 0 so the relay needs
#      no change) and cross-checks the state-index table against port/tools/relay/arena_contract.py.
#
#   2. LIVE HALF (needs Windows + ROM; NOT run here): `--live-rows` prints the rows a local agent runs on two
#      or three consoles, with the log lines each must show. See cloud_tasks/PLAYINT1_DESIGN.md section 9.
#
# Usage: python port/tools/party_hits_proof.py            (host half; exit 0 = all rows ok)
#        python port/tools/party_hits_proof.py --live-rows
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(HERE, "relay"))

FAILS = []
N = [0]


def check(name, ok, detail=""):
    N[0] += 1
    print(("ok    " if ok else "FAIL  ") + name + ((" -- " + detail) if (detail and not ok) else ""))
    if not ok:
        FAILS.append(name)


class Probe:
    def __init__(self, exe):
        self.p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True)

    def q(self, line):
        self.p.stdin.write(line + "\n")
        self.p.stdin.flush()
        return self.p.stdout.readline().strip()

    def v(self, line):
        out = self.q(line)
        d = dict(kv.split("=", 1) for kv in out.split() if "=" in kv)
        return d

    def close(self):
        self.p.stdin.close()
        self.p.wait()


def build():
    exe = os.path.join(os.environ.get("TMPDIR", "/tmp"), "party_hits_probe_bin")
    r = subprocess.run(["g++", "-std=c++11", "-Wall", "-Wextra", "-Werror", "-I", os.path.join(PORT, "hal"),
                        os.path.join(HERE, "party_hits_probe.cpp"), "-o", exe], capture_output=True, text=True)
    if r.returncode:
        print(r.stderr)
        sys.exit("probe build failed")
    return exe


def judge(P, ctl="", **kw):
    """Fresh state, optional control switch (e.g. check_reach=0), one judge line. Returns the verdict word."""
    P.q("reset")
    if ctl:
        P.q("set " + ctl)
    return P.v("judge " + " ".join("%s=%s" % kv for kv in kw.items())).get("verdict")


def rows(P):
    print("-- baseline: a friend in reach lands")
    check("melee in reach lands", judge(P) == "ok")
    d = P.v("judge ik=0 now=1000")                       # the shove numbers (fresh state needed)
    P.q("reset")
    d = P.v("judge")
    check("melee shove: hurt state, no damage by default", d.get("hurt") == "1" and d.get("dmg") == "0", str(d))
    check("shove pushes AWAY from the attacker (attacker +z of me -> yaw 0x8000)", d.get("yaw") == "32768", str(d))
    P.q("reset")
    d = P.v("judge ik=2 gy=120 cy=120")
    check("stomp: squash frames set, small hop, no damage", d.get("squash") == "20" and d.get("dmg") == "0", str(d))
    P.q("reset")
    d = P.v("judge ik=1")
    check("pound: bigger shove than melee", int(d.get("speed", 0)) > 0x1C000, str(d))
    P.q("reset"); P.q("set damage=1")
    d = P.v("judge")
    check("SM64DS_ROOM_DAMAGE=1: one health unit, still a shove", d.get("dmg") == "1" and d.get("hurt") == "1", str(d))
    P.q("reset"); P.q("set damage=1")
    d = P.v("judge ik=3 cz=300 gz=300")
    check("eat claim never carries damage", d.get("verdict") == "ok" and d.get("dmg") == "0" and d.get("eat") == "1", str(d))

    print("-- MUST-FAIL pairs: each forged claim is refused, and lands once its ONE check is removed")
    pairs = [
        ("out-of-reach melee refused", dict(gz=900, cz=900), "check_reach=0", "out_of_reach"),
        ("out-of-reach stomp (friend far below my reach window)", dict(ik=2, gy=-400, cy=-400), "check_reach=0", "out_of_reach"),
        ("stomp from the side (not above) refused", dict(ik=2, gz=60, cz=60), "check_reach=0", "out_of_reach"),
        ("eat from beyond tongue range refused", dict(ik=3, gz=900, cz=900), "check_reach=0", "out_of_reach"),
        ("wrong level refused", dict(lvl=9), "check_level=0", "wrong_level"),
        ("wrong area refused", dict(area=2), "check_level=0", "wrong_area"),
        ("wrong act refused", dict(act=3), "check_level=0", "wrong_act"),
        ("attacker in a menu / cutscene refused", dict(inlevel=0), "check_level=0", "not_in_level"),
        ("toggle off (SM64DS_ROOM_INTERACT=0) refused", dict(), None, "toggle_off"),
        ("not a party / coop room (solo, VS, world) refused", dict(), None, "no_room"),
        ("victim busy (hurt / invulnerable / star-get) refused", dict(busy=1), "check_busy=0", "victim_busy"),
        ("victim already in a mouth refused", dict(eaten=1), "check_busy=0", "victim_busy"),
        ("claimed position far from where the victim saw him (forged x)", dict(cx=3000), "check_pos=0", "pos_mismatch"),
    ]
    for name, kw, ctl, want in pairs:
        P.q("reset")
        if name.startswith("toggle off"):
            P.q("set interact=0"); ctl = "check_toggle=0"
        if name.startswith("not a party"):
            P.q("set room=0"); ctl = "check_room=0"
        got = P.v("judge " + " ".join("%s=%s" % kv for kv in kw.items())).get("verdict")
        check(name, got == want, "got %s want %s" % (got, want))
        P.q("reset")
        if name.startswith("toggle off"):
            P.q("set interact=0")
        if name.startswith("not a party"):
            P.q("set room=0")
        P.q("set " + ctl)
        got2 = P.v("judge " + " ".join("%s=%s" % kv for kv in kw.items())).get("verdict")
        check("  control (check removed) lands: " + name, got2 == "ok", "got %s" % got2)

    print("-- no word from him / stale: refused (no control needed, these are inputs not checks)")
    check("no SYNL from him refused", judge(P, presence=0) == "no_presence")
    check("SYNL older than 3 s refused", judge(P, page=4000) == "no_presence")
    check("no SYNG position refused", judge(P, ghost=0) == "no_ghost")
    check("SYNG older than 1.5 s refused", judge(P, gage=2000) == "no_ghost")
    check("claim from myself / seat out of range refused", judge(P, att=0) == "bad_seat" and judge(P, att=9) == "bad_seat")
    check("a reply kind (ack / release) is not a victim-side claim", judge(P, ik=4) == "bad_kind" and judge(P, ik=5) == "bad_kind")
    check("unknown kind refused", judge(P, ik=15) == "bad_kind")

    print("-- cooldown: repeat inside 30 frames refused, after it lands; per attacker")
    P.q("reset")
    a = P.v("judge now=100").get("verdict")
    b = P.v("judge now=120 ctr=2").get("verdict")
    c = P.v("judge now=129 ctr=3").get("verdict")
    d = P.v("judge now=131 ctr=4").get("verdict")
    check("first lands, +20 and +29 refused, +31 lands", (a, b, c, d) == ("ok", "cooldown", "cooldown", "ok"), str((a, b, c, d)))
    P.q("reset"); P.q("set check_cooldown=0")
    a = P.v("judge now=100").get("verdict"); b = P.v("judge now=101 ctr=2").get("verdict")
    check("  control (cooldown removed): the repeat lands", (a, b) == ("ok", "ok"), str((a, b)))
    P.q("reset")
    a = P.v("judge att=1 now=100").get("verdict"); b = P.v("judge att=2 now=101").get("verdict")
    check("cooldown is per attacker (seat 2 right after seat 1 lands)", (a, b) == ("ok", "ok"), str((a, b)))
    P.q("reset")
    r = P.v("judge now=100 busy=1").get("verdict"); e = P.v("judge now=101").get("verdict")
    check("(refused for busy, then a clean claim next frame) lands", (r, e) == ("victim_busy", "ok"), str((r, e)))
    P.q("reset")
    a = P.v("judge now=100 ik=3 cz=300 gz=300").get("verdict"); b = P.v("judge now=140 att=2 ik=3 cz=300 gz=300").get("verdict")
    c = P.v("judge now=300 att=2 ik=3 cz=300 gz=300").get("verdict")
    check("eat re-arm: nobody is eaten twice inside 150 frames", (a, b, c) == ("ok", "eat_busy", "ok"), str((a, b, c)))
    P.q("reset"); P.q("set check_eat_rearm=0")
    P.v("judge now=100 ik=3 cz=300 gz=300"); b = P.v("judge now=140 att=2 ik=3 cz=300 gz=300").get("verdict")
    check("  control (re-arm removed): a second eat inside 150 lands", b == "ok", b)

    print("-- inputs the victim does not take on the claim's word")
    check("reach uses where I SAW him, not the position he claims (claim near, ghost far, pos check off) -> refused",
          judge(P, ctl="check_pos=0", cz=100, gz=900) == "out_of_reach")
    P.q("reset"); P.q("set check_pos=0"); P.q("set check_reach=0")
    check("  control (both checks removed): the same forged claim lands", P.v("judge cz=100 gz=900").get("verdict") == "ok")
    check("claim far, ghost near (a forger lying the other way) -> pos_mismatch", judge(P, cz=900, gz=100) == "pos_mismatch")

    print("-- attacker side: pose and position -> claim")
    for name, args, want in [
            ("punch in its window", "state=59 frames=5", "0"),
            ("punch too early", "state=59 frames=0", "-1"),
            ("punch after its window", "state=59 frames=40", "-1"),
            ("spin / dive / slide kick / sweep kick count as melee", "state=47 frames=5", "0"),
            ("landing after a ground pound = pound", "state=45 prev=61 frames=2", "1"),
            ("an ordinary landing is NOT a pound", "state=45 prev=18 frames=2", "-1"),
            ("falling in a jump = stomp candidate", "state=18 vy=-50000", "2"),
            ("rising in a jump is not", "state=18 vy=50000", "-1"),
            ("Yoshi tongue window = eat", "state=3 frames=10", "3"),
            ("idle walk = nothing", "state=14 frames=10", "-1"),
            ("hurt = nothing", "state=7 frames=3", "-1")]:
        check(name, P.v("pose " + args).get("ik") == want, P.q("pose " + args))
    check("contact: friend in front within reach", P.v("contact ik=0 mx=0 mz=0 yaw=0 tx=0 ty=0 tz=100").get("contact") == "1")
    check("contact: friend BEHIND me is not punched", P.v("contact ik=0 mx=0 mz=0 yaw=0 tx=0 ty=0 tz=-100").get("contact") == "0")
    check("contact: friend too far", P.v("contact ik=0 mx=0 mz=0 yaw=0 tx=0 ty=0 tz=900").get("contact") == "0")
    check("contact: stomp needs the friend BELOW me", P.v("contact ik=2 mx=0 my=120 mz=0 yaw=0 tx=0 ty=0 tz=0").get("contact") == "1" and
          P.v("contact ik=2 mx=0 my=0 mz=0 yaw=0 tx=0 ty=0 tz=0").get("contact") == "0")
    check("contact: pound reaches around me (no facing needed)", P.v("contact ik=1 mx=0 mz=0 yaw=0 tx=0 ty=0 tz=-150").get("contact") == "1")

    print("-- eat hold (victim side)")
    P.q("reset"); P.q("hold_begin eater=1 yaw=0 now=100")
    s1 = P.v("hold_tick now=110").get("step"); s2 = P.v("hold_tick now=146").get("step")
    check("held 10 frames in, spat at 45", (s1, s2) == ("holding", "spit"), str((s1, s2)))
    P.q("reset"); P.q("hold_begin eater=1 yaw=0 now=100")
    check("eater leaves the level: abort, no spit", P.v("hold_tick now=110 present=0").get("step") == "abort")
    P.q("reset"); P.q("hold_begin eater=1 yaw=0 now=100")
    check("room toggle turned off mid hold: abort", P.v("hold_tick now=110 toggle=0").get("step") == "abort")
    P.q("reset"); P.q("hold_begin eater=1 yaw=0 now=100")
    check("I open a menu mid hold: abort", P.v("hold_tick now=110 menu=1").get("step") == "abort")
    P.q("reset")
    check("idle hold stays idle", P.v("hold_tick now=500").get("step") == "idle")
    print("-- eat claim book (eater side): replies believed only from the seat claimed, in time")
    P.q("reset"); P.q("book sent victim=2 now=100")
    check("ack from the wrong seat refused", P.v("book ack seat=3 now=105").get("ack") == "0")
    check("ack from the victim in time believed", P.v("book ack seat=2 now=105").get("ack") == "1")
    check("release from the wrong seat refused", P.v("book release seat=3").get("release") == "0")
    check("release from the victim believed", P.v("book release seat=2").get("release") == "1")
    P.q("reset"); P.q("book sent victim=2 now=100")
    check("late ack (after 20 frames) refused", P.v("book ack seat=2 now=130").get("ack") == "0")
    P.q("reset")
    check("ack with no claim open refused", P.v("book ack seat=2 now=100").get("ack") == "0")

    print("-- wire: no relay change")
    for ik in range(6):
        w = P.v("wire ik=%d ctr=%d vic=2 att=1" % (ik, 100 + ik))
        check("SYNH with interaction %d: real validator accepts, hit_kind stays 0, kind/counter round-trip" % ik,
              w.get("validate") == "1" and w.get("hit_kind") == "0" and w.get("ik") == str(ik) and w.get("ctr") == str(100 + ik), str(w))
    w = P.v("wire ik=2 ctr=1 vic=1 att=1")
    check("victim == attacker still refused by the real validator", w.get("validate") == "0", str(w))
    w = P.v("wire ik=2 ctr=1 vic=5 att=1")
    check("victim seat 5 not encodable", w.get("encode") == "0", str(w))
    relay_src = open(os.path.join(HERE, "relay", "relay.py")).read()
    check("relay still has HIT_KINDS = 1 (no relay change is needed or made)", "HIT_KINDS = 1" in relay_src)

    print("-- the state table agrees with the contract's")
    try:
        import arena_contract as ac
        names = ac.STATE_NAMES
        c = dict(kv.split("=") for kv in P.q("consts").split())
        want = {"YoshiPower": "St_YoshiPower", "PunchKick": "St_PunchKick", "GroundPound": "St_GroundPound", "Land": "St_Land",
                "Jump": "St_Jump", "Fall": "St_Fall", "Dive": "St_Dive", "SlideKick": "St_SlideKick", "SweepKick": "St_SweepKick",
                "Spin": "St_Spin", "LongJump": "St_LongJump", "BackFlip": "St_BackFlip", "SideFlip": "St_SideFlip",
                "Hurt": "St_Hurt", "NoControl": "St_NoControl", "InYoshiMouth": "St_InYoshiMouth"}
        bad = [k for k, n in want.items() if names[int(c[k])] != n]
        check("every state index in party_hits_core.h names the same state in arena_contract.STATE_NAMES", not bad, str(bad))
        blocked = [i for i in range(78) if P.v("blocks state=%d" % i).get("blocks") == "1"]
        must = [i for i, n in enumerate(names) if n in ac.DISABLED_STATES]
        check("state_blocks_hit covers every referee DISABLED_STATE", set(must) <= set(blocked), str(sorted(set(must) - set(blocked))))
    except ImportError as e:
        check("arena_contract importable", False, str(e))


LIVE_ROWS = """LIVE ROWS (a local agent, Windows + ROM; two consoles A,B in one party room, same level / area / act):
 L1 punch:   A punches B at 100 units.    A log: [phit] claim sent ik=melee -> seat B.  B log: [phit] APPLY melee from seat A (yaw ...).
             B is pushed away and stunned; B health byte unchanged.
 L2 kick / pound / stomp: same with ik=melee / pound / stomp. Stomp: A bounces (log: [phit] stomp bounce), B squashes ~20 frames.
 L3 TOGGLE:  launch B with SM64DS_ROOM_INTERACT=0.  A punches B: A still logs the claim; B logs [phit] REFUSE toggle_off and nothing moves.
 L4 NO ROOM: single-player / VS / world console: no [phit] line at all, no hook installed (log: [phit] off: not a party/coop room).
 L5 FAR:     A forges a claim from 900 units (SM64DS_PHIT_LAB=forge:melee:900): B logs REFUSE out_of_reach; with SM64DS_PHIT_NOREACH=1 on B
             the same forged claim lands (the control).
 L6 LEVEL:   A in another level / area: B logs REFUSE wrong_level / wrong_area (control SM64DS_PHIT_NOLEVEL=1 lands).
 L7 REPEAT:  A mashes punch: B applies the first, REFUSE cooldown for the rest inside 30 frames (control SM64DS_PHIT_NOCOOLDOWN=1).
 L8 DAMAGE:  launch both with SM64DS_ROOM_DAMAGE=1: one health unit is lost per believed hit; =0 (default): none.
 L9 EAT:     A is Yoshi; tongue touches B: B logs EAT hold from seat A, parks in front of A ~45 frames, is spat out along A's facing,
             logs EAT release; A logs ack + release.  Health unchanged, no egg.  Walk B away mid-hold into another level: abort.
 L10 COOP:   the same L1..L9 in a coop room (transport SYNB 0x41, registered by party_hits itself via coop_ctl_register).
 L11 SOLID BODY UNCHANGED: coins / stars / caps / switches near a friend still behave (party_cyl.cpp is not touched by this lane).
"""

if __name__ == "__main__":
    if "--live-rows" in sys.argv:
        print(LIVE_ROWS)
        sys.exit(0)
    exe = build()
    P = Probe(exe)
    try:
        rows(P)
    finally:
        P.close()
    print("\n%d rows, %d failed" % (N[0], len(FAILS)))
    if FAILS:
        print("FAILED: " + "; ".join(FAILS))
        sys.exit(1)
    sys.exit(0)
