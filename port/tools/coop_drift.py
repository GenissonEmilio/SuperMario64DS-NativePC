#!/usr/bin/env python3
"""coop_drift.py: proof tool for coop phase 2 (lane COOPMOVE1, run mp1).

Reads the per-console logs the game writes with SM64DS_COOP_MOVE_SAMPLE / _DUMP / _LOG and answers the questions the
rows ask. Pure log analysis: it starts no process and needs no ROM.

  coop_drift.py drift  LOG_A LOG_B [--settle MS] [--from-wall MS] [--to-wall MS] [--json]
        Pair every synced actor by its death-table SLOT (identical on every console for one level and act) and compare
        the pose of B with A's pose interpolated to B's sample wall time. Prints, per class, the position error
        (world units) and the angle error (16-bit units) as p50 / p95 / max, and the M1 verdict: p95 <= 24 units for
        path movers and rotators, <= 80 for enemies, angles <= 0x800 (p95).
  coop_drift.py first  LOG_A LOG_B [--frames N] [--skip K]
        M2: the error of B's samples from tick K to K+N after its first sample (the snapshot lands a few ticks in, inside the fade), per class (late joiner in band at its first frame).
  coop_drift.py fields RUN1 RUN2 [--min-off 0x318] [--enemy-min-off 0x110] [--max 12] [--out FILE]
        Phase-field discovery from two runs' [mvd] dumps (SM64DS_COOP_MOVE_DUMP=a,b): a word is a phase field when it
        changes between the two ticks of a run and has the same value in both runs at the same tick (so it is neither a
        constant nor a heap pointer). Writes a SM64DS_COOP_MOVE_PROFILE file (id period band snap flags nf off:size ...).
  coop_drift.py stats  LOG...
        The last [movestat] line of each log.
"""
import argparse
import collections
import json
import math
import re
import sys

# Class ids that chase, bounce and decide (the 80 unit band). Everything else that is synced is a mover on a fixed route.
ENEMY = {200, 201, 202, 206, 219, 220, 337, 270, 230, 312, 255, 232, 215, 260, 237, 223, 310}   # 220 = the pendulum iron balls (2000 units/s at the bottom): judged as enemies
ROUTE_BAND_P95 = 24.0
ENEMY_BAND_P95 = 80.0
ANGLE_P95 = 0x800

MVS = re.compile(r"^\[mvs\] w=(\d+) lf=(\d+) seat=(-?\d+)(.*)$")


def load_samples(path):
    """slot -> list of (wall_ms, x, y, z, ay, az, cls), sorted by wall time."""
    out = collections.defaultdict(list)
    with open(path, errors="replace") as f:
        for ln in f:
            m = MVS.match(ln)
            if not m:
                continue
            w = int(m.group(1))
            for tok in m.group(4).split():
                p = tok.split(":")
                if len(p) != 7:
                    continue
                slot, cls, x, y, z, ay, az = (int(v) for v in p)
                out[slot].append((w, x, y, z, ay, az, cls))
    for v in out.values():
        v.sort()
    return out


def adiff(a, b):
    d = (a - b) & 0xFFFF
    return 0x10000 - d if d > 0x8000 else d


def interp(series, t, max_gap=400):
    """Pose of a slot at wall time t from the two samples around it (None when there is no pair or the gap is wide)."""
    lo, hi = 0, len(series) - 1
    if not series or t < series[0][0] or t > series[-1][0]:
        return None
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if series[mid][0] <= t:
            lo = mid
        else:
            hi = mid - 1
    a = series[lo]
    b = series[min(lo + 1, len(series) - 1)]
    if b[0] - a[0] > max_gap:
        return None
    if b[0] == a[0]:
        return a
    k = (t - a[0]) / float(b[0] - a[0])
    return (t, a[1] + (b[1] - a[1]) * k, a[2] + (b[2] - a[2]) * k, a[3] + (b[3] - a[3]) * k, a[4], a[5], a[6])


def pct(v, q):
    v = sorted(v)
    if not v:
        return 0.0
    return v[min(len(v) - 1, int(round(q * (len(v) - 1))))]


def compare(A, B, t0, t1, same_cls=True):
    per = collections.defaultdict(lambda: ([], []))
    for slot, sb in B.items():
        sa = A.get(slot)
        if not sa:
            continue
        for s in sb:
            if s[0] < t0 or s[0] > t1:
                continue
            a = interp(sa, s[0])
            if a is None or (same_cls and a[6] != s[6]):
                continue
            d = math.sqrt((a[1] - s[1]) ** 2 + (a[2] - s[2]) ** 2 + (a[3] - s[3]) ** 2)
            ang = max(adiff(a[4], s[4]), adiff(a[5], s[5]))
            per[s[6]][0].append(d)
            per[s[6]][1].append(ang)
    return per


def report(per, label):
    print("== %s" % label)
    print("  %-5s %6s  %8s %8s %8s   %7s %7s" % ("class", "n", "p50", "p95", "max", "ang p95", "ang max"))
    bad = []
    for cls in sorted(per, key=lambda c: -pct(per[c][0], 0.95)):
        d, a = per[cls]
        p95 = pct(d, 0.95)
        band = ENEMY_BAND_P95 if cls in ENEMY else ROUTE_BAND_P95
        ok = p95 <= band and pct(a, 0.95) <= ANGLE_P95
        if not ok:
            bad.append((cls, p95, pct(a, 0.95)))
        print("  %-5d %6d  %8.1f %8.1f %8.1f   %7d %7d  %s" % (cls, len(d), pct(d, 0.5), p95, max(d), pct(a, 0.95), max(a),
                                                               "ok" if ok else "OVER (%d)" % band))
    allp = [x for c in per for x in per[c][0]]
    return bad, allp


def cmd_drift(args):
    A, B = load_samples(args.a), load_samples(args.b)
    if not A or not B:
        print("FAIL  no [mvs] samples (A %d slots, B %d slots)" % (len(A), len(B)))
        return 2
    firstA = min(s[0][0] for s in A.values())
    firstB = min(s[0][0] for s in B.values())
    t0 = max(firstA, firstB) + args.settle
    if args.from_wall:
        t0 = max(t0, args.from_wall)
    lastA = max(s[-1][0] for s in A.values())
    lastB = max(s[-1][0] for s in B.values())
    t1 = min(lastA, lastB)
    if args.to_wall:
        t1 = min(t1, args.to_wall)
    print("A first %d last %d | B first %d last %d | window %d..%d (%.1f s)" % (firstA, lastA, firstB, lastB, t0, t1, (t1 - t0) / 1000.0))
    per = compare(A, B, t0, t1)
    bad, allp = report(per, "B vs A at B's sample times")
    n = sum(len(per[c][0]) for c in per)
    if args.json:
        print(json.dumps({c: {"n": len(per[c][0]), "p50": pct(per[c][0], .5), "p95": pct(per[c][0], .95), "max": max(per[c][0])} for c in per}))
    if n < args.min_pairs:
        print("FAIL  only %d paired samples (need %d)" % (n, args.min_pairs))
        return 2
    movers = [c for c in per if c not in ENEMY]
    enemies = [c for c in per if c in ENEMY]
    print("%s  movers: %d classes, enemies: %d classes, %d paired samples" % ("PASS" if not bad else "FAIL", len(movers), len(enemies), n))
    for c, p95, a in bad:
        print("      over band: class %d p95 %.1f units, angle p95 %d" % (c, p95, a))
    return 0 if not bad else 1


def cmd_first(args):
    A, B = load_samples(args.a), load_samples(args.b)
    firstB = min(s[0][0] for s in B.values())
    t0 = firstB + args.skip * 1000 // 60
    per = compare(A, B, t0, t0 + args.frames * 1000 // 60 + 1)
    bad, allp = report(per, "B's first %d frames vs A" % args.frames)
    print("%s  first-frames: %d paired samples, worst %.1f units" % ("PASS" if allp and not bad else "FAIL", len(allp), max(allp) if allp else -1))
    return 0 if allp and not bad else 1


MVD = re.compile(r"^\[mvd\] lv=(\d+) lf=(\d+) slot=(\d+) cls=(\d+) w=([0-9a-f]+)$")


def load_dumps(path):
    """(level, cls, slot, tick) -> list of 32-bit words (offset 0xd0 + 4*i)"""
    d = {}
    with open(path, errors="replace") as f:
        for ln in f:
            m = MVD.match(ln.strip())
            if m:
                h = m.group(5)
                d[(int(m.group(1)), int(m.group(4)), int(m.group(3)), int(m.group(2)))] = [int(h[i:i + 8], 16) for i in range(0, len(h) - 7, 8)]
    return d


def cmd_fields(args):
    R1, R2 = load_dumps(args.run1), load_dumps(args.run2)
    ticks = sorted({k[3] for k in R1})
    if len(ticks) < 2:
        print("FAIL  need dumps at two ticks, got %s" % ticks)
        return 2
    ta, tb = ticks[0], ticks[-1]
    votes = collections.defaultdict(lambda: collections.Counter())   # cls -> Counter(word index)
    slots = collections.Counter()
    for (lv, cls, slot, tk), w in R1.items():
        if tk != ta:
            continue
        wb = R1.get((lv, cls, slot, tb))
        a2, b2 = R2.get((lv, cls, slot, ta)), R2.get((lv, cls, slot, tb))
        if not (wb and a2 and b2):
            continue
        slots[cls] += 1
        for i in range(min(len(w), len(wb), len(a2), len(b2))):
            if w[i] != wb[i] and w[i] == a2[i] and wb[i] == b2[i]:
                votes[cls][i] += 1
    prof = []
    for cls in sorted(votes):
        minoff = args.enemy_min_off if cls in ENEMY else args.min_off
        picks = []
        for i, n in sorted(votes[cls].items()):
            off = 0xd0 + 4 * i
            if off < minoff or n * 2 < slots[cls]:       # varies in at least half of the class's actors
                continue
            picks.append(off)
        print("class %-4d actors %-3d varying words: %s" % (cls, slots[cls], " ".join("0x%x" % o for o in picks) or "-"))
        if len(picks) > args.max:
            print("   %d varying words, keeping the first %d" % (len(picks), args.max))
            picks = picks[:args.max]
        # width: a word that only ever holds a small value is written as a whole word anyway (4); narrow widths are a manual edit
        period = 100 if cls in ENEMY else 1000
        band, snap = (40, 400) if cls in ENEMY else (8, 150)
        if picks:
            prof.append("%d %d %d %d 1 %d %s" % (cls, period, band, snap, len(picks), " ".join("0x%x:4" % o for o in picks)))
    if args.out:
        with open(args.out, "w") as f:
            f.write("# generated by coop_drift.py fields from %s %s\n" % (args.run1.split("/")[-1], args.run2.split("/")[-1]))
            f.write("\n".join(prof) + "\n")
        print("wrote %d rows to %s" % (len(prof), args.out))
    return 0


def cmd_stats(args):
    for p in args.logs:
        last = None
        with open(p, errors="replace") as f:
            for ln in f:
                if ln.startswith("[movestat]"):
                    last = ln.strip()
        print("%s: %s" % (p, last))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    d = sub.add_parser("drift"); d.add_argument("a"); d.add_argument("b")
    d.add_argument("--settle", type=int, default=4000); d.add_argument("--from-wall", type=int, default=0)
    d.add_argument("--to-wall", type=int, default=0); d.add_argument("--min-pairs", type=int, default=200)
    d.add_argument("--json", action="store_true"); d.set_defaults(fn=cmd_drift)
    f = sub.add_parser("first"); f.add_argument("a"); f.add_argument("b"); f.add_argument("--frames", type=int, default=6); f.add_argument("--skip", type=int, default=4)
    f.set_defaults(fn=cmd_first)
    p = sub.add_parser("fields"); p.add_argument("run1"); p.add_argument("run2")
    p.add_argument("--min-off", type=lambda s: int(s, 0), default=0x318)
    p.add_argument("--enemy-min-off", type=lambda s: int(s, 0), default=0x110)
    p.add_argument("--max", type=int, default=12); p.add_argument("--out", default="")
    p.set_defaults(fn=cmd_fields)
    s = sub.add_parser("stats"); s.add_argument("logs", nargs="+"); s.set_defaults(fn=cmd_stats)
    a = ap.parse_args()
    sys.exit(a.fn(a))


if __name__ == "__main__":
    main()
