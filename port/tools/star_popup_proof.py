#!/usr/bin/env python3
"""STAR POPUP PROOF, party rooms (run mp1, lane STARPOP1). Builds on party_stars_proof.py (same rig: real consoles started the way
the launcher starts a party room, local relay copy, quiet and muted, PSTAR_RIG / PSTAR_SEED / PSTAR_LOCK as there).

  python star_popup_proof.py --exe NEW.exe --root TREE --out DIR [--fps 30] [--kit KIT.exe --kitroot ROOT] row [row ...]

rows
  pstar     A collects BOB star (the ROM's own star handler); B in BOB sees the popup, 5 s later exits where A lands; C in WF gets a toast
  pstar_off MUST-FAIL: SM64DS_STAR_POPUP=0 on every console: nothing sent, B stays in BOB
  pkey      a key event (synthetic, SM64DS_STAR_POPUP_FAKE) in level 49: B exits through the key landing (level 5)
  pnoexit   a star that does not exit the collector (synthetic exits=0): toast only, B stays
  phold     B is held unsafe when the countdown ends: it exits after the hold
  pforge    forged SYNX with a non-zero reserved word: no popup, no exit
Coop rows live in the lane's rig (coop rows need the coop harness of coop_world_proof / coop_rows); see the lane out dir.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import party_stars_proof as P  # noqa: E402

say, info = P.say, P.info
BOB, WF = P.BOB, 7


def lines(t, pat):
    return re.findall(r"(?m)^.*" + pat + r".*$", t)


def lc(t):
    return [(int(a), int(b), int(c)) for a, b, c in re.findall(r"(?m)^\[lvl\] change: level (-?\d+) -> (-?\d+), entrance (\d+)", t)]


def base_extra(extra):
    e = {"SM64DS_STAR_POPUP_LOG": "1"}
    e.update(extra)
    return e


def run_star(row, off=False):
    f = P.frames_for(int(os.environ.get("SP_SEC", "90")))
    po = {"SM64DS_STAR_POPUP": "0"} if off else {}
    stopa, stopb = os.environ.get("SP_STOP_A"), os.environ.get("SP_STOP_B")
    ea = base_extra(dict(P.warp(300), SM64DS_STAR_TRIGGER="1100", SM64DS_STAR_PROBE="1", SM64DS_EXIT_WATCH="1", **po))
    eb = base_extra(dict(P.warp(300), **po))
    ec = base_extra(dict(P.warp(300, WF), **po))
    if stopa:
        ea["SM64DS_STAR_POPUP_STOP"] = stopa
    if stopb:
        eb["SM64DS_STAR_POPUP_STOP"] = stopb
    r = P.room(row, [dict(frames=f, extra=ea), dict(frames=f, extra=eb), dict(frames=f, extra=ec)])
    P.common(row, r.cons)
    ta, tb, tc = [c.text() for c in r.cons]
    for n, t in (("A", ta), ("B", tb), ("C", tc)):
        info("%s %s level changes %s" % (row, n, lc(t)))
        for l in lines(t, r"\[starpop\]")[:10]:
            info("%s %s %s" % (row, n, l[:220]))
    exb = lines(tb, r"\[starpop\] EXIT")
    a_exit = [x for x in lc(ta) if x[0] == BOB]
    b_exit = [x for x in lc(tb) if x[0] == BOB]
    if off:
        say(not lines(ta, r"\[starpop\] tx") and not exb and not b_exit,
            "%s: MUST-FAIL: popup off, nothing sent, no EXIT, B stays in BOB (B exits %s)" % (row, b_exit))
        return r
    say(len(lines(ta, r"\[starpop\] tx")) >= 1, "%s: A's real star sent a popup event" % row)
    say(len(lines(tb, r"\[starpop\] rx .*same=1")) >= 1, "%s: B (same level + act) got the popup" % row)
    say(len(exb) == 1 and bool(b_exit), "%s: B exited once and left BOB (%s)" % (row, b_exit))
    say(not lines(ta, r"\[starpop\] rx"), "%s: A got no popup of its own" % row)
    say(len(lines(tc, r"\[starpop\] rx .*same=0")) >= 1 and not lines(tc, r"\[starpop\] EXIT") and not [x for x in lc(tc) if x[0] == WF],
        "%s: C (WF) got a toast only, stayed in WF (%s)" % (row, lc(tc)))
    say(bool(a_exit and b_exit) and a_exit[0][1:] == b_exit[0][1:], "%s: B landed where A landed: A %s B %s" % (row, a_exit, b_exit))
    return r


def run_fake(row, kind, exits, lv, expect_exit, expect_land=None, hold=None):
    f = P.frames_for(int(os.environ.get("SP_SEC", "70")))
    ea = base_extra(dict(P.warp(300, lv), SM64DS_STAR_POPUP_FAKE="%d:%d@900" % (kind, exits)))
    eb = base_extra(P.warp(300, lv))
    if hold:
        eb["SM64DS_STAR_POPUP_HOLD"] = hold
    if os.environ.get("SP_STOP_B"):
        eb["SM64DS_STAR_POPUP_STOP"] = os.environ["SP_STOP_B"]
    r = P.room(row, [dict(frames=f, extra=ea), dict(frames=f, extra=eb)])
    P.common(row, r.cons)
    ta, tb = [c.text() for c in r.cons]
    for n, t in (("A", ta), ("B", tb)):
        info("%s %s level changes %s" % (row, n, lc(t)))
        for l in lines(t, r"\[starpop\]")[:10]:
            info("%s %s %s" % (row, n, l[:220]))
    exb = lines(tb, r"\[starpop\] EXIT")
    say(len(lines(ta, r"\[starpop\] tx")) == 1 and len(lines(tb, r"\[starpop\] rx")) >= 1, "%s: A sent one event, B received it" % row)
    if expect_exit:
        say(len(exb) == 1, "%s: B exited once" % row)
        if hold:
            w = lambda l: int(re.search(r"wall=(\d+)", l).group(1))
            rxl = lines(tb, r"\[starpop\] rx")
            if rxl and exb:
                say(w(exb[0]) - w(rxl[0]) > 5600, "%s: the exit waited for the hold: %d ms after the popup (countdown is 5000)" % (row, w(exb[0]) - w(rxl[0])))
        if expect_land:
            say((lv, expect_land[0], expect_land[1]) in lc(tb), "%s: B landed %s (%s)" % (row, expect_land, lc(tb)))
    else:
        say(not exb and not [x for x in lc(tb) if x[0] == lv], "%s: B did NOT exit (toast only)" % row)
    return r


def row_pforge():
    """a room member sends SYNX with reserved 8 (an unknown bit) and with reserved 3 (both kinds): neither does anything"""
    row = "pforge"
    f = P.frames_for(60)
    ex = base_extra(P.warp(300))

    def script(ctx):
        P.in_bob(ctx, [0])
        ctx.forger.say(BOB)
        import time as _t
        _t.sleep(1.0)
        import struct
        for rsv in (8, 3, 0x100):
            ctx.forger.seq = (ctx.forger.seq + 1) & 0xffff
            d = bytearray(P.T.pstar(course=0, star=0, seq=ctx.forger.seq))
            d[8:10] = struct.pack("<H", rsv)
            ctx.forger.raw(bytes(d))
            _t.sleep(0.4)
    r = P.room(row, [dict(frames=f, extra=ex)], forger_role=P.T.ROLE_PARTY, script=script)
    P.common(row, r.cons)
    t = r.cons[0].text()
    say(not lines(t, r"\[starpop\] EXIT") and not lines(t, r"\[starpop\] rx .*same=1"),
        "%s: forged reserved words made no popup and no exit on the real console" % row)
    return r


P.ROWS.update(pstar=lambda: run_star("pstar"), pstar_off=lambda: run_star("pstar_off", True),
              pkey=lambda: run_fake("pkey", 2, 1, 36, True, (2, 10)),
              pnoexit=lambda: run_fake("pnoexit", 1, 0, BOB, False),
              phold=lambda: run_fake("phold", 1, 1, BOB, True, None, os.environ.get("SP_HOLD", "1000:2500")),
              pforge=row_pforge)

if __name__ == "__main__":
    sys.exit(P.main())
