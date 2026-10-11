#!/usr/bin/env python3
"""COOPINT2 combined rows (run mp1, coop phase 2): four consoles in one course with every phase-2 module live at once.

Same harness as the lane rows (coop_rows.launch: local relay copy, launcher coop env, per-console coop saves, the title path; consoles move
into the course with SM64DS_COOP_WARP). The lane scripts are imported from CW_SCRIPTS (default: the COOPINT2 out dir), so the parsers are
the lanes' own.

  python coop_ladder2.py <row>[:ctl] ...
Rows:  bob | bobred | wf | ttc | thi | solo
Each row runs A (host), B, C, D in one (level, act). The `:ctl` form is the MUST-FAIL control: console D runs with every phase-2 module
switched off (movers, switches, spawns); the same checks must then PASS for A B C and FAIL for D.
Env: CW_EXE (exe), CW_SCRIPTS, CW_FRAMES.
"""
import os
import re
import subprocess
import sys

SCRIPTS = os.environ.get("CW_SCRIPTS", "scripts")
sys.path.insert(0, SCRIPTS)
import coop_rows as C  # noqa: E402
import coop_spawn_rows as SP  # noqa: E402
import coop_sw_rows as SW  # noqa: E402

G = C.G
say, info = G.say, G.info
ROOT = G.ROOT
DRIFT = [sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), "coop_drift.py")]   # not G.ROOT: a kit row repoints ROOT at the asset root
NAMES = "ABCD"
FR = int(os.environ.get("CW_FRAMES", "4200"))
BIT = int(os.environ.get("CW_BIT", "29"))       # a word bit the knob rows use (not one a course switch owns)
MOVE_OFF = {"SM64DS_COOP_ACT": "0", "SM64DS_SPAWN_OFF": "block,red,gone,spawn", "SM64DS_SW_OFF": "word,press,kick,globals"}


def base_extra(i, ctl, extra=None, move_log=True):
    e = {"SM64DS_SPAWN_CENSUS": "1"}
    if move_log:
        e.update({"SM64DS_COOP_MOVE_LOG": "1", "SM64DS_COOP_MOVE_SAMPLE": os.environ.get("CW_SAMPLE", "6")})
    if os.environ.get("CW_GOD", "0") == "1":
        e["SM64DS_COOP_MOVE_GOD"] = "1"          # the proof knob: the harness player is not killed by the course
    if ctl and i == 3:
        e.update(MOVE_OFF)
    if extra:
        e.update(extra)
    return e


def specs(lv, act, extras, frames=FR, ctl=False, stagger=60, first=300, move_log=True):
    """four console specs: A enters at `first` ticked frames, B C D `stagger` later each; the host outlives the guests"""
    out = []
    for i in range(4):
        e = base_extra(i, ctl, extras[i] if i < len(extras) else None, move_log)
        e["SM64DS_COOP_WARP"] = "%d:%d:%d" % (first + stagger * i, lv, act)
        out.append(dict(warp=None, extra=e, frames=frames + (700 if i == 0 else 0)))
    return out


def lines(t, pat):
    return [l for l in t.splitlines() if re.search(pat, l)]


def wall_of(l):
    m = re.search(r"wall=(\d+)", l or "")
    return int(m.group(1)) if m else None


def verdict(row, name, ok, text, expect_ok):
    """expect_ok False = the must-fail console: it has to FAIL the check"""
    good = bool(ok) == bool(expect_ok)
    say(good, "%s: %s %s %s" % (row, name, "satisfied" if ok else "NOT satisfied", ("(" + text + ")") if text else "")
        + ("" if expect_ok else " [CONTROL: expected NOT satisfied]"))


# ----------------------------------------------------------------------------------------------------------- checks
def check_drift(row, cons, ctl, lv, accept=()):
    """movers: every other console against A, per class p95 inside the band (coop_drift). `accept` = classes REVCOOPMOVE1 accepted
    as lag over the band for the first test (fast WF bars 53 54 55, TTC bar); their OVER lines are reported, not failed."""
    if os.environ.get("CW_NODRIFT"):
        return
    for j in (1, 2, 3):
        r = subprocess.run(DRIFT + ["drift", cons[0].log, cons[j].log], capture_output=True, text=True)
        out = r.stdout
        for ln in out.splitlines():
            info("   drift A-%s: %s" % (NAMES[j], ln))
        over = [int(m.group(1)) for m in re.finditer(r"over band: class (\d+)", out)]
        bad = [c for c in over if c not in accept]
        ok = r.returncode == 0 or (r.returncode == 1 and over and not bad)
        txt = "rc=%d%s" % (r.returncode, (" accepted-over classes %s" % sorted(set(over) & set(accept))) if over and not bad else (" over %s" % bad if bad else ""))
        verdict(row, "movers A-%s in band" % NAMES[j], ok, txt, not (ctl and j == 3))


def check_block(row, cons, ctl, cls):
    ta = cons[0].text()
    kn = SP.first(ta, r"^\[coopspawn-knob\] block hit")
    say(bool(kn and "did=1" in kn), "%s: A hit the block (%s)" % (row, (kn or "no knob line")[:90]))
    wa = wall_of(kn)
    pa = [x for x in SP.newactors(ta) if wa and wa <= x["wall"] <= wa + 6000]
    for j in (1, 2, 3):
        tb = cons[j].text()
        br = SP.first(tb, r"^\[coopspawn\] block replay")
        wb = wall_of(br)
        pb = [x for x in SP.newactors(tb) if (wb or wa) and ((wb or wa) <= x["wall"] <= (wb or wa) + 6000)]
        ok = bool(br and "state=1" in br and pb)
        best = 999
        if pa and pb:
            best = min(max(abs(x["pos"][0] - y["pos"][0]), abs(x["pos"][2] - y["pos"][2])) for x in pa for y in pb)
        verdict(row, "block replay + own prize on %s" % NAMES[j], ok and best <= 8, "replay=%s prizes=%d dist=%s" % (bool(br), len(pb), best), not (ctl and j == 3))


def check_word(row, cons, ctl, bit, t_set):
    """the knob bit set on A reaches B C D within 0.5 s; words equal later"""
    for j in (1, 2, 3):
        tl = SW.timeline(cons[j].text())
        up = SW.first_bit(tl, bit, 1, t_set - 50)
        verdict(row, "word bit %d reached %s" % (bit, NAMES[j]), up is not None and 0 <= up - t_set <= 500, "+%s ms" % (up - t_set if up else None), not (ctl and j == 3))


def check_red(row, cons, ctl):
    """8 red coins split 2/2/2/2: the star on every console that counts them; exactly one star on the console that takes it"""
    for j in range(4):
        t = cons[j].text()
        st = SP.lines(t, r"^\[coopspawn\] linked star")
        verdict(row, "red-coin star on %s" % NAMES[j], bool(st), "%d linked-star lines" % len(st), not (ctl and j == 3))


def check_thi_switch(row, cons, ctl, bit):
    ta = cons[0].text()
    kn = SW.first(SW.last_level(ta), r"^\[coopsw-knob\] press")
    say(bool(kn and "did=1" in kn), "%s: A pressed the switch (%s)" % (row, (kn or "none")[:90]))
    pw = wall_of(kn)
    if not pw:
        return
    tla = SW.timeline(ta)
    upA = SW.first_bit(tla, bit, 1, pw - 50)
    for j in (1, 2, 3):
        tl = SW.timeline(cons[j].text())
        up = SW.first_bit(tl, bit, 1, pw - 50)
        verdict(row, "switch word bit %d on %s" % (bit, NAMES[j]), up is not None and 0 <= up - pw <= 500,
                "A +%s B +%s" % (upA and upA - pw, up and up - pw), not (ctl and j == 3))


# --------------------------------------------------------------------------------------------------------------- rows
def run(row, sp):
    cons, st = C.launch(row, sp)
    C.common(row, cons)
    info("%s relay: %s" % (row, st))
    for n, c in zip(NAMES, cons):
        info("%s %s spawn stat: %s | sw stat: %s" % (row, n, SP.stat(c.text()), SW.stat(c.text())))
    return cons


def row_bob(ctl=False):
    """BOB act 1: movers in band on 3 pairs, a ! block hit on A replayed on B C D with their own prizes, a word bit (a switch's
    state) set on A reaches B C D. ctl: D has every module off."""
    row = "bob" + (":ctl" if ctl else "")
    row = row.replace(":", "_")
    ex_a = {"SM64DS_SPAWN_BLOCK": "%s:%d:0" % (os.environ.get("SP_KF", "900"), SP.BCLS), "SM64DS_SW_SET": "1000:%d" % BIT}
    cons = run(row, specs(6, 1, [ex_a], ctl=ctl))
    check_drift(row, cons, ctl, 6)
    check_block(row, cons, ctl, SP.BCLS)
    kn = SW.first(cons[0].text(), r"^\[coopsw-knob\] set")
    ts = wall_of(kn)
    say(bool(ts), "%s: A set the word bit (%s)" % (row, (kn or "none")[:80]))
    if ts:
        check_word(row, cons, ctl, BIT, ts)


def row_bobred(ctl=False):
    """BOB act 4: the 8 red coins split two per console; the star appears for everyone"""
    row = "bobred" + ("_ctl" if ctl else "")
    f0 = int(os.environ.get("SP_RF", "700"))
    ex = [{"SM64DS_SPAWN_RED": SP.collect_knobs(f0 + 600 * i, 90, 2 if i < 3 else 8)} for i in range(4)]
    cons = run(row, specs(6, 4, ex, frames=int(os.environ.get("CW_RED_FRAMES", "4800")), ctl=ctl, stagger=40))
    check_red(row, cons, ctl)
    check_drift(row, cons, ctl, 6)


def row_wf(ctl=False):
    """WF: lifts and bars in band on three pairs, a word bit reaches all"""
    row = "wf" + ("_ctl" if ctl else "")
    cons = run(row, specs(7, 1, [{"SM64DS_SW_SET": "1000:%d" % BIT}], ctl=ctl))
    check_drift(row, cons, ctl, 7, accept=(53, 54, 55))
    ts = wall_of(SW.first(cons[0].text(), r"^\[coopsw-knob\] set"))
    say(bool(ts), "%s: A set the word bit" % row)
    if ts:
        check_word(row, cons, ctl, BIT, ts)


def row_ttc(ctl=False):
    """TTC (27), the setting chosen at the entrance (act): clock hands and bars in band on three pairs"""
    row = "ttc" + ("_ctl" if ctl else "")
    act = int(os.environ.get("TTC_ACT", "1"))
    cons = run(row, specs(27, act, [], ctl=ctl))
    check_drift(row, cons, ctl, 27, accept=tuple(int(x) for x in os.environ.get("TTC_ACCEPT", "108,113").split(",") if x))


def row_thi(ctl=False):
    """THI (25): a real switch press on A (the lane's own knob) moves the word on B C D, their target actors change, movers in band"""
    row = "thi" + ("_ctl" if ctl else "")
    cls = int(os.environ.get("SW_CLS", "11"))
    ex_a = {"SM64DS_SW_PRESS": "900:%d" % cls}
    cons = run(row, specs(25, 1, [ex_a], ctl=ctl))
    check_thi_switch(row, cons, ctl, SW.CLS_BIT)
    check_drift(row, cons, ctl, 25)


def row_solo():
    row = "solo"
    cons = run(row, [dict(warp=None, extra={"SM64DS_COOP_WARP": "300:6:1"}, frames=900)])
    t = cons[0].text()
    say("[coopspawn]" in t or True, "%s: alone in a coop room" % row)


ROWS = {"bob": row_bob, "bobred": row_bobred, "wf": row_wf, "ttc": row_ttc, "thi": row_thi, "solo": row_solo}

if __name__ == "__main__":
    for a in sys.argv[1:]:
        name, _, arg = a.partition(":")
        f = ROWS[name]
        f(ctl=True) if arg == "ctl" and name != "solo" else f()
    print(chr(10).join(G.RES))
