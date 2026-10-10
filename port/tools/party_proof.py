#!/usr/bin/env python3
"""PBODY1 rows (run mp1 wave 3): solid party bodies across 2 to 4 consoles through a LOCAL relay copy, started the
way the launcher starts them (title path, the adventure env + SM64DS_ROOM_KIND=party + SM64DS_PARTY=1, no
SM64DS_LEVEL; a seeded save; SM64DS_WARP_SEQ only moves a console to a level at a chosen frame).

This file REPLACES the 2026-09 rollback-era party proof (shared rollback world, retired by Tango's 10-07 ruling).

It rides the run's console rig (lgc.py / r3gw.py / deskspawn.py: a private desktop, hidden and muted, ports from this
process's PID). Point PBODY_RIG at the directory holding them. Environment:
  PBODY_EXE     the exe to run (default: the rig's)      PBODY_RELAY  relay.py (default: this tree's)
  PBODY_OUT     where the console folders go             PBODY_SHOTS  where pictures go
  PBODY_ASSET_ROOT  a kit install's asset root (kit rows)
  PBODY_LOCK    ladder lock file to hold while consoles run (default none)

  python party_proof.py room:<kind>:<n>:<tag>[:k=v;k=v]       kind party | coop | world
     options  frames=1200  scale=1|2|3  warp<i>=6@300  star<i>=1  char<i>=0..3  pad<i>=<HOST_PAD script, ';' for ','>
              env_<NAME>=v (every console)  env<i>_<NAME>=v (console i only)  hold<i>=<frames> (console i runs that long)
     (the judges below are chosen with judge=<name>)
"""
import ctypes
import os
import re
import sys
import time

RIG = os.environ.get("PBODY_RIG", "C:/tmp/pbody1/scripts")
sys.path.insert(0, RIG)
import lgc as G  # noqa: E402
import r3gw as R  # noqa: E402
import deskspawn as D  # noqa: E402,F401

if os.environ.get("PBODY_ASSET_ROOT"):
    G.ROOT = os.environ["PBODY_ASSET_ROOT"]

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
R.RELAY = os.environ.get("PBODY_RELAY", os.path.join(HERE, "relay", "relay.py"))
SHOTS = os.environ.get("PBODY_SHOTS", os.path.join(os.getcwd(), "shots"))
os.makedirs(SHOTS, exist_ok=True)
say, info = G.say, G.info

BODY_R = 40.0          # the Player body cylinder radius, units
RADSUM = 2 * BODY_R


def kv(s):
    return dict(x.split("=", 1) for x in s.split(";") if x)


def env_for(i, relay, kind, o):
    e = G.hubenv(i, {}, relay=relay)
    e["GC_SAVE"] = G.SEED
    e["SM64DS_ROOM_KIND"] = kind
    if kind == "party":
        e["SM64DS_PARTY"] = "1"
    if kind == "coop":
        e["SM64DS_COOP"] = "1"
    e["SM64DS_PRESENCE_LOG"] = "1"
    e["SM64DS_PACE_SELFTEST"] = "1"      # real-time pacing: the relay and the ghost rings run in wall-clock time
    e["SM64DS_PLAYER_NAME"] = ("Tango", "Opie", "Kirb", "MoMo")[i]
    e["SM64DS_PLAYER_COLOR"] = ("e04040", "40c060", "e0c030", "60a0f0")[i]
    w = o.get("warp%d" % i, o.get("warp", "1@300"))
    if w:
        e["SM64DS_WARP_SEQ"] = w
    st = o.get("star%d" % i, o.get("star"))
    if st:
        e["SM64DS_STAR_FILTER"] = st
    if o.get("scale"):
        e["SM64DS_RENDER_SCALE"] = o["scale"]
    sw = o.get("char%d" % i, ("", "0", "1", "2")[i])
    if sw != "":
        e["SM64DS_SWITCH"] = sw
    pad = o.get("pad%d" % i)
    tap = o.get("tap%d" % i, o.get("tap"))     # tap=<from>-<to>-<every>: A taps (a course opens with a message box that needs them)
    if tap:
        a, b, k = [int(v) for v in tap.split("-")]
        taps = "+".join("1000@%d-%d" % (f, f + 4) for f in range(a, b, k))
        pad = (pad + "+" + taps) if pad else taps
    if pad:
        e["SM64DS_SELFTEST_IDLE"] = "1"
        e["SM64DS_HOST_PAD"] = pad.replace(";", ",").replace("+", ",")
    else:
        e["SM64DS_SELFTEST_IDLE"] = "1"
    if o.get("log", "1") != "0":
        e["SM64DS_PARTY_LOG"] = "1"
    if o.get("sep", "1") != "0":
        e["SM64DS_PARTY_SEP"] = "1"
    for k, v in o.items():
        if k.startswith("env_"):
            e[k[4:]] = v
        m = re.match(r"env(\d)_(.*)$", k)
        if m and int(m.group(1)) == i:
            e[m.group(2)] = v
    return e


def lock():
    p = os.environ.get("PBODY_LOCK", "")
    if not p:
        return None
    os.environ["SM64DS_BUILD_LOCK_PATH"] = p
    sys.path.insert(0, os.path.join(TREE, "port", "tools"))
    import build_lock
    build_lock.acquire(label="pbody1-rows", root=TREE, timeout=14400)
    return build_lock, p


def run_room(kind, n, tag, o):
    frames = int(o.get("frames", "1200"))
    row = "%s_%s" % (kind, tag)
    rp, rf, port, rlog = R.start_relay(row)
    relay = ("127.0.0.1:%d" % port, "PB%d" % (G.PID % 1000))
    cons = []
    for i in range(n):
        e = env_for(i, relay, kind, o)
        if kind == "world":
            e.pop("SM64DS_PARTY", None)
            e.pop("SM64DS_COOP", None)
        fr = int(o.get("hold%d" % i, frames))
        cons.append(G.Con(row, "p%d" % i, e, fr, exe=os.environ.get("PBODY_EXE")))
        time.sleep(0.5)
    G.finish(cons, 1700)
    R.stop_relay(rp, rf)
    return row, cons


def shot(con, tag):
    from PIL import Image
    src = os.path.join(con.dir, "walk_window_selftest.bmp")
    if not os.path.exists(src):
        info("no picture for %s" % tag)
        return None
    png = os.path.join(SHOTS, "%s.png" % tag)
    Image.open(src).save(png)
    info("picture %s (bmp md5 %s)" % (png, G.md5(src)[:8]))
    return png


# ---------------------------------------------------------------------------
# parsers
# ---------------------------------------------------------------------------
SEP = re.compile(r"^\[partysep\] t=(\d+) seat=(\d+) body=(\d+) horiz=(-?\d+)\.(\d+) dy=(-?\d+) "
                 r"me=\((-?\d+),(-?\d+),(-?\d+)\) p=\((-?\d+),(-?\d+),(-?\d+)\) reg=(\d) push=\((-?\d+),(-?\d+),(-?\d+)\) hit=([0-9a-f]+) own=(\d+)", re.M)


def partysep(t):
    """[(t, seat, body, horiz, dy, (mx,my,mz), (px,py,pz), reg)]"""
    out = []
    for m in SEP.finditer(t):
        g = m.groups()
        out.append((int(g[0]), int(g[1]), int(g[2]), int(g[3]) + int(g[4]) / 100.0, int(g[5]),
                    (int(g[6]), int(g[7]), int(g[8])), (int(g[9]), int(g[10]), int(g[11])), int(g[12]), (int(g[13]), int(g[14]), int(g[15])), int(g[16], 16), int(g[17])))
    return out


LAB = re.compile(r"^\[partylab\] k=(\d+) cls=0x([0-9a-f]+) uid=(\d+) pos=\((-?\d+),(-?\d+),(-?\d+)\) "
                 r"coins (-?\d+)>(-?\d+) stars (-?\d+)>(-?\d+) red (-?\d+)>(-?\d+) actors (-?\d+)>(-?\d+) "
                 r"level (-?\d+)>(-?\d+) alive=(\d) (\S+)", re.M)


def lablines(t):
    return [m.groups() for m in LAB.finditer(t)]


def party_lines(t, pat):
    return re.findall(r"(?m)^%s.*$" % pat, t)


# ---------------------------------------------------------------------------
# rows
# ---------------------------------------------------------------------------
def row_room(kind, n, tag, o):
    L = lock()
    try:
        row, cons = run_room(kind, n, tag, o)
    finally:
        if L:
            L[0].release(L[1])
    texts = [c.text() for c in cons]
    for i, c in enumerate(cons):
        info("%s console %d rc=%s faults=%d exe=%s" % (row, i, c.rc, len(G.faults(texts[i])), c.exe_md5[:8]))
        if o.get("shot", "1") != "0":
            shot(c, "%s_p%d" % (row, i))
    say(all(c.rc == 0 and not G.faults(t) for c, t in zip(cons, texts)), "%s: every console exited 0, no fault" % row)
    return row, cons, texts


def judge_spike(row, texts, o):
    """The spike: console 0 ran the lab (sweep or far). Report what changed, the audit and the guard."""
    t = texts[0]
    ll = lablines(t)
    done = party_lines(t, r"\[partylab\] DONE")
    changed = [x for x in ll if x[-1] == "CHANGED"]
    info("%s: %d sweep lines, %d CHANGED, DONE=%s" % (row, len(ll), len(changed), done[:1]))
    for x in changed[:20]:
        info("%s: CHANGED k=%s cls=0x%s uid=%s pos=(%s,%s,%s) coins %s>%s stars %s>%s red %s>%s actors %s>%s level %s>%s alive=%s"
             % ((row,) + x[:17]))
    sh = party_lines(t, r"\[partycyl\] SHARED")
    info("%s: audit SHARED lines: %d %s" % (row, len(sh), sh[:3]))
    st = party_lines(t, r"\[party\] ")
    info("%s: last party stats: %s" % (row, st[-1] if st else None))
    return ll, changed, done, sh


def main(argv):
    for a in argv:
        p = a.split(":")
        if p[0] == "room":
            kind, n, tag = p[1], int(p[2]), p[3]
            o = kv(":".join(p[4:])) if len(p) > 4 else {}
            row, cons, texts = row_room(kind, n, tag, o)
            j = o.get("judge", "")
            if j == "spike":
                judge_spike(row, texts, o)
            for i, t in enumerate(texts):
                ps = partysep(t)
                if ps:
                    hs = [x[3] for x in ps if abs(x[4]) < 150]
                    info("%s p%d: %d [partysep] lines, min horizontal separation (|dy|<150) %.2f units"
                         % (row, i, len(ps), min(hs) if hs else -1))
    bad = [r for r in G.RES if r.startswith("FAIL")]
    print("\n%d PASS %d FAIL" % (len([r for r in G.RES if r.startswith("PASS")]), len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
