#!/usr/bin/env python3
"""THE PIT-DEATH RUNG: a VS player walks off the arena into a pit (run mp1, lane PITCRASH1).

    python port/tools/vs_pit_rung.py --lock <ladder lockfile> --map M --n N
                                     [--fallers 0,1] [--key 0x0040] [--frames F]
                                     [--match] [--kit-root DIR] [--exe EXE]
                                     [--transport loop|relay1|relay2] [--out DIR]
                                     [--name NAME] [--expect-deaths K]

WHY. On test build 3 your OWN pit death in a VS match crashed your game
(c0000005 after "[wipe] WRONG RECEIVER at SetForwardTime"). Every other VS rung
keeps the players on the ground: mp_ladder's VS rungs walk the four arena
directions with a toggling pad, the match rungs end on the first star. No rung
had ever sent a body off the edge, and a remote body's pit death takes a
different path (no wipe on that screen), so a faller had to be ON the console
under test.

WHAT IT DOES. N headless consoles in one session (mp_ladder's own Run, vs_env,
lock and relay), every console idle except the fallers, whose pad is held on
one direction (SM64DS_COMMS_INJECT: the DS key register, so the press crosses
the wire like a player's) until they walk off the map, die in the pit, respawn
and walk off again. With --match the arena's blocks break and a late star ends
the match (MATCH OVER), so a pit death lands mid-match and the match still has
to end the same way everywhere.

VERDICT, per console: rc 0, no FAULT, no "[wipe] WRONG RECEIVER" line, session
up and lasted the run; on EVERY console every faller's body went down the pit
and came back (a respawn: one frame that lifts it more than 5000 units from
below the arena floor) at least --expect-deaths times; the respawn frames are
the same on every console; each faller's own console ran its own wipe
("[wipe] SetToStart on wipe 6"); the dhdiff sweep over every pairing reports NO
DIVERGENCE; --match adds one identical MATCH OVER line per console.

WHICH MAP. --map 2 (level 29) has the pits: holding up from any of its four
starts walks off the edge within ~800 frames. On maps 0, 1 and 3 (levels 51,
43, 42) straight walks in all four directions from every start never took a
body below its floor (run mp1, lane PITCRASH1's survey), so no rung there
reaches a pit death.

--kit-root runs the kit's own exe from a player-shaped install in the
launcher's shape (no SM64DS_COMMS_FANOUT, no SM64DS_FAULTS_FATAL, the default
net mode), as mp_ladder's kit rung does.
"""

import argparse
import collections
import hashlib
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mp_ladder as ML    # noqa: E402

VS_POS = re.compile(r"^\[vs\] f(\d+) slot(\d+) actor=\S+ no=\d+ char=\d+ "
                    r"pos=\((-?\d+),(-?\d+),(-?\d+)\)", re.M)
WIPE6 = "[wipe] SetToStart on wipe 6"
WRONG = "[wipe] WRONG RECEIVER"


def respawns(t, slot):
    """Frames on which this slot's body was lifted more than 5000 units in one
    frame from well below where it started: St_Respawn_Init's teleport to the
    spawn point (+0x3e8000) after St_DeadPit. Positions are 20.12. The body
    must have been at least 2000 units under its first reported height the
    frame before, and land within 20000 units of it: the probe rows after the
    last scripted frame carry junk positions (a 200000-unit "jump" on f900 of
    a 900-frame run on level 51, with the body on the ground the frame
    before), and those are not a pit death."""
    ys = {}
    for m in VS_POS.finditer(t):
        if int(m.group(2)) == slot:
            ys[int(m.group(1))] = int(m.group(4)) >> 12
    if not ys:
        return [], None
    start = ys[min(ys)]
    out, prev = [], None
    for f in sorted(ys):
        y = ys[f]
        if (prev is not None and y - prev[1] > 5000 and f - prev[0] <= 2
                and prev[1] < start - 2000 and abs(y - start) < 20000):
            out.append(f)
        prev = (f, y)
    return out, min(ys.values())


def r_pit(L, run, r):
    n = r.n
    o = r.opts
    frames = r.frames or 1500
    relay = run.start_relay()
    kit = o.get("kit_root")
    exe = os.path.join(kit, "walk_window.exe") if kit else None
    specs = []
    for k in range(n):
        d, e = ML.vs_env(run, k, n, frames, relay, r.paced, r.rtt,
                         netmode=None if kit else "lockstep", match=o["match"],
                         inject=False, exe=exe, asset_root=kit)
        e["SM64DS_VS_MAP"] = str(o["map"])
        e["SM64DS_VS_STATE_HASH"] = "1"
        e["SM64DS_SELFTEST_IDLE"] = "1"
        if o["match"]:
            # A late star: the pit deaths come first, then the match ends on it.
            e["SM64DS_STAR_TRIGGER"] = o.get("star", "1200,1280")
        if k in o["fallers"]:
            e["SM64DS_COMMS_INJECT"] = "key=0x%04x" % o["key"]
        if kit:
            e.pop("SM64DS_COMMS_FANOUT", None)
            e.pop("SM64DS_FAULTS_FATAL", None)
        e["SM64DS_VS_NAMES"] = ",".join(["pit%d" % i for i in range(n)] +
                                        [""] * ((4 if n <= 4 else 16) - n))
        specs.append((k, e, d, 0.0 if k == 0 else 1.0 + 0.3 * k))
    rcs, texts, wall = run.launch(specs, ML.timeout_for(n, frames, r.paced, 90))
    cs = {k: ML.parse(t) for k, t in texts.items()}
    ck = ML.Checks()
    for k in range(n):
        c, t = cs[k], texts[k]
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        ck.ok(not c["faults"], "p%d faults=%d %s" % (k, len(c["faults"]),
                                                     (c["faults"][:1] or [""])[0][:120]))
        ck.ok(WRONG not in t, "p%d no '[wipe] WRONG RECEIVER' line (%d)" % (k, t.count(WRONG)))
        ML.session_up(ck, k, c, n)
        if k in o["fallers"]:
            ck.ok(WIPE6 in t, "p%d (a faller) ran its own pit wipe: '%s' x%d" % (k, WIPE6, t.count(WIPE6)))
    seen = {}
    for s in o["fallers"]:
        per = {}
        for k in range(n):
            fr, low = respawns(texts[k], s)
            per[k] = fr
            ck.ok(len(fr) >= o["deaths"], "p%d saw slot %d fall and respawn %d time(s) (want >= %d; "
                  "lowest y %s; frames %s)" % (k, s, len(fr), o["deaths"], low, fr[:6]))
        lists = {tuple(v) for v in per.values()}
        ck.ok(len(lists) == 1, "slot %d's respawn frames identical on every console (%d distinct)"
              % (s, len(lists)))
        seen[s] = per
    if o["match"]:
        for k in range(n):
            ck.ok(len(cs[k]["match"]) == 1, "p%d one MATCH OVER line (%d)" % (k, len(cs[k]["match"])))
        lines = {c["match"][0] for c in cs.values() if c["match"]}
        ck.ok(len(lines) == 1, "MATCH OVER identical on every console (%d distinct)" % len(lines))
        if lines:
            ck.note(next(iter(lines))[:140])
        end = ML.match_frame(cs)
        delays = [c["close"]["indelay"] for c in cs.values() if c["close"]]
        ML.lasted(ck, cs, range(n), end, margin=10 + 2 * max(delays or [0]))
        ok, msg = ML.sweep(texts, list(range(n)), 24, run.dir, want=end)
    else:
        ML.lasted(ck, cs, range(n), frames)
        delays = sorted({c["close"]["indelay"] for c in cs.values() if c["close"]})
        margin = max(24, 2 * (delays[-1] if delays else 0) + 8)
        ok, msg = ML.sweep(texts, list(range(n)), margin, run.dir, want=frames)
    ck.ok(ok, msg)
    ML.relay_checks(ck, run)
    ck.note("map %d fallers %s key 0x%04x respawns %s" % (
        o["map"], ",".join(map(str, o["fallers"])), o["key"],
        " ".join("s%d:%s" % (s, seen[s].get(0, [])[:6]) for s in seen)))
    if kit:
        ck.note("kit exe md5 %s" % hashlib.md5(open(exe, "rb").read()).hexdigest())
    return ck, wall, rcs


ML.BODIES["pit"] = r_pit


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", default=ML.DEFAULT_EXE)
    ap.add_argument("--asset-root")
    ap.add_argument("--kit-root", help="a player-shaped install (its walk_window.exe, build/assets, "
                    "extracted); runs in the launcher's shape")
    ap.add_argument("--lock", required=True)
    ap.add_argument("--map", type=int, required=True)
    ap.add_argument("--n", type=int, default=2)
    ap.add_argument("--fallers", default="0")
    ap.add_argument("--key", default="0x0040")
    ap.add_argument("--frames", type=int, default=1500)
    ap.add_argument("--match", action="store_true")
    ap.add_argument("--star", default="1200,1280")
    ap.add_argument("--transport", default="loop", choices=("loop", "relay1", "relay2"))
    ap.add_argument("--rtt", type=int, default=0)
    ap.add_argument("--paced", action="store_true")
    ap.add_argument("--expect-deaths", type=int, default=1)
    ap.add_argument("--name")
    ap.add_argument("--out")
    ap.add_argument("--relay-py", default=ML.RELAY_PY)
    ap.add_argument("--relay1-py")
    ap.add_argument("--busy-log", default=os.environ.get("SM64DS_BUSY_LOG", ""))
    ap.add_argument("--hold-s", type=float, default=0)
    ap.add_argument("--busy-wait", type=float, default=0)
    ap.add_argument("--lock-timeout", type=float, default=7200)
    a = ap.parse_args()
    a.minpos, a.kit, a.rom, a.kit_work = 0, None, None, None
    if a.kit_root:
        a.exe = os.path.join(a.kit_root, "walk_window.exe")
        a.asset_root = a.kit_root
    L = ML.Ladder(a)
    os.makedirs(L.out, exist_ok=True)
    fallers = [int(x) for x in a.fallers.split(",") if x != ""]
    name = a.name or "pit%d-map%d-f%s%s-%s" % (a.n, a.map, "".join(map(str, fallers)),
                                               "-match" if a.match else "", a.transport)
    r = ML.Rung(name, "pit", a.n, ("dev",), a.transport, a.rtt, a.paced, a.frames,
                map=a.map, fallers=fallers, key=int(a.key, 0), match=a.match, star=a.star,
                deaths=a.expect_deaths, kit_root=os.path.abspath(a.kit_root) if a.kit_root else None)
    feats = ML.Features(L.exe, L.relay_py, [])
    verdict, wall, detail, rcs = L.run_rung(r, 0, feats)
    L.record(r, verdict, wall, detail, rcs)
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
