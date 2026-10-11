#!/usr/bin/env python3
"""Log analysis for run mp1 lane BOSSCORE1 (coop shared targeting and bosses).

Reads the stderr logs of two or more consoles of one coop room and answers the proof questions:

  use     <log>...                 the seat ClosestPlayer returned for each logged actor slot, per console, in order
  agree   <log>... [--cls N]       per slot: the LAST seat is equal on every console; prints the switch wall times
  hold    <log>... [--min-ms 900]  the decider's "switch ... after Nms" lines: none under the hold
  boss    <log>... [--max-pos 120] pair the [cbs] samples of the boss across consoles by wall time: position gap, HP gap
  hp      <log>...                 the HP of the boss at the end on every console (and the first time it dropped)
  stats   <log>                    the last [cotgtstat] / [cbsstat] lines

Exit code 0 = the check passed, 1 = it failed, 2 = no data.
"""
import re
import sys

USE = re.compile(r"^\[cotgt\] use seat=(-?\d+) slot=(\d+) cls=(\d+) via=(\w+) tick=(\d+) wall=(\d+)", re.M)
SWITCH = re.compile(r"^\[cotgt\] (switch|pick) slot=(\d+) cls=(\d+) (-?\d+)->(-?\d+)(?: after (\d+)ms| \(no hold\)) tick=(\d+) wall=(\d+)", re.M)
CBS = re.compile(r"^\[cbs\] w=(\d+) t=(\d+) slot=(\d+) cls=(\d+) pos=(-?\d+),(-?\d+),(-?\d+) auth=(-?\d+) me=(-?\d+) hp=(-?\d+) st=(\d+)(.*)$", re.M)
HITLINE = re.compile(r"^\[cbs\] (hit|hit-rx|apply-hp|apply-state|authority|defeated)(.*)$", re.M)


def text(p):
    return open(p, encoding="utf-8", errors="replace").read()


def uses(t):
    out = {}
    for m in USE.finditer(t):
        seat, slot, cls, via, tick, wall = m.groups()
        out.setdefault((int(slot), int(cls)), []).append((int(wall), int(seat), via))
    return out


def cmd_use(logs):
    for p in logs:
        u = uses(text(p))
        print("== %s" % p)
        for (slot, cls), seq in sorted(u.items()):
            print("  slot %d cls %d: %s" % (slot, cls, " ".join("%d(%s)" % (s, v) for _, s, v in seq)))
    return 0


def cmd_agree(logs, cls=None):
    per = [uses(text(p)) for p in logs]
    keys = set()
    for u in per:
        keys |= {k for k in u if cls is None or k[1] == cls}
    bad = 0
    shown = 0
    for k in sorted(keys):
        lasts = [(u[k][-1][1] if k in u else None) for u in per]
        have = [x for x in lasts if x is not None]
        ok = len(have) < 2 or len(set(have)) == 1
        if not ok:
            bad += 1
        if (not ok) or shown < 12:
            shown += 1
            print("  slot %d cls %d last seats %s %s" % (k[0], k[1], lasts, "OK" if ok else "DISAGREE"))
    print("agree: %d slots compared, %d disagree" % (len(keys), bad))
    return 0 if keys and bad == 0 else (2 if not keys else 1)


def cmd_hold(logs, min_ms=900):
    n = 0; bad = 0
    for p in logs:
        for m in SWITCH.finditer(text(p)):
            if m.group(1) != "switch":
                continue
            n += 1
            after = int(m.group(6))
            if after < min_ms:
                bad += 1
                print("  %s: switch slot %s %s->%s after only %d ms" % (p, m.group(2), m.group(4), m.group(5), after))
            else:
                print("  %s: switch slot %s cls %s %s->%s after %d ms (wall %s)" % (p, m.group(2), m.group(3), m.group(4), m.group(5), after, m.group(8)))
    print("hold: %d switches, %d under %d ms" % (n, bad, min_ms))
    return 2 if n == 0 else (1 if bad else 0)


def samples(t, cls=None):
    out = {}
    for m in CBS.finditer(t):
        w, tk, slot, c, x, y, z, auth, me, hp, st, rest = m.groups()
        if cls is not None and int(c) != cls:
            continue
        out.setdefault(int(slot), []).append(dict(w=int(w), pos=(int(x), int(y), int(z)), auth=int(auth), me=int(me), hp=int(hp), st=int(st), rest=rest))
    return out


def cmd_boss(logs, max_pos=120, cls=None):
    S = [samples(text(p), cls) for p in logs]
    slots = set()
    for s in S:
        slots |= set(s)
    worst = 0.0; worst_hp = 0; pairs = 0; hp_bad = 0
    for slot in sorted(slots):
        a = S[0].get(slot, [])
        for j in range(1, len(S)):
            b = S[j].get(slot, [])
            for sa in a:
                best = None
                for sb in b:
                    d = abs(sb["w"] - sa["w"])
                    if best is None or d < best[0]:
                        best = (d, sb)
                if best is None or best[0] > 300:
                    continue
                pairs += 1
                sb = best[1]
                dist = sum((p - q) ** 2 for p, q in zip(sa["pos"], sb["pos"])) ** 0.5
                worst = max(worst, dist)
                if sa["hp"] != sb["hp"]:
                    hp_bad += 1
    print("boss: %d sample pairs, worst position gap %.0f units, hp differing in %d pairs" % (pairs, worst, hp_bad))
    if pairs == 0:
        return 2
    return 0 if worst <= max_pos else 1


def cmd_hp(logs, cls=None):
    ends = []
    for p in logs:
        t = text(p)
        s = samples(t, cls)
        last = {slot: v[-1]["hp"] for slot, v in s.items()}
        first_drop = {}
        for slot, v in s.items():
            h0 = v[0]["hp"]
            for x in v:
                if x["hp"] < h0:
                    first_drop[slot] = x["w"]
                    break
        print("  %s: final hp %s, first drop wall %s" % (p, last, first_drop))
        ends.append(last)
    ok = True
    if len(ends) > 1:
        for slot in ends[0]:
            vals = [e.get(slot) for e in ends if slot in e]
            if len(set(vals)) > 1:
                ok = False
    print("hp: %s" % ("equal on every console" if ok else "DIFFER"))
    return 0 if ok and ends and ends[0] else (2 if not ends or not ends[0] else 1)


def cmd_stats(logs):
    for p in logs:
        t = text(p)
        for tag in ("[cotgtstat]", "[cbsstat]"):
            ls = [ln for ln in t.splitlines() if ln.startswith(tag)]
            print("%s %s" % (p, ls[-1] if ls else tag + " none"))
    return 0


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    cmd = sys.argv[1]
    args = sys.argv[2:]
    opts = {}
    logs = []
    i = 0
    while i < len(args):
        if args[i] in ("--cls", "--min-ms", "--max-pos"):
            opts[args[i][2:]] = int(args[i + 1]); i += 2
        else:
            logs.append(args[i]); i += 1
    if cmd == "use":
        return cmd_use(logs)
    if cmd == "agree":
        return cmd_agree(logs, opts.get("cls"))
    if cmd == "hold":
        return cmd_hold(logs, opts.get("min-ms", 900))
    if cmd == "boss":
        return cmd_boss(logs, opts.get("max-pos", 120), opts.get("cls"))
    if cmd == "hp":
        return cmd_hp(logs, opts.get("cls"))
    if cmd == "stats":
        return cmd_stats(logs)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
