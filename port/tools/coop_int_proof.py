#!/usr/bin/env python3
"""COOP INTEGRATION PROOF (run mp1, lane COOPINT1): save + boot + world together, 4 consoles.

Real game processes started the way the launcher starts a coop room (the title path, SM64DS_COOP*, no SM64DS_LEVEL,
a NEW coop file), through a local relay copy, quiet and muted through mp2_proof.env_base, ports from this process id,
kill by exact pid. Built on coop_boot_proof.py (Room) and coop_save_proof.py (Con, relay, images).

  int4    host + 2 guests boot a NEW coop file together and play the opening; after it all three warp to Bob-omb Battlefield
          (act 3). Guest 1 kills a Goomba and three coins, touches the PowerStar and saves a star (the game's own
          CollectStar + SaveCurrentFile); the host and guest 2 must lose the same actors; a 4th console joins late and
          must not see them; every console ends on its frame cap and leaves a picture.
  int4q   the same, then the host process is killed after the 4th console is in: the three guests exit with code 7 and
          the host coop image still holds the star.

  python coop_int_proof.py --exe NEW.exe --base BASE.exe [--root ASSETROOT] [--out DIR] row [row ...]
"""
import argparse
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import coop_save_proof as CS  # noqa: E402
import coop_boot_proof as CB  # noqa: E402

say, info = CS.say, CS.info
RES = CS.RES
BOB, ACT = 6, 3
GOOMBA, COIN, STAR = 200, 288, 178
SAVE_STAR = (1, 2)          # the star the save probe records (course 1, bit 2)
BITS = re.compile(r"(?m)^\[coopbits\] (\w+) level=(-?\d+) act=(-?\d+) part=(-?\d+) n=(\d+) bits=([0-9a-f ]+)$")


def bits(t, why=None, level=BOB):
    return [(m.group(1), int(m.group(5)), m.group(6)) for m in BITS.finditer(t)
            if int(m.group(2)) == level and (why is None or m.group(1) == why)]


def reconciled(t):
    return re.findall(r"(?m)^\[coop\] reconcile kill slot=(\d+) actor=(\d+)", t)


def run(row, quit_host):
    n_open = int(os.environ.get("CI_OPEN_FRAMES", "5200"))
    cap = n_open + int(os.environ.get("CI_WORLD_FRAMES", "3000"))
    common = {"SM64DS_COOP_LOG": "1", "SM64DS_COOP_WARP": "150:%d:%d" % (BOB, ACT), "SM64DS_INTRO_WATCH": "1"}
    g1 = dict(common, SM64DS_COOP_KILL="60:%d:1;65:%d:3" % (GOOMBA, COIN), SM64DS_COOP_STAR="200",
              SM64DS_COOPSAVE_PROBE="star:%d:%d@%d" % (SAVE_STAR[0], SAVE_STAR[1], n_open + 1800))
    extras = {0: dict(common), 1: g1, 2: dict(common)}
    rm = CB.Room(row, 3, "new", cap, extras=extras, gaps={1: 0.5, 2: 0.5})
    rm.host_frames = cap + 4500
    rm.start()
    cons = rm.cons
    c4 = None
    dl = time.time() + 1700
    host_killed = None
    while time.time() < dl:
        for c in cons:
            c.poll()
        if c4 is None and "star collected" in cons[1].text() and "[coopsave] PROBE" in cons[1].text():
            time.sleep(4)
            ex = dict(common)
            gt = os.path.join(CB.ARGS.out, row, "guesttemp3")
            os.makedirs(gt, exist_ok=True)
            ex["SM64DS_COOP_TEMP_ROOT"] = gt
            c4 = CS.Con(row, "guest3", CB.ARGS.exe, CB.benv(3, rm.relay, False, None, ex), 3200, CB.ARGS.root, CB.ARGS.out)
            cons.append(c4)
        if quit_host and c4 is not None and host_killed is None and ("joined level %d" % BOB) in c4.text() and \
                "snapshot from seat" in c4.text():
            time.sleep(10)
            host_killed = time.time()
            cons[0].kill()
            info("%s: killed the host (pid %d) 10 s after the 4th console held the snapshot" % (row, cons[0].pid))
        if c4 is not None and all(c.poll() is not None for c in (cons[1:] if host_killed else cons)):
            break
        time.sleep(0.2)
    for c in cons:
        c.poll()
    sav = rm.sav
    rm.stop()
    h, a, b = cons[0], cons[1], cons[2]
    names = {0: "host", 1: "guest1(actor)", 2: "guest2", 3: "guest4(late)"}
    for i, c in enumerate(cons):
        info("%s %s rc=%s exe=%s faults=%d" % (row, names[i], c.rc, CS.sha(c.exe), c.text().count("FAULT code")))
    # 1. boot: a new file, the opening together
    play = [CB.first(c, r"the opening is playing here") for c in cons[:3]]
    rel2 = []
    for c in cons[:3]:
        rr = [t for t, s in CB.lines(c) if s.startswith("RELEASED")]
        rel2.append(rr[1] if len(rr) > 1 else None)
    say(all(play), "%s: the new file's opening played on all three starting consoles" % row)
    ok2 = all(x is not None for x in rel2)
    say(ok2 and max(rel2) - min(rel2) < 1000, "%s: all three entered the grounds together (end release spread %s ms)" % (row, (max(rel2) - min(rel2)) if ok2 else "?"))
    # 2. world: guest 1 acts
    ta, tb, tc = a.text(), b.text(), h.text()
    ka = re.findall(r"(?m)^\[coop-knob\] kill class=(\d+) slot=(\d+)", ta)
    lb = re.findall(r"(?m)^\[coop\] local bit slot=(\d+)", ta)
    star = re.findall(r"(?m)^\[coop\] star collected: class=(\d+) slot=(-?\d+)", ta)
    info("%s guest1 kills %s, local bits %s, star collected %s" % (row, ka, lb, star))
    say(len(ka) >= 4 and len(lb) >= 4, "%s: guest 1 killed a Goomba and three coins (%d kills, %d bits announced)" % (row, len(ka), len(lb)))
    say(bool(star) and "star touch armed" in ta, "%s: guest 1 touched the PowerStar (the star's own collect handler: %s)" % (row, star[:2]))
    probe = re.findall(r"(?m)^\[coopsave\] PROBE star course (\d+) bit (\d+) collected.*ok=(\d+)", ta)
    say(bool(probe) and probe[0][2] == "1", "%s: guest 1 saved star %s through the game's own CollectStar + SaveCurrentFile %s" % (row, SAVE_STAR, probe))
    killed_slots = set(int(x) for x in lb)
    fin = {}
    for name, t in (("g1", ta), ("host", tc), ("g2", tb)):
        bb = bits(t)
        fin[name] = bb[-1] if bb else None

    def has(hexwords, slot):
        w = hexwords.split()
        return bool(int(w[slot >> 5], 16) >> (slot & 31) & 1)
    for name in ("host", "g2"):
        f = fin[name]
        miss = [s_ for s_ in sorted(killed_slots) if not (f and has(f[2], s_))]
        say(f is not None and not miss, "%s: %s's final bitmap holds every slot guest 1 announced %s (missing %s)" % (row, name, sorted(killed_slots), miss))
    info("%s final BOB bitmaps (n): %s" % (row, {k: (v[1] if v else None) for k, v in fin.items()}))
    say(all(fin.values()) and len(set(v[2] for v in fin.values())) == 1 and fin["g1"][1] >= 5,
        "%s: guest 1, host and guest 2 end with the same bitmap, n=%s (Goomba, 3 coins, the star)" % (row, fin["g1"] and fin["g1"][1]))
    # 3. late 4th
    if c4 is None:
        say(False, "%s: the 4th console was never started (no star)" % row)
        return rm
    t4 = c4.text()
    est = bits(t4, "established")
    jn = re.findall(r"(?m)^\[coop\] joined level %d act %d: (.*)$" % (BOB, ACT), t4)
    info("%s 4th console joins: %s" % (row, jn[:2]))
    say(bool(jn) and "snapshot from seat" in jn[0], "%s: the late 4th console got the group's snapshot before spawn (%s)" % (row, jn[:1]))
    say(bool(est) and bool(fin["g1"]) and est[0][2] == fin["g1"][2], "%s: its first bitmap equals guest 1's final one, n=%s" % (row, est[0][1] if est else None))
    pat = r"(?m)^\[coop\] census level=%d act=%d actors=(\d+) slotted=(\d+)" % (BOB, ACT)
    cen, cena = re.findall(pat, t4), re.findall(pat, ta)
    info("%s censuses BOB: guest1 %s | 4th %s" % (row, cena[:3], cen[:2]))
    if cen and cena:
        say(int(cena[0][1]) - int(cen[0][1]) >= 4, "%s: its first census lacks the killed actors (guest1 first %s slotted, 4th %s)" % (row, cena[0][1], cen[0][1]))
    # 4. the host's coop image
    img = open(sav, "rb").read()
    pr = CS.parse(img)
    st = CS.stars_of(pr[(0, 0)][2])
    say(SAVE_STAR in st and pr[(0, 0)][1] and pr[(0, 1)][1], "%s: the host coop image holds star %s with valid checksums (stars %s)" % (row, SAVE_STAR, st))
    # 5. pictures / host quit
    if quit_host:
        g = cons[1:]
        say(all(c.rc == 7 for c in g), "%s: after the host was killed every guest exited with code 7 (rcs %s)" % (row, [c.rc for c in g]))
        say(all("The host left. Room closed." in c.text() for c in g), "%s: every guest said 'The host left. Room closed.'" % row)
    else:
        say(all(c.rc == 0 for c in cons), "%s: every console ran to its frame cap, rc %s" % (row, [c.rc for c in cons]))
        pics = [CB.png(c, row) for c in cons]
        info("%s pictures: %s" % (row, [os.path.basename(p) if p else None for p in pics]))
        say(all(pics), "%s: a picture from each of the four consoles exists (shots/)" % row)
    return rm


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--base", required=True)
    ap.add_argument("--root", default=CS.REPO)
    ap.add_argument("--out", default=os.path.join(CS.REPO, "build", "coop_int_proof"))
    ap.add_argument("rows", nargs="+")
    a = ap.parse_args()
    a.exe = os.path.abspath(a.exe)
    a.base = os.path.abspath(a.base)
    CS.ARGS = CB.ARGS = a
    os.makedirs(a.out, exist_ok=True)
    table = {"int4": lambda: run("int4", False), "int4q": lambda: run("int4q", True)}
    for r in a.rows:
        table[r]()
    bad = [r for r in RES if r.startswith("FAIL")]
    print("\n%d PASS %d FAIL" % (len([r for r in RES if r.startswith("PASS")]), len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
