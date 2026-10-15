#!/usr/bin/env python3
"""Judges the logs of a coop world run (hal/coop_world.cpp) the way the COOPWORLD1 proof rows do.

A console started with SM64DS_COOP=1 SM64DS_COOP_LOG=1 prints, to its log:
  [coopbits] <why> level=L act=A part=P n=N bits=<16 hex words>    the death-table part of the group it is in
  [coop] census level=L act=A actors=T slotted=S classes: id:n ...   placed actors by class (40 / 300 / 800 frames in)
  [coop] joined level L act A: ...                                   first in, or a snapshot applied before spawn
  [coopstat] seat=.. owner=.. est=.. ... applied=.. recon=.. local=.. drop bad=.. group=.. nonce=.. sub=..
The proof knobs (inert unless set) that drive a run: SM64DS_COOP_KILL=F:CLASS:N, SM64DS_COOP_FORGE=F:name,
SM64DS_COOP_WARP=F:LEVEL:ACT, SM64DS_COOP_SYNC=0 (the must-fail control: apply nothing received).

  python coop_world_proof.py equal   A.log B.log [--level 6]   final bitmaps equal and non-empty (live sync)
  python coop_world_proof.py late    A.log B.log [--level 6]   B's FIRST bitmap (the latch, before spawn) equals A's final
                                                              one, and B's first census lacks exactly A's kills
  python coop_world_proof.py differ  A.log B.log [--level 6]   the sync-off control: A has bits, B has none
  python coop_world_proof.py refused A.log [--bad N --group N --nonce N --sub N]   forged input counted, table untouched
Exit 0 = every check held. Reads logs only; starts nothing.
"""
import re
import sys

BITS = re.compile(r"(?m)^\[coopbits\] (\w+) level=(-?\d+) act=(-?\d+) part=(-?\d+) n=(\d+) bits=([0-9a-f ]+)$")
CENSUS = re.compile(r"(?m)^\[coop\] census level=(\d+) act=(\d+) actors=(\d+) slotted=(\d+) classes:(.*)$")
STAT = re.compile(r"(?m)^\[coopstat\] (.*)$")


def read(p):
    with open(p, errors="replace") as f:
        return f.read()


def bits(t, level, why=None):
    return [m for m in BITS.finditer(t) if m.group(2) == str(level) and (why is None or m.group(1) == why)]


def first_census(t, level):
    m = [c for c in CENSUS.findall(t) if c[0] == str(level)]
    return int(m[0][3]) if m else None


def stat(t):
    m = STAT.findall(t)
    d = {}
    for tok in (m[-1].split() if m else []):
        if "=" in tok:
            k, v = tok.split("=", 1)
            d[k] = v
    return d


def check(ok, msg):
    print(("PASS  " if ok else "FAIL  ") + msg)
    return bool(ok)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    mode = argv[1]
    level = 6
    opts = {}
    args = []
    i = 2
    while i < len(argv):
        if argv[i].startswith("--"):
            opts[argv[i][2:]] = int(argv[i + 1])
            i += 2
        else:
            args.append(argv[i])
            i += 1
    level = opts.get("level", level)
    ta = read(args[0])
    ok = True
    if mode == "refused":
        d = stat(ta)
        for k in ("bad", "group", "nonce", "sub"):
            if k in opts:
                ok &= check(int(d.get(k, -1)) >= opts[k], "%s refused and counted >= %d (got %s)" % (k, opts[k], d.get(k)))
        b = bits(ta, level)
        ok &= check(bool(b) and b[-1].group(5) == "0" and int(d.get("applied", -1)) == 0, "table untouched (n=%s applied=%s)" % (b and b[-1].group(5), d.get("applied")))
        return 0 if ok else 1
    tb = read(args[1])
    la, lb = bits(ta, level), bits(tb, level)
    if not la or not lb:
        print("FAIL  no [coopbits] lines for level %d (SM64DS_COOP_LOG=1 set?)" % level)
        return 1
    if mode == "equal":
        ok &= check(la[-1].group(6) == lb[-1].group(6) and int(la[-1].group(5)) > 0,
                    "final bitmaps equal, n=%s / %s" % (la[-1].group(5), lb[-1].group(5)))
    elif mode == "differ":
        ok &= check(int(la[-1].group(5)) > 0 and int(lb[-1].group(5)) == 0, "control: A n=%s, B n=%s" % (la[-1].group(5), lb[-1].group(5)))
    elif mode == "late":
        eb = bits(tb, level, "established")
        ok &= check(bool(eb) and eb[0].group(6) == la[-1].group(6) and int(eb[0].group(5)) > 0,
                    "B's first bitmap (at the latch) equals A's final one, n=%s" % (eb and eb[0].group(5)))
        sa, sb = first_census(ta, level), first_census(tb, level)
        ok &= check(sa is not None and sb is not None and sa - sb == int(la[-1].group(5)),
                    "B's first census lacks exactly A's %s kills (A %s, B %s)" % (la[-1].group(5), sa, sb))
    else:
        print("unknown mode", mode)
        return 2
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
