#!/usr/bin/env python3
"""COOPCASTLE1 rows (run mp1, coop phase 2): castle Boos (209) and MIPS (187) in step across consoles. Built on coop_ladder2's harness
(local relay copy, launcher coop env, per-console coop saves, the title path, SM64DS_COOP_WARP moves a console into the course).

  python coop_castle_rows.py <row>[:ctl] ...
Rows:  boo | kill | mips | catch | late | solo
Every row has a MUST-FAIL control (`:ctl`): the LAST console runs with phase 2 off (and, for `kill`, with the death sync off); the same
checks must PASS for the others and FAIL for it.
Env: CW_EXE, CW_SCRIPTS, CW_FRAMES, CASTLE_LV (default 3), CASTLE_MIPLV (default 1), CASTLE_ACT (default 1)
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import coop_ladder2 as L  # noqa: E402

G = L.G
say, info = G.say, G.info
NAMES = "ABCD"
LV = int(os.environ.get("CASTLE_LV", "3"))
MIPLV = int(os.environ.get("CASTLE_MIPLV", "1"))
ACT = int(os.environ.get("CASTLE_ACT", "1"))
FR = int(os.environ.get("CW_FRAMES", "3000"))
DRIFT = [sys.executable, os.path.join(HERE, "coop_castle_drift.py")]
LOGV = {"SM64DS_COOP_CASTLE_LOG": "1"}

CC = re.compile(r"^\[coopcastle\] wall=(\d+) f=(\d+) boo=(\d+) mip=(\d+) key=(\d+) b0=(\S+) m0=(.*)$", re.M)


def cc(t):
    return [(int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)), m.group(6), m.group(7)) for m in CC.finditer(t)]


def verdict(row, name, ok, text, expect_ok):
    good = bool(ok) == bool(expect_ok)
    say(good, "%s: %s %s %s" % (row, name, "satisfied" if ok else "NOT satisfied", ("(" + text + ")") if text else "")
        + ("" if expect_ok else " [CONTROL: expected NOT satisfied]"))


def sp(lv, act, extras, ctl, n, frames=None, first=300, stagger=60, ctl_extra=None):
    """n console specs; with ctl the LAST one switches phase 2 off (L.MOVE_OFF) plus ctl_extra"""
    out = L.specs(lv, act, [dict(LOGV, **(extras[i] if i < len(extras) else {})) for i in range(4)], frames=frames or FR, ctl=False,
                  stagger=stagger, first=first)[:n]
    if ctl:
        out[n - 1]["extra"].update(L.MOVE_OFF)
        if ctl_extra:
            out[n - 1]["extra"].update(ctl_extra)
    return out


def drift(row, cons, ctl, pairs, cls, lo_wall=None):
    """per-class p95 of console j against A inside the band; the last console is the control when ctl"""
    last = len(cons) - 1
    for j in pairs:
        args = DRIFT + ["drift", cons[0].log, cons[j].log]
        if lo_wall:
            args += ["--from-wall", str(lo_wall)]
        r = subprocess.run(args, capture_output=True, text=True)
        for ln in r.stdout.splitlines():
            info("   drift A-%s: %s" % (NAMES[j], ln))
        rows = {}
        for ln in r.stdout.splitlines():
            m = re.match(r"\s+(\d+)\s+(\d+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+(\d+)\s+(\d+)\s+(\S+)", ln)
            if m:
                rows[int(m.group(1))] = (int(m.group(2)), float(m.group(3)), float(m.group(4)), float(m.group(5)), m.group(8))
        for c in cls:
            if c not in rows:
                verdict(row, "class %d A-%s paired samples" % (c, NAMES[j]), False, "none", not (ctl and j == last))
                continue
            n, p50, p95, mx, ok = rows[c]
            verdict(row, "class %d A-%s p95 in band" % (c, NAMES[j]), ok == "ok", "n=%d p50 %.1f p95 %.1f max %.1f" % (n, p50, p95, mx), not (ctl and j == last))


def run(row, specs_):
    return L.run(row, specs_)


def row_boo(ctl=False):
    """castle Boos (level LV): three consoles, the same Boos in the same place (drift p95 inside the enemy band)"""
    row = "castle_boo" + ("_ctl" if ctl else "")
    n = 3
    cons = run(row, sp(LV, ACT, [], ctl, n))
    for j, c in enumerate(cons):
        v = cc(c.text())
        info("%s %s boo counts seen: %s" % (row, NAMES[j], sorted(set(x[2] for x in v))))
        say(bool(v) and max(x[2] for x in v) > 0, "%s: console %s sees Boos (max %d)" % (row, NAMES[j], max([x[2] for x in v] or [0])))
    drift(row, cons, ctl, (1, 2), (209,))


def row_kill(ctl=False):
    """A kills 2 Boos the ROM's own way (SM64DS_COOP_KILL); B and C end with the same number of live Boos. ctl: C has the death sync off"""
    row = "castle_kill" + ("_ctl" if ctl else "")
    n = 3
    kf = int(os.environ.get("CASTLE_KF", "700"))
    cons = run(row, sp(LV, ACT, [{"SM64DS_COOP_KILL": "%d:209:2" % kf}], ctl, n, ctl_extra={"SM64DS_COOP_SYNC": "0"}))
    ta = cons[0].text()
    kn = re.findall(r"\[coop-knob\] kill class=209 asked (\d+) did (\d+)", ta)
    say(bool(kn) and kn[-1][1] == "2", "%s: A killed 2 Boos the ROM's own way (%s)" % (row, kn[-1:] if kn else "no knob line"))
    va = cc(ta)
    info("%s A boo count series (last 12): %s" % (row, [x[2] for x in va][-12:]))
    for j in (1, 2):
        vb = cc(cons[j].text())
        info("%s %s boo count series: %s" % (row, NAMES[j], [x[2] for x in vb][-14:]))
        ok = bool(va and vb) and vb[-1][2] == va[-1][2] and max(x[2] for x in vb) > va[-1][2]
        verdict(row, "console %s ends with A's Boo count" % NAMES[j], ok,
                "A last %s, %s max %s last %s" % (va[-1][2] if va else None, NAMES[j], max([x[2] for x in vb] or [0]), vb[-1][2] if vb else None),
                not (ctl and j == n - 1))


def row_mips(ctl=False):
    """MIPS (level MIPLV): the hunt flag wakes the rabbit, A stands next to it for a while; its path is the same on B C (drift) and it moved"""
    row = "castle_mips" + ("_ctl" if ctl else "")
    n = 3
    chase = os.environ.get("CASTLE_CHASE", "600:900")
    ex = [{"SM64DS_CASTLE_HUNT": "1", "SM64DS_CASTLE_CHASE": chase}, {"SM64DS_CASTLE_HUNT": "1"}, {"SM64DS_CASTLE_HUNT": "1"}]
    cons = run(row, sp(MIPLV, ACT, ex, ctl, n))
    for j, c in enumerate(cons):
        v = cc(c.text())
        pos = [x[6].split(" ")[0] for x in v if x[3] > 0]
        info("%s %s mips path: %s" % (row, NAMES[j], pos[::max(1, len(pos) // 14)]))
    drift(row, cons, ctl, (1, 2), (187,))


def row_catch(ctl=False):
    """A catches MIPS (the existing grab stand-in, SM64DS_RABBIT_TRIGGER); the key spawns on A once; B's later catch spawns none"""
    row = "castle_catch" + ("_ctl" if ctl else "")
    n = 2
    f1 = int(os.environ.get("CASTLE_C1", "600"))
    f2 = int(os.environ.get("CASTLE_C2", "2400"))
    press = os.environ.get("CASTLE_PRESS", "")
    base = {"SM64DS_CASTLE_HUNT": "1"}
    ea = dict(base, SM64DS_RABBIT_TRIGGER=str(f1))
    eb = dict(base, SM64DS_RABBIT_TRIGGER=str(f2))
    if press:
        ea["SM64DS_PROBE_INPUT"] = press
        eb["SM64DS_PROBE_INPUT"] = press
    cons = run(row, sp(MIPLV, ACT, [ea, eb], ctl, n, frames=int(os.environ.get("CW_FRAMES", "4200"))))
    for j, c in enumerate(cons):
        t = c.text()
        ks = re.findall(r"(?m)^\[coopcastle\] key spawned.*$", t)
        rb = re.findall(r"(?m)^\s*\[rabbit\] f\d+ (?:GRABBED|state).*$", t)[:6]
        info("%s %s key spawns %d | rabbit lines %s" % (row, NAMES[j], len(ks), rb))
    ka = len(re.findall(r"(?m)^\[coopcastle\] key spawned", cons[0].text()))
    kb = len(re.findall(r"(?m)^\[coopcastle\] key spawned", cons[1].text()))
    say(ka == 1, "%s: the key spawned once on A (%d)" % (row, ka))
    verdict(row, "B's later catch spawns no second key", kb == 0, "B key spawns %d" % kb, not ctl)


def row_late(ctl=False):
    """A kills 2 Boos before C joins; C sees A's Boo count and the Boos sit in step from its first samples"""
    row = "castle_late" + ("_ctl" if ctl else "")
    n = 3
    kf = int(os.environ.get("CASTLE_KF", "500"))
    late = int(os.environ.get("CASTLE_LATE", "1800"))
    out = sp(LV, ACT, [{"SM64DS_COOP_KILL": "%d:209:2" % kf}], ctl, n, frames=late + 1500)
    out[2]["extra"]["SM64DS_COOP_WARP"] = "%d:%d:%d" % (late, LV, ACT)
    out[2]["frames"] = 1500
    cons = run(row, out)
    va, vc = cc(cons[0].text()), cc(cons[2].text())
    info("%s A boo series tail %s | C boo series %s" % (row, [x[2] for x in va][-6:], [x[2] for x in vc][:8]))
    ok = bool(va and vc) and vc[0][2] == va[-1][2]
    verdict(row, "late console C sees A's Boo count from its first line", ok,
            "A end %s, C first %s last %s" % (va[-1][2] if va else None, vc[0][2] if vc else None, vc[-1][2] if vc else None), not ctl)
    drift(row, cons, ctl, (2,), (209,))


def row_solo():
    row = "castle_solo"
    cons = run(row, [dict(warp=None, extra=dict(LOGV, SM64DS_COOP_WARP="300:%d:%d" % (LV, ACT)), frames=900)])
    v = cc(cons[0].text())
    say(bool(v), "%s: alone in a coop room, the castle runs (%d log lines, boo max %d)" % (row, len(v), max([x[2] for x in v] or [0])))


ROWS = {"boo": row_boo, "kill": row_kill, "mips": row_mips, "catch": row_catch, "late": row_late, "solo": row_solo}

if __name__ == "__main__":
    for a in sys.argv[1:]:
        name, _, arg = a.partition(":")
        f = ROWS[name]
        f(ctl=True) if arg == "ctl" and name != "solo" else f()
    print(chr(10).join(G.RES))
