#!/usr/bin/env python3
"""ARENA MODE LOG READERS AND JUDGES (run mp1, lane ARENAGATE1, plan phase A6).

Pure functions over the text of console logs and the local relay's log: no
process, no lock, no file. mp_ladder.py's arena rungs launch the consoles and
hand the texts here; every judge returns a list of (ok, message) items the
ladder's Checks takes as they are, so one rung's verdict names exactly which
item failed.

Every judge has a MUST-FAIL CONTROL: mutate(kind, texts) returns a copy of the
real logs with one thing broken (two winners, a console that never heard the
verdict, a body that answers 4 frames late, a camera on the wrong slot...) and
the rung re-runs the same judge on it and demands a failure. A judge that
cannot go red proves nothing.

    python arena_logs.py --selftest        # judges against built-in fixtures,
                                           # incl. every control
    python arena_logs.py --digest 0 2 time 0     # the lobby's session digest

The lines read are the ones the lanes print (contract: out\\ARENACORE1\\
contract.md, ARENALOBBY1 contract section 10, STARREF1's vs_arena_ref.cpp,
PUPPET1's puppet rows); they are the ladder's proposal where a lane has not
landed and change HERE.
"""
import collections
import copy
import re
import sys

# ---------------------------------------------------------------------------
# the lines
# ---------------------------------------------------------------------------
FAULT = re.compile(r"\bFAULT\b|c0000005|c0000409|\bFATAL\b")
LOCKSTEP_TELL = re.compile(r"\[comms:loopback\]|THE HOST LEFT THE VS MATCH|NOT installing", re.M)
INSTALLED = re.compile(r"^\[vsarena\] installed: arena mode, (\d+) bodies, seat pin (-?\d+)", re.M)
SEATED = re.compile(r"^\[vsarena\] seated: seat (\d+) is my body", re.M)
RELAY_SEAT = re.compile(r"^\[vsarena\] the relay named my seat", re.M)
FIRST_VS = re.compile(r"^\[vsarena\] first VS frame: seat (\d+), (\d+) bodies", re.M)
UP = re.compile(r"^\[vsarena\] session up", re.M)
START = re.compile(r"^\[vsarena\] START t0=\d+ first_to=(\d+) time=(\d+)s roster=0x([0-9a-f]+) "
                   r"markers=(\d+) seed=(\d+) map=(-?\d+)", re.M)
READY = re.compile(r"^\[vsarena\] READY sent: map=(\d+) time=(\d+)s first_to=(\d+) capacity=(\d+) "
                   r"markers=(\d+) digest=(\d+)", re.M)
OVER_REF = re.compile(r"^\[vsarena\] MATCH OVER ref reason=(\d+) winners=0x([0-9a-f]+) count=(\d+) "
                      r"scores=(-?\d+),(-?\d+),(-?\d+),(-?\d+) seq=(\d+)", re.M)
OVER_LEG = re.compile(r"^\[vs\] MATCH OVER f(\d+) (win=\S+ scores=\S+ players=\d+ total=\d+) "
                      r"pending_scene=\d+ results_screen=(\S+) (winner=-?\d+[^\n]*)", re.M)
OVER_ANY = re.compile(r"^\[vs\] MATCH OVER[^\n]*", re.M)
OVER_ARENA = re.compile(r"^\[vs\] f(\d+) ARENA MATCH OVER from the referee: reason=(\d+) "
                        r"winners=0x([0-9a-f]+) scores ([^\n]*)", re.M)
CONN_LOST = re.compile(r"connection lost", re.I)
LEAVE = re.compile(r"^\[vsarena-ref\] LEAVE seat=(\d+) why=(\d+) spilled=(\d+)", re.M)
JOIN = re.compile(r"^\[vsarena-ref\] JOIN seat=(\d+) mid=(\d+)", re.M)
TAKEN = re.compile(r"^\[vsarena-ref\] STAR_TAKEN inst=(\d+) winner=(\d+) score=(-?\d+) claim=(\d+) "
                   r"progress=(\d+)", re.M)
SPILL = re.compile(r"^\[vsarena-ref\] SPILL inst=(\d+) seat=(\d+) marker=(\d+) cause=(\d+) "
                   r"score=(-?\d+)", re.M)
HIT = re.compile(r"^\[vsarena-ref\] HIT victim=(\d+) attacker=(\d+) trade=(\d+)", re.M)
EXITLINE = re.compile(r"^\[vsarena\] exit [^\n]*", re.M)
HUB_EXIT = re.compile(r"^\[hub\] exit[^\n]*", re.M)
PUP_TAKEN = re.compile(r"taken over: Behavior skipped")
PUP_LINE = re.compile(r"^\[pup\] t=\d+ f=\d+ seat=(\d+) ", re.M)
LAT = re.compile(r"^\[vsarena-lat\] press A at tick (\d+), my body \(slot (\d+)\) answered at "
                 r"tick (\d+): (\d+) frame\(s\) \(state (\d+) -> (\d+)\) netmode=(\S+)", re.M)
NOANS = re.compile(r"^\[vsarena-lat\] press A at tick (\d+): no answer", re.M)
TX = re.compile(r"^\[vsarena\] tx f=(\d+) seat=(\d+) pos=(-?\d+),(-?\d+),(-?\d+) "
                r"v=(-?\d+),(-?\d+),(-?\d+) yaw=(-?\d+) anim=(\d+) cur=(\d+) st=(\d+)", re.M)
RX = re.compile(r"^\[vsarena\] rx seat=(\d+) f=(\d+) t=(\d+) pos=(-?\d+),(-?\d+),(-?\d+) "
                r"v=(-?\d+),(-?\d+),(-?\d+) yaw=(-?\d+) anim=(\d+) st=(\d+)", re.M)
VSPAD = re.compile(r"^\[vs\] f(\d+) slot(\d+) actor=\S+ no=(\d+) char=(\d+) pos=\((-?\d+),(-?\d+),"
                   r"(-?\d+)\) touched=\d+ pad=([0-9a-f]{4})", re.M)
CAMW = re.compile(r"^\[camw\] f(\d+) mode=\S+ at=\((-?[\d.]+),(-?[\d.]+),(-?[\d.]+)\)", re.M)
BODY = re.compile(r"^\[vs\] f(\d+) slot(\d+) actor=\S+ no=\d+ char=\d+ "
                  r"pos=\((-?\d+),(-?\d+),(-?\d+)\)", re.M)
# the relay's referee lines (referee.py log, through relay.py's log())
R_START = re.compile(r"arena (\S+) START seats=\[([\d, ]*)\] map=(\d+) time=(\d+)s first_to=(\d+)")
R_OVER = re.compile(r"arena (\S+) MATCH_OVER reason=(\d+) winners=\[([\d, ]*)\] scores=\[([-\d, ]*)\]")
R_LEFT = re.compile(r"arena (\S+) seat (\d+) left the match \((\d+)\), (\d+) stars spilled")

OVER_NAMES = {1: "time-up", 2: "first-to-N", 3: "last-standing", 4: "everyone-left"}


def mask_seats(m):
    return [k for k in range(16) if m >> k & 1]


def ints(s):
    return [int(x) for x in re.findall(r"-?\d+", s)]


# ---------------------------------------------------------------------------
# lobby digest (the same canonical string port/tools/lobby/app/server.py hashes)
# ---------------------------------------------------------------------------
def fnv1a32(text):
    h = 0x811C9DC5
    for b in text.encode("ascii"):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def lobby_digest(map_, players, target, netmode="arena"):
    """map|players|win|target|king|luigi|seed|netmode|wire (win is stars when a
    star target is set, else time; wire 2 up to 4 players)."""
    text = "%d|%d|%s|%d|0|0|0|%s|%d" % (map_, players, "stars" if target else "time",
                                        target or 0, netmode, 2 if players <= 4 else 3)
    return fnv1a32(text), text


# ---------------------------------------------------------------------------
# facts of one console
# ---------------------------------------------------------------------------
def facts(t):
    f = {"len": len(t)}
    f["faults"] = FAULT.findall(t)
    f["lockstep"] = LOCKSTEP_TELL.findall(t)
    m = INSTALLED.search(t)
    f["installed"] = (int(m.group(1)), int(m.group(2))) if m else None
    m = SEATED.search(t)
    f["seat"] = int(m.group(1)) if m else None
    f["relay_seat_line"] = bool(RELAY_SEAT.search(t))
    f["first_vs"] = bool(FIRST_VS.search(t))
    f["up"] = len(UP.findall(t))
    f["start"] = START.findall(t)
    f["ready"] = READY.findall(t)
    f["over_ref"] = OVER_REF.findall(t)
    f["over_leg"] = OVER_LEG.findall(t)
    f["over_any"] = OVER_ANY.findall(t)
    f["over_arena"] = OVER_ARENA.findall(t)
    f["conn_lost"] = bool(CONN_LOST.search(t))
    f["leave"] = [(int(a), int(b), int(c)) for a, b, c in LEAVE.findall(t)]
    f["join"] = [(int(a), int(b)) for a, b in JOIN.findall(t)]
    f["taken"] = [(int(a), int(b), int(c)) for a, b, c, _, _ in TAKEN.findall(t)]
    f["spill"] = [(int(b), int(d)) for a, b, c, d, e in SPILL.findall(t)]
    f["pup_seats"] = sorted({int(s) for s in PUP_LINE.findall(t)})
    f["pup_takeovers"] = len(PUP_TAKEN.findall(t))
    return f


# ---------------------------------------------------------------------------
# the relay's own record of the match (the authority's side of the cross-check)
# ---------------------------------------------------------------------------
def relay_facts(rt):
    r = {"start": R_START.findall(rt), "over": R_OVER.findall(rt), "left": R_LEFT.findall(rt)}
    r["violations"] = len(re.findall(r"strike|violation", rt, re.I))
    return r


# ---------------------------------------------------------------------------
# the match judge
# ---------------------------------------------------------------------------
def judge_match(texts, rcs, stay, expect, rt=None, lane=None):
    """texts / rcs: {console index: text / exit code}. stay: the consoles that
    play the whole match (a leaver is judged by judge_leave). expect: dict
    reason (1 time-up / 2 first-to-N / None), first_to, n (consoles in the
    match), exit0 (default True). lane: {'A1': bool} what the exe carries.
    -> [(ok, msg)]"""
    lane = lane or {}
    out = []
    F = {k: facts(texts[k]) for k in texts}

    def ok(c, m):
        out.append((bool(c), m))
    seats = {}
    for k in sorted(stay):
        f = F[k]
        if expect.get("exit0", True):
            ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        ok(not f["faults"], "p%d no fault (%s)" % (k, f["faults"][:1] or "-"))
        ok(not f["lockstep"], "p%d no lockstep line (%s)" % (k, f["lockstep"][:1] or "-"))
        ok(f["installed"] is not None, "p%d installed arena mode" % k)
        ok(f["seat"] is not None, "p%d seated" % k)
        if f["seat"] is not None:
            seats[k] = f["seat"]
        if expect.get("seat_line"):
            n_sl = len(RELAY_SEAT.findall(texts[k]))
            ok(n_sl == 1, "p%d printed '[vsarena] the relay named my seat' exactly once (%d): the launcher "
                          "watchdog widens its window on it" % (k, n_sl))
        ok(f["up"] == 1, "p%d exactly one 'session up' (%d)" % (k, f["up"]))
        ok(len(f["start"]) == 1, "p%d exactly one START (%d)" % (k, len(f["start"])))
        ok(not f["conn_lost"], "p%d never lost the referee" % k)
        if expect.get("min_taken"):
            ok(len(f["taken"]) >= expect["min_taken"], "p%d saw the referee award >= %d stars (%d): a match "
               "where nothing is scored proves nothing about stars" % (k, expect["min_taken"], len(f["taken"])))
        ok(len(f["over_ref"]) == 1, "p%d exactly one referee MATCH OVER (%d)" % (k, len(f["over_ref"])))
        ok(len(f["over_leg"]) == 1 and len(f["over_any"]) == 1,
           "p%d exactly one '[vs] MATCH OVER' marker (%d/%d)" % (k, len(f["over_leg"]), len(f["over_any"])))
    ok(len(set(seats.values())) == len(seats), "every console took its own seat %s" % sorted(seats.items()))
    # one verdict everywhere
    refs = {k: F[k]["over_ref"][0] for k in stay if len(F[k]["over_ref"]) == 1}
    legs = {k: (F[k]["over_leg"][0][1], F[k]["over_leg"][0][2], F[k]["over_leg"][0][3])
            for k in stay if len(F[k]["over_leg"]) == 1}
    starts = {k: F[k]["start"][0] for k in stay if len(F[k]["start"]) == 1}
    ok(len(refs) == len(stay) and len(set(refs.values())) == 1,
       "the referee's MATCH OVER (reason, winners, scores, seq) is byte-identical on every console "
       "(%d distinct)" % len(set(refs.values())))
    ok(len(legs) == len(stay) and len(set(legs.values())) == 1,
       "the '[vs] MATCH OVER' marker, frame stripped, is byte-identical on every console "
       "(%d distinct)" % len(set(legs.values())))
    ok(len(starts) == len(stay) and len(set(starts.values())) == 1,
       "START (rules, roster, seed, map) is the same on every console (%d distinct)" % len(set(starts.values())))
    if refs:
        r0 = next(iter(refs.values()))
        reason, wmask, count = int(r0[0]), int(r0[1], 16), int(r0[2])
        scores = [int(x) for x in r0[3:7]]
        win = mask_seats(wmask)
        ok(len(win) == count and count >= 1, "winners mask 0x%x names %d seat(s), count says %d" % (wmask, len(win), count))
        if expect.get("reason"):
            ok(reason == expect["reason"], "match ended by %s (want %s)" % (
                OVER_NAMES.get(reason, reason), OVER_NAMES[expect["reason"]]))
        if reason == 2:
            ok(count == 1, "first-to-N: ONE winner (%s), no two winners" % win)
            ft = expect.get("first_to")
            if ft:
                ok(max(scores) >= ft and sum(1 for s in scores if s >= ft) == 1,
                   "first-to-%d: exactly one seat reached it (scores %s)" % (ft, scores))
        top = max(scores[s] for s in win) if win and max(win) < 4 else None
        if top is not None:
            ok(all(scores[s] <= top for s in range(4) if s not in win),
               "the winners hold the top score (%s, winners %s)" % (scores, win))
        # the legacy marker must say the same winner the referee did
        if legs:
            lg = next(iter(legs.values()))
            w = re.search(r"winner=(-?\d+)", lg[2])
            if w and win:
                if len(win) == 1:
                    ok(int(w.group(1)) in win, "the marker's winner=%s is the referee's winner %s" % (w.group(1), win))
                else:
                    ok(int(w.group(1)) == -1, "a tie between %s: the marker says winner=-1 (draw), it says %s" % (win, w.group(1)))
    # the authority's own record (the relay's referee log) against what the consoles heard
    if rt is not None:
        rf = relay_facts(rt)
        ok(len(rf["over"]) == 1, "the relay's referee ended ONE match (%d MATCH_OVER lines)" % len(rf["over"]))
        if rf["over"] and refs:
            code, reason, winners, rscores = rf["over"][0]
            r0 = next(iter(refs.values()))
            ok(int(reason) == int(r0[0]) and mask_seats(int(r0[1], 16)) == ints(winners),
               "the referee's own log (reason %s winners %s) is what every console heard (reason %s winners %s)"
               % (reason, ints(winners), r0[0], mask_seats(int(r0[1], 16))))
        if rf["start"]:
            ok(len(rf["start"]) == 1, "the relay's referee started ONE match (%d)" % len(rf["start"]))
    # puppets: every console took over the other seats' bodies (A1)
    if lane.get("A1"):
        for k in sorted(stay):
            others = [s for s in seats.values() if s != seats.get(k)]
            if k in seats and others:
                seen = set(F[k]["pup_seats"])
                ok(F[k]["pup_takeovers"] >= 1 and set(others) <= seen | {seats[k]},
                   "p%d drove puppets for seats %s (saw %s, %d takeovers)" % (
                       k, others, sorted(seen), F[k]["pup_takeovers"]))
    return out


# ---------------------------------------------------------------------------
# leave / rejoin
# ---------------------------------------------------------------------------
def ledger_before_leave(t, seat):
    """What the referee's ledger held for `seat` just before its LEAVE: the
    last STAR_TAKEN score for it, minus the hit / hazard spills it suffered
    after that (SPILL lines name the seat the star came off)."""
    score = 0
    for m in re.finditer(r"^\[vsarena-ref\] (STAR_TAKEN inst=\d+ winner=(\d+) score=(-?\d+)|"
                         r"SPILL inst=\d+ seat=(\d+) marker=\d+ cause=(\d+) score=(-?\d+)|"
                         r"LEAVE seat=(\d+))", t, re.M):
        if m.group(2) is not None and int(m.group(2)) == seat:
            score = int(m.group(3))
        elif m.group(4) is not None and int(m.group(4)) == seat and int(m.group(5)) != 2:
            score = int(m.group(6)) if int(m.group(6)) >= 0 else max(0, score - 1)
        elif m.group(7) is not None and int(m.group(7)) == seat:
            break
    return score


def judge_leave(texts, rcs, stay, leaver, rt=None, rejoined=None):
    """The leaver's console went away mid-match. Every staying console: one
    LEAVE for the leaver's seat, naming how many stars were spilled, equal to
    the stars the leaver held per the referee's ledger; the match went on to
    one identical MATCH OVER whose scores hold 0 for the leaver; the relay's
    referee logged the same leave. `rejoined`: the seat the returning console
    took, which every staying console must see JOIN mid-match."""
    out = []

    def ok(c, m):
        out.append((bool(c), m))
    F = {k: facts(texts[k]) for k in texts}
    spilled_all = set()
    held_all = set()
    for k in sorted(stay):
        f = F[k]
        lv = [x for x in f["leave"] if x[0] == leaver]
        ok(len(lv) == 1, "p%d heard ONE LEAVE for seat %d (%d)" % (k, leaver, len(lv)))
        if lv:
            spilled_all.add(lv[0][2])
            held = ledger_before_leave(texts[k], leaver)
            held_all.add(held)
            ok(lv[0][2] == held, "p%d: the leaver's %d star(s) spilled == the %d the ledger held" % (
                k, lv[0][2], held))
        ok(lv and len(f["over_ref"]) == 1, "p%d the match went on to a MATCH OVER after the leave" % k)
        if f["over_ref"]:
            sc = [int(x) for x in f["over_ref"][0][3:7]]
            if rejoined is None:
                ok(sc[leaver] == 0, "p%d: the leaver's final score is 0 (%s)" % (k, sc))
    ok(len(spilled_all) == 1 and len(held_all) == 1,
       "every console agrees on the spill (%s) and on what the leaver held (%s)" % (
           sorted(spilled_all), sorted(held_all)))
    ok(any(h >= 1 for h in held_all), "the leaver held at least one star (a leave with nothing to spill proves nothing)")
    if rt is not None:
        rf = relay_facts(rt)
        lv = [x for x in rf["left"] if int(x[1]) == leaver]
        ok(len(lv) >= 1 and int(lv[0][3]) in spilled_all,
           "the relay's referee logged seat %d leaving with %s stars spilled (consoles: %s)" % (
               leaver, lv[0][3] if lv else "-", sorted(spilled_all)))
    if rejoined is not None:
        for k in sorted(stay):
            jn = [x for x in F[k]["join"] if x == (rejoined, 1)]
            ok(len(jn) >= 1, "p%d saw seat %d JOIN mid-match" % (k, rejoined))
    return out


# ---------------------------------------------------------------------------
# own-body latency, wire, camera
# ---------------------------------------------------------------------------
def judge_latency(texts, rcs, want_zero=True, min_answers=4):
    """Every console's own body answers its press on the press frame: every
    '[vsarena-lat]' answer reads 0 frame(s), at least min_answers of them, no
    'no answer'. want_zero=False is the LOCKSTEP CONTROL: every answer must be
    3 frames or more (the instrument sees a real delay)."""
    out = []
    for k in sorted(texts):
        t = texts[k]
        got = [int(m.group(4)) for m in LAT.finditer(t)]
        modes = {m.group(7) for m in LAT.finditer(t)}
        none = len(NOANS.findall(t))
        out.append((rcs.get(k) == 0 and not FAULT.search(t), "p%d rc=%s faults=%d" % (
            k, rcs.get(k), len(FAULT.findall(t)))))
        if want_zero:
            out.append((len(got) >= min_answers and all(g == 0 for g in got) and none == 0,
                        "p%d own body answers in 0 frames (%s, no-answer %d, netmode %s)" % (
                            k, got, none, ",".join(sorted(modes)) or "-")))
        else:
            out.append((len(got) >= min_answers and all(g >= 3 for g in got),
                        "CONTROL p%d lockstep answers 3+ frames later (%s, netmode %s)" % (
                            k, got, ",".join(sorted(modes)) or "-")))
    return out


def judge_wire(texts, rcs, n):
    """The mesh row (A0): every SYNS a console logged as received equals, field
    for field, the line its sender logged for that frame; sequences are
    monotonic; only my own slot ever reads a non-zero pad; every console heard
    every other seat."""
    out = []
    tx, seat_of = {}, {}
    for k in sorted(texts):
        m = SEATED.search(texts[k])
        seat = int(m.group(1)) if m else -1
        seat_of[k] = seat
        tx[seat] = {}
        for mm in TX.finditer(texts[k]):
            tx[seat][int(mm.group(1))] = tuple(int(mm.group(i)) for i in (3, 4, 5, 6, 7, 8, 9, 10, 12))
    for k in sorted(texts):
        t = texts[k]
        me = seat_of[k]
        rx_by, bad = {}, 0
        for mm in RX.finditer(t):
            s, f = int(mm.group(1)), int(mm.group(2))
            rec = tuple(int(mm.group(i)) for i in (4, 5, 6, 7, 8, 9, 10, 11, 12))
            rx_by.setdefault(s, []).append(f)
            if tx.get(s, {}).get(f) != rec:
                bad += 1
        foreign = mine = 0
        for mm in VSPAD.finditer(t):
            slot, pad = int(mm.group(2)), int(mm.group(8), 16)
            if slot == me and pad:
                mine += 1
            elif slot != me and slot < n and pad:
                foreign += 1
        mono = all(v == sorted(v) and len(v) == len(set(v)) for v in rx_by.values())
        heard = {s: len(v) for s, v in sorted(rx_by.items())}
        out.append((rcs.get(k) == 0 and not FAULT.search(t), "p%d rc=%s faults=%d" % (k, rcs.get(k), len(FAULT.findall(t)))))
        out.append((bad == 0 and mono, "p%d every received SYNS == the sender's logged one (mismatch %d, monotonic %s)" % (k, bad, mono)))
        out.append((foreign == 0 and mine > 0, "p%d only my own slot reads a pad (mine %d, foreign %d)" % (k, mine, foreign)))
        out.append((len(heard) == n - 1 and all(c > 0 for c in heard.values()),
                    "p%d heard every other seat %s" % (k, heard)))
    return out


def camera_votes(t):
    """-> (slot the camera follows, votes, frames judged). The camera's 'at'
    point against every body's position on that frame: the nearest body is the
    one it films."""
    bodies = collections.defaultdict(dict)
    for f, s, x, y, z in BODY.findall(t):
        bodies[int(f)][int(s)] = (int(x) / 4096.0, int(y) / 4096.0, int(z) / 4096.0)
    votes = collections.Counter()
    nf = 0
    for f, x, y, z in CAMW.findall(t):
        f = int(f)
        if f < 150 or f not in bodies or len(bodies[f]) < 2:
            continue
        at = (float(x), float(y), float(z))
        best = min(bodies[f], key=lambda s: sum((bodies[f][s][i] - at[i]) ** 2 for i in range(3)))
        votes[best] += 1
        nf += 1
    return (votes.most_common(1)[0][0] if votes else -1), dict(votes), nf


def body_positions(t):
    """slot -> distinct positions its body took on THIS console (the [vs] probe)."""
    pos = collections.defaultdict(set)
    for f, s, x, y, z in BODY.findall(t):
        pos[int(s)].add((x, y, z))
    return {s: len(v) for s, v in pos.items()}


def judge_camera(texts, rcs, min_frames=100, min_move=100, others_move=False):
    """Each console films its OWN slot (the field bug: the child filmed slot 0), and
    its OWN body moves (>= min_move distinct positions on its own screen: the
    lockstep rungs' 'a motionless body cannot pass' rule; a frozen joiner body
    passes a camera check on its own); with others_move (puppets landed) every
    other slot's body moves on this screen too."""
    out = []
    seats = []
    for k in sorted(texts):
        t = texts[k]
        m = SEATED.search(t)
        me = int(m.group(1)) if m else -1
        seats.append(me)
        bp = body_positions(t)
        out.append((bp.get(me, 0) >= min_move,
                    "p%d own body (slot %d) moved on its own screen: %d distinct positions (>= %d)" % (
                        k, me, bp.get(me, 0), min_move)))
        if others_move:
            low = {s: c for s, c in bp.items() if s != me and c < min_move // 2}
            out.append((not low and len(bp) >= 2, "p%d sees every other body move (distinct positions %s)" % (
                k, {s: c for s, c in sorted(bp.items())})))
        follows, votes, nf = camera_votes(t)
        out.append((rcs.get(k) in (0, None) and not FAULT.search(t), "p%d rc=%s faults=%d" % (k, rcs.get(k), len(FAULT.findall(t)))))
        out.append((me == k and follows == me and nf >= min_frames,
                    "p%d camera films its own slot: seat %d, camera follows slot %d over %d frames %s" % (
                        k, me, follows, nf, votes)))
    out.append((len(set(seats)) == len(seats), "one seat per console %s" % seats))
    return out


# ---------------------------------------------------------------------------
# solo alone in a world
# ---------------------------------------------------------------------------
def judge_solo_alone(text, rc):
    f = facts(text)
    return [
        (rc == 0, "alone console rc=%s" % rc),
        (not f["faults"], "no fault (%s)" % (f["faults"][:1] or "-")),
        (f["installed"] is not None and f["seat"] is not None, "installed + seated"),
        # A lone console's READY is accepted by the referee ("session up ... (READY accepted)", REVSTARREF1 item: session up
        # prints on READY-accepted or START, whichever is first), so the rule is no START and no MATCH OVER, not no "session up".
        (not f["start"] and not f["over_ref"] and not f["over_leg"],
         "no match starts with one console (START %d, MATCH OVER %d, session up %d)" % (len(f["start"]), len(f["over_leg"]), f["up"])),
        (not f["conn_lost"], "no 'connection lost' while merely waiting"),
    ]


# ---------------------------------------------------------------------------
# MUST-FAIL CONTROLS
# ---------------------------------------------------------------------------
def mutate(kind, texts):
    """Copy of texts with ONE thing broken; the rung demands its judge goes red.
    Returns (name, texts) or None when the real logs have nothing to break."""
    t = dict(texts)
    ks = sorted(t)
    if not ks:
        return None
    k = ks[-1]
    if kind == "two-winners":
        m = OVER_REF.search(t[k])
        if not m:
            return None
        w = int(m.group(2), 16) ^ 0x3
        t[k] = t[k][:m.start(2)] + "%04x" % w + t[k][m.end(2):]
        return "winners mask differs on p%d" % k, t
    if kind == "second-marker":
        m = OVER_LEG.search(t[k])
        if not m:
            return None
        t[k] = t[k] + "\n[vs] MATCH OVER f%d win=time-up scores=0,0,0,0 players=2 total=0 pending_scene=0 results_screen=port winner=9\n" % (int(m.group(1)) + 1)
        return "a second, different MATCH OVER marker on p%d" % k, t
    if kind == "deaf":
        t[k] = OVER_REF.sub("", t[k])
        return "p%d never heard the referee's MATCH OVER" % k, t
    if kind == "lost":
        t[k] = t[k] + "\n[vsarena] connection lost: no MATCH OVER from the referee 3000 ms after the clock ran out\n"
        return "p%d reports 'connection lost'" % k, t
    if kind == "late-body":
        if not LAT.search(t[k]):
            return None
        t[k] = LAT.sub(lambda m: m.group(0).replace(": %s frame(s)" % m.group(4), ": 4 frame(s)"), t[k])
        return "p%d answers 4 frames late" % k, t
    if kind == "camera":
        if not CAMW.search(t[k]):
            return None
        # point p(last)'s camera at slot 0 by swapping the seat it believes it has
        t[k] = SEATED.sub("[vsarena] seated: seat 0 is my body", t[k])
        return "p%d believes it is seat 0 (the camera follows another slot)" % k, t
    if kind == "wire":
        m = RX.search(t[k])
        if not m:
            return None
        s = m.group(0)
        t[k] = t[k].replace(s, s.replace("pos=%s," % m.group(4), "pos=%d," % (int(m.group(4)) + 7), 1), 1)
        return "one SYNS received differs from the one sent on p%d" % k, t
    if kind == "no-spill":
        m = LEAVE.search(t[k])
        if not m:
            return None
        t[k] = t[k][:m.start(3)] + str(int(m.group(3)) + 1) + t[k][m.end(3):]
        return "p%d's LEAVE spills one star too many" % k, t
    return None


# ---------------------------------------------------------------------------
# SELFTEST (fixtures built here; every judge and every control)
# ---------------------------------------------------------------------------
def _console(seat, n, reason=2, winners=1, scores=(3, 1, 0, 0), frame=700, up=True, leave=None,
             lat=None, cam=False, pup=True):
    L = ["[vsarena] installed: arena mode, %d bodies, seat pin %d (test), hub open." % (n, seat),
         "[vsarena] seated: seat %d is my body (data_020a0f10 = %d), %d bodies" % (seat, seat, n),
         "[vsarena] first VS frame: seat %d, %d bodies (ROM count %d, me %d)" % (seat, n, n, seat)]
    if up:
        L += ["[vsarena] session up players=%d (START)" % n,
              "[vsarena] START t0=%d first_to=3 time=30s roster=0x%04x markers=4 seed=77 map=0" % (1000 + seat * 17, (1 << n) - 1)]
    L += ["[vsarena-ref] STAR_TAKEN inst=1 winner=0 score=1 claim=5 progress=1 stamp=1",
          "[vsarena-ref] STAR_TAKEN inst=2 winner=1 score=1 claim=5 progress=2 stamp=1",
          "[vsarena-ref] STAR_TAKEN inst=3 winner=0 score=2 claim=6 progress=3 stamp=1"]
    if pup:
        for s in range(n):
            if s != seat:
                L += ["[pup] t=100 f=200 seat=%d ft=1.0 mode=i pos=1,2,3 yaw=0 anim=1 cur=0 buf=50 newest=1" % s,
                      "[pup] seat %d taken over: Behavior skipped" % s]
    if leave:
        L += ["[vsarena-ref] LEAVE seat=%d why=1 spilled=%d" % leave]
    if up:
        L += ["[vs] f%d ARENA MATCH OVER from the referee: reason=%d winners=0x%04x scores %d,%d,%d,%d" % (
            frame, reason, winners, *scores),
            "[vsarena] MATCH OVER ref reason=%d winners=0x%04x count=%d scores=%d,%d,%d,%d seq=9" % (
                reason, winners, bin(winners).count("1"), *scores),
            "[vs] MATCH OVER f%d win=%s scores=%d,%d,%d,%d players=%d total=%d pending_scene=0 "
            "results_screen=port winner=%d winner_name=p%d" % (
                frame, "star-target" if reason == 2 else "time-up", *scores, n, sum(scores),
                (winners.bit_length() - 1) if bin(winners).count("1") == 1 else -1,
                (winners.bit_length() - 1) if bin(winners).count("1") == 1 else -1)]
    if lat is not None:
        for i, d in enumerate(lat):
            L.append("[vsarena-lat] press A at tick %d, my body (slot %d) answered at tick %d: %d frame(s) "
                     "(state 15 -> 18) netmode=%s" % (240 + 60 * i, seat, 240 + 60 * i + d, d, "arena" if d == 0 else "lockstep"))
    if cam:
        for f in range(200, 330):
            for s in range(n):
                L.append("[vs] f%d slot%d actor=0 no=0 char=3 pos=(%d,0,0) touched=0 pad=0000" % (f, s, (s + 1) * 100 * 4096 + f * 4096))
            L.append("[camw] f%d mode=a at=(%d.0,0.0,0.0)" % (f, (seat + 1) * 100 + f))
    return "\n".join(L) + "\n"


def old_return_outcome(tj):
    """Which clean ending the returning console reached against the OLD relay
    (the one that names a returner a seat while the dead holder still owns it):
    'refused' (seat has no body -> ERROR + exit 4), 'seated' (the old relay
    idle-expired the dead holder and the console got a seat) or None."""
    if re.search(r"^\[vsarena\] ERROR: seat \d+ has no body", tj, re.M):
        return "refused"
    if SEATED.search(tj):
        return "seated"
    return None


def judge_old_return(tj, rc, seat):
    """The returning console against the OLD relay accepts exactly two clean
    results, and either one is judged hard.
    (a) refused: exactly one '[vsarena] ERROR: seat N has no body' line, exit 4,
        no VS frame loop, no seat taken.
    (b) seated: the old relay idle-expired the dead holder and the console got
        ITS OWN seat back: seated once in `seat`, exit 0, one referee MATCH OVER,
        no 'has no body' line.
    Anything else (a fault or crash, another seat, two MATCH OVERs, exit codes
    that fit neither ending) is red."""
    out = []

    def ok(c, m):
        out.append((bool(c), m))
    which = old_return_outcome(tj)
    ok(which is not None, "the returning console reached a clean ending: refused (ERROR seat N has no body) "
                          "or seated in its own seat (%s)" % (which or "neither"))
    ok(not FAULT.search(tj), "the returning console had no fault or crash text")
    if which == "refused":
        n_err = len(re.findall(r"^\[vsarena\] ERROR: seat \d+ has no body", tj, re.M))
        ok(n_err == 1, "refused: exactly one '[vsarena] ERROR: seat N has no body' line (%d)" % n_err)
        ok(rc == 4, "refused: left with exit code 4 (rc=%s)" % rc)
        ok(not re.search(r"first VS frame", tj), "refused: never entered the VS frame loop")
        ok(not SEATED.search(tj), "refused: was never seated")
    elif which == "seated":
        seats = [int(x) for x in SEATED.findall(tj)]
        ok(seats == [seat], "seated: seated once, in its own seat %d (%s)" % (seat, seats))
        ok(rc == 0, "seated: exited 0 (rc=%s)" % rc)
        n_over = len(re.findall(r"^\[vsarena\] MATCH OVER ref", tj, re.M))
        ok(n_over == 1, "seated: heard exactly one referee MATCH OVER (%d)" % n_over)
        ok(not re.search(r"seat \d+ has no body", tj), "seated: no 'has no body' line")
    return out


def selftest():
    n_ok = 0
    bad = []

    def expect(name, cond):
        nonlocal n_ok
        if cond:
            n_ok += 1
        else:
            bad.append(name)

    def red(items):
        return any(not ok for ok, _ in items)
    # 2P first-to-3, seat 0 wins
    T = {0: _console(0, 2), 1: _console(1, 2)}
    R = {0: 0, 1: 0}
    rt = ("relay: arena AB START seats=[0, 1] map=0 time=30s first_to=3\n"
          "relay: arena AB MATCH_OVER reason=2 winners=[0] scores=[3, 1]\n")
    good = judge_match(T, R, [0, 1], {"reason": 2, "first_to": 3}, rt=rt, lane={"A1": True})
    expect("match good", not red(good))
    for kind in ("two-winners", "second-marker", "deaf", "lost"):
        m = mutate(kind, T)
        expect("match control " + kind, m is not None and red(judge_match(m[1], R, [0, 1], {"reason": 2, "first_to": 3}, rt=rt)))
    # two winners in first-to-N
    T2 = {0: _console(0, 2, scores=(3, 3, 0, 0), winners=3), 1: _console(1, 2, scores=(3, 3, 0, 0), winners=3)}
    expect("first-to-N with two winners is red", red(judge_match(T2, R, [0, 1], {"reason": 2, "first_to": 3})))
    # time up, tie allowed (reason 1)
    T3 = {0: _console(0, 2, reason=1, winners=3, scores=(2, 2, 0, 0)), 1: _console(1, 2, reason=1, winners=3, scores=(2, 2, 0, 0))}
    expect("time-up tie is one identical verdict", not red(judge_match(T3, R, [0, 1], {"reason": 1})))
    # latency
    TL = {0: _console(0, 2, lat=[0, 0, 0, 0, 0, 0]), 1: _console(1, 2, lat=[0, 0, 0, 0, 0, 0])}
    expect("latency zero", not red(judge_latency(TL, R)))
    m = mutate("late-body", TL)
    expect("latency control", m is not None and red(judge_latency(m[1], R)))
    TLK = {0: _console(0, 2, lat=[4, 4, 4, 4, 5, 4]), 1: _console(1, 2, lat=[4, 4, 4, 4, 4, 4])}
    expect("lockstep control reads 3+", not red(judge_latency(TLK, R, want_zero=False)))
    expect("lockstep control fails on zero", red(judge_latency(TL, R, want_zero=False)))
    # camera
    TC = {0: _console(0, 2, cam=True), 1: _console(1, 2, cam=True)}
    expect("camera good", not red(judge_camera(TC, R)))
    m = mutate("camera", TC)
    expect("camera control", m is not None and red(judge_camera(m[1], R)))
    TWRONG = {0: _console(0, 2, cam=True),
              1: re.sub(r"(\[camw\] f\d+ mode=a at=\()(\d+)", lambda m: m.group(1) + str(int(m.group(2)) - 100),
                        _console(1, 2, cam=True))}
    expect("camera on the wrong slot is red", red(judge_camera(TWRONG, R)))
    # leave: p2 held 2 stars when it left
    held_log = ("[vsarena-ref] STAR_TAKEN inst=4 winner=2 score=1 claim=1 progress=1 stamp=1\n"
                "[vsarena-ref] STAR_TAKEN inst=5 winner=2 score=2 claim=2 progress=2 stamp=1\n")
    # the LEAVE line must come after the stars: rebuild by hand
    TV = {}
    for k in (0, 1):
        base = _console(k, 3, reason=1, winners=1, scores=(2, 0, 0, 0))
        base = base.replace("[vsarena-ref] STAR_TAKEN inst=1", held_log + "[vsarena-ref] STAR_TAKEN inst=1", 1)
        base = base.replace("[vs] f700 ARENA", "[vsarena-ref] LEAVE seat=2 why=1 spilled=2\n[vs] f700 ARENA", 1)
        TV[k] = base
    rtv = "arena AB seat 2 left the match (1), 2 stars spilled\narena AB MATCH_OVER reason=1 winners=[0] scores=[2, 0]\n"
    good = judge_leave(TV, {0: 0, 1: 0}, [0, 1], 2, rt=rtv)
    expect("leave good", not red(good))
    m = mutate("no-spill", TV)
    expect("leave control", m is not None and red(judge_leave(m[1], {0: 0, 1: 0}, [0, 1], 2, rt=rtv)))
    # wire
    def wire(seat, other):
        L = ["[vsarena] seated: seat %d is my body" % seat]
        for f in range(10, 40):
            L.append("[vsarena] tx f=%d seat=%d pos=%d,0,0 v=1,0,0 yaw=0 anim=1 cur=0 st=15" % (f, seat, f * 10 + seat))
            L.append("[vsarena] rx seat=%d f=%d t=5 pos=%d,0,0 v=1,0,0 yaw=0 anim=1 st=15" % (other, f, f * 10 + other))
            L.append("[vs] f%d slot%d actor=0 no=0 char=3 pos=(0,0,0) touched=0 pad=%s" % (f, seat, "0040"))
        return "\n".join(L)
    TW = {0: wire(0, 1), 1: wire(1, 0)}
    expect("wire good", not red(judge_wire(TW, {0: 0, 1: 0}, 2)))
    m = mutate("wire", TW)
    expect("wire control", m is not None and red(judge_wire(m[1], {0: 0, 1: 0}, 2)))
    # solo alone
    solo = _console(0, 2, up=False, pup=False)
    expect("solo alone good", not red(judge_solo_alone(solo, 0)))
    expect("solo alone with a MATCH OVER is red", red(judge_solo_alone(_console(0, 2), 0)))
    # digest vectors from ARENALOBBY1 contract section 2
    expect("digest vectors", lobby_digest(0, 2, 0)[0] == 1565304087 and lobby_digest(1, 4, 3)[0] == 1476757693
           and lobby_digest(3, 3, 5)[0] == 1991793976 and lobby_digest(0, 2, 0, "lockstep")[0] == 1172137709)
    # returner against the OLD relay: two clean endings, everything else red
    REF = "[vsarena] installed\n[vsarena] ERROR: seat 1 has no body\n"
    OVR = "[vsarena] MATCH OVER ref reason=1 winners=0x1 count=4 x\n"
    SEAT = "[vsarena] installed\n[vsarena] seated: seat 1 is my body\n[vs] first VS frame\n" + OVR
    expect("old return refused is green", not red(judge_old_return(REF, 4, 1)))
    expect("old return seated is green", not red(judge_old_return(SEAT, 0, 1)))
    expect("old return refused rc 0 is red", red(judge_old_return(REF, 0, 1)))
    expect("old return refused + fault is red", red(judge_old_return(REF + "FAULT c0000005\n", 4, 1)))
    expect("old return refused twice is red", red(judge_old_return(REF + REF, 4, 1)))
    expect("old return seated in another seat is red", red(judge_old_return(SEAT, 0, 2)))
    expect("old return seated rc 1 is red", red(judge_old_return(SEAT, 1, 1)))
    expect("old return seated + fault is red", red(judge_old_return(SEAT + "FAULT c0000005\n", 0, 1)))
    expect("old return two MATCH OVERs is red", red(judge_old_return(SEAT + OVR, 0, 1)))
    expect("old return neither ending is red", red(judge_old_return("[vsarena] installed\n", 0, 1)))
    print("arena_logs selftest: %d checks ok, %d failed %s" % (n_ok, len(bad), bad))
    return 0 if not bad else 1


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(selftest())
    if "--digest" in sys.argv:
        a = sys.argv[sys.argv.index("--digest") + 1:]
        d, text = lobby_digest(int(a[0]), int(a[1]), int(a[3]), "arena")
        print(d, text)
        sys.exit(0)
    print(__doc__)
