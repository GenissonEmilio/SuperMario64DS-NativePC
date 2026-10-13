#!/usr/bin/env python3
"""COOPTP1 rows (run mp1): TP to friends in a coop room, 2..3 consoles through a LOCAL relay copy on the TITLE PATH
(the launcher's coop env). Built on the COOPWORLD1 row plumbing (coop_rows.py, lgc.py, r3gw.py, kept next to this
file or in CTP_SCRIPTS). Console 0 is the host (seat 0) and is the FRIEND; console 1 (seat 1) clicks the friend's row
through the proof knob SM64DS_COOP_TP_FAKE=<seat>@<tick> (the same call the mouse handler makes; the real mouse path is
covered by PLIST1's WM_LBUTTONDOWN rows).

  python coop_tp_proof.py <row>[:base] ...
Rows: badge | tp | tpmid | tplocked | tpoff | tpparty | refuse_menu | refuse_me | refuse_same | alone
Env: CTP_EXE (exe under test), CTP_BASE_EXE (control = f91d317300), CTP_SCRIPTS.
:base runs the row on the base exe (the must-fail control).
"""
import os, re, sys

sys.path.insert(0, os.environ.get("CTP_SCRIPTS", os.path.dirname(os.path.abspath(__file__))))
import coop_rows as CR
import lgc as G

say, info = G.say, G.info
EXE = os.environ.get("CTP_EXE", "C:/tmp/cooptp1/exes/b1/walk_window.exe")
BASE = os.environ.get("CTP_BASE_EXE", "C:/tmp/cooptp1/base/walk_window.exe")
CR.EXE = EXE
BOB = 6
GOOMBA, COIN = CR.GOOMBA, CR.COIN
KILLS = "500:%d:1;505:%d:3" % (GOOMBA, COIN)
TPTICK = int(os.environ.get("CTP_TICK", "1100"))
FRAMES = int(os.environ.get("CTP_FRAMES", "2200"))


def tp_env(extra=None):
    e = {"SM64DS_COOP_TP_LOG": "1", "SM64DS_PLIST_LOG": "1", "SM64DS_STAR_POPUP_LOG": "0"}
    if extra:
        e.update(extra)
    return e


def run(row, exe, friend_warp, friend_extra, me_extra, me_warp=None, kind="coop", frames=FRAMES, third=None):
    sp = [dict(warp=friend_warp, extra=tp_env(friend_extra), frames=frames, exe=exe, kind=kind),
          dict(warp=me_warp, extra=tp_env(me_extra), frames=frames, exe=exe, kind=kind, delay=0.5)]
    if third:
        sp.append(dict(warp=third.get("warp"), extra=tp_env(third.get("extra")), frames=frames, exe=exe, kind=kind))
    cons, st = CR.launch(row, sp)
    info("%s relay: %s" % (row, st))
    return cons


def lines(t, pat):
    return re.findall(r"(?m)^.*%s.*$" % pat, t)


def bits_final(t, level):
    b = [m for m in CR.bits_of(t) if m.group(2) == str(level)]
    return (b[-1].group(3), int(b[-1].group(5)), b[-1].group(6)) if b else None


def first_est(t, level):
    b = [m for m in CR.bits_of(t, "established") if m.group(2) == str(level)]
    return (b[0].group(3), int(b[0].group(5)), b[0].group(6)) if b else None


def wall_of(l):
    m = re.search(r"wall=(\d+)", l)
    return int(m.group(1)) if m else None


def row_badge(arm="new"):
    row = "badge" + ("_base" if arm == "base" else "")
    exe = BASE if arm == "base" else EXE
    cons = run(row, exe, "%d@300" % BOB, {}, {}, frames=900)
    fa, ca = cons[0], cons[1]
    CR.common(row, cons)
    t = ca.text()
    on = lines(t, r"\[cotp\] badge seat 0 ON")
    info("%s A badge lines: %s" % (row, on[:2]))
    if arm == "base":
        say(not on and not lines(t, r"\[cotp\]"), "%s: base exe has no [cotp] code (no badge)" % row)
    else:
        say(bool(on), "%s: A shows the TP badge on the friend's row once B is in a level" % row)
        say(not lines(t, r"\[cotp\] badge seat 1"), "%s: no badge on my own row" % row)
    import r3gw as R
    R.bmp_png(ca, "%s_A.png" % row)


def landing(row, ta, tb, level, act):
    """A (the clicker) landed in the friend's level with the friend's group."""
    tpl = lines(ta, r"\[cotp\] TP to seat 0")
    land = lines(ta, r"\[cotp\] landed")
    info("%s A: %s" % (row, tpl[:1]))
    info("%s A: %s" % (row, land[:1]))
    say(bool(tpl) and "level %d act %d" % (level, act) in tpl[0], "%s: A clicked B's row and asked the ROM for level %d act %d" % (row, level, act))
    ms = re.search(r"established level (\d+) act (\d+), (\d+) ms after the click", land[0]) if land else None
    say(bool(ms) and int(ms.group(1)) == level and int(ms.group(2)) == act and int(ms.group(3)) <= 10000,
        "%s: A landed in the friend's level+act, group established within 10 s (%s ms)" % (row, ms and ms.group(3)))
    ka = CR.last_bits(ta)
    kb = CR.last_bits(tb)
    info("%s A last bits %s | B last bits %s" % (row, ka and ka[:3], kb and kb[:3]))
    say(bool(ka and kb) and ka[0] == kb[0] == str(level) and ka[1] == kb[1] == str(act), "%s: group key equal (A %s, B %s)" % (row, ka and ka[:2], kb and kb[:2]))
    return ka, kb


def row_tp(arm="new", warp=BOB, locked=False):
    row = ("tplocked" if locked else "tp") + ("_base" if arm == "base" else "")
    exe = BASE if arm == "base" else EXE
    fx = {"SM64DS_COOP_KILL": KILLS} if not locked else {}
    cons = run(row, exe, "%d@300" % warp, fx, {"SM64DS_COOP_TP_FAKE": "0@%d" % TPTICK})
    fb, fa = cons[0], cons[1]
    CR.common(row, cons)
    ta, tb = fa.text(), fb.text()
    if arm == "base":
        lvA = [m for m in CR.bits_of(ta) if m.group(2) == str(warp)]
        say(not lines(ta, r"\[cotp\]") and not lvA, "%s: CONTROL base exe: no [cotp], A never enters level %d (nothing happens)" % (row, warp))
        return
    info("%s B knobs: %s" % (row, CR.knobs(tb)))
    ka, kb = landing(row, ta, tb, warp, 1)
    fe = first_est(ta, warp)
    info("%s A first (landing) bitmap %s" % (row, fe and fe[:2]))
    if not locked:
        say(bool(fe and kb and kb[2] == fe[2] and fe[1] >= 4), "%s: A's census of dead equals B's (landing bitmap == B's final, n=%s: dead stay dead)" % (row, fe and fe[1]))
        cA = [m for m in re.findall(r"(?m)^\[coop\] census level=%d act=(\d+) actors=(\d+) slotted=(\d+)" % warp, ta)]
        cB = [m for m in re.findall(r"(?m)^\[coop\] census level=%d act=(\d+) actors=(\d+) slotted=(\d+)" % warp, tb)]
        info("%s census A %s | B %s" % (row, cA[:1], cB[:1]))
        say(bool(cA and cB) and int(cB[0][2]) - int(cA[0][2]) == 4, "%s: A's first census lacks exactly the 4 killed actors (B %s, A %s)" % (row, cB[:1], cA[:1]))
        say(any("snapshot from seat 0" in l for l in lines(ta, r"\[coop\] joined")), "%s: A took the late-join snapshot from B" % row)
    else:
        info("%s stars_total in presence log: %s" % (row, lines(ta, r"stars")[:2]))
    import r3gw as R
    R.bmp_png(fa, "%s_A_end.png" % row)


def row_tpoff(arm="new"):
    row = "tpoff"
    cons = run(row, EXE, "%d@300" % BOB, {"SM64DS_COOP_TP": "0"}, {"SM64DS_COOP_TP_FAKE": "0@%d" % TPTICK})
    fb, fa = cons[0], cons[1]
    CR.common(row, cons)
    ta = fa.text()
    info("%s A lines: %s" % (row, lines(ta, r"\[cotp\]")[:6]))
    say(bool(lines(ta, r"host setting from seat 0: TP OFF")), "%s: A heard the host switch TP off" % row)
    say(not lines(ta, r"\[cotp\] badge seat 0 ON"), "%s: toggle off = no badge on A" % row)
    say(not lines(ta, r"\[cotp\] TP to seat") and not [m for m in CR.bits_of(ta) if m.group(2) == str(BOB)],
        "%s: toggle off = the click does nothing, A stays out of BOB" % row)
    import r3gw as R
    R.bmp_png(fa, "%s_A.png" % row)


def row_tpparty():
    row = "tpparty"
    cons = run(row, EXE, "%d@300" % BOB, {}, {"SM64DS_COOP_TP_FAKE": "0@%d" % TPTICK}, kind="party", frames=1500)
    CR.common(row, cons)
    ta = cons[1].text()
    say(not lines(ta, r"\[cotp\]") and not [m for m in CR.bits_of(ta)], "%s: party room = no badge, no TP, no coop code at all" % row)
    info("%s A plist: %s" % (row, lines(ta, r"\[plist\]")[:3]))


def row_refuse(which):
    """menu: the friend pauses (Start) before the click -> refusal. me: I pause before my own click -> refusal."""
    row = "refuse_" + which
    fx, mx = {}, {"SM64DS_COOP_TP_FAKE": "0@%d" % TPTICK}
    if which == "menu":
        fx["SM64DS_HOST_PAD"] = "10@%d" % (TPTICK - 200)
    else:
        mx["SM64DS_HOST_PAD"] = "10@%d" % (TPTICK - 100)
    cons = run(row, EXE, "%d@300" % BOB, fx, mx)
    CR.common(row, cons)
    ta = cons[1].text()
    say = G.say
    info("%s A cotp: %s" % (row, lines(ta, r"\[cotp\]")[:8]))
    refused = lines(ta, r"\[cotp\] say: Can't TP")
    say(bool(refused), "%s: refused with a chat line (%s)" % (row, refused[:1]))
    say(not lines(ta, r"\[cotp\] TP to seat"), "%s: no warp was started and nothing queued" % row)
    say(not [m for m in CR.bits_of(ta) if m.group(2) == str(BOB)], "%s: A is still out of BOB" % row)


def row_same():
    row = "refuse_same"
    cons = run(row, EXE, "%d@300" % BOB, {}, {"SM64DS_COOP_TP_FAKE": "0@%d" % TPTICK}, me_warp="%d@320" % BOB)
    CR.common(row, cons)
    ta = cons[1].text()
    say(bool(lines(ta, r"say: You are already with")) and not lines(ta, r"\[cotp\] TP to seat"), "%s: already in the friend's level+act: refused, no reload" % row)


def row_alone():
    row = "alone"
    cons, st = CR.launch(row, [dict(warp="%d@300" % BOB, extra=tp_env({"SM64DS_COOP_TP_FAKE": "0@600"}), frames=1000)])
    CR.common(row, cons)
    t = cons[0].text()
    say(not lines(t, r"\[cotp\] TP to seat"), "%s: a lone coop console never TPs (no friend)" % row)
    info("%s: %s" % (row, lines(t, r"\[cotp\]")[:4]))


if __name__ == "__main__":
    for r in sys.argv[1:]:
        name, _, arg = r.partition(":")
        arm = "base" if arg == "base" else "new"
        if name == "badge": row_badge(arm)
        elif name == "tp": row_tp(arm)
        elif name == "tpmid": row_tp(arm, warp=int(os.environ.get("CTP_MID", "8")))
        elif name == "tplocked": row_tp(arm, warp=int(os.environ.get("CTP_LOCKED", "28")), locked=True)
        elif name == "tpoff": row_tpoff()
        elif name == "tpparty": row_tpparty()
        elif name == "refuse_menu": row_refuse("menu")
        elif name == "refuse_me": row_refuse("me")
        elif name == "refuse_same": row_same()
        elif name == "alone": row_alone()
        else: print("unknown row", name)
    print("\n".join(G.RES))
