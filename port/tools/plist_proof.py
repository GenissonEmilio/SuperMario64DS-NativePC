#!/usr/bin/env python3
"""PLIST1 rows (run mp1 wave 56): the party / coop player list across 2 to 4 consoles through a LOCAL relay copy,
started the way the launcher starts them (title path, the adventure env + SM64DS_ROOM_KIND, no SM64DS_LEVEL; a
seeded save; SM64DS_WARP_SEQ only moves a console to a level at a chosen frame).

It rides the run's console rig (lgc.py / r3gw.py / deskspawn.py: a private desktop, hidden and muted, ports from this
process's PID). Point PLIST_RIG at the directory holding them. Other environment:
  PLIST_EXE     the exe to run (default: the rig's)        PLIST_RELAY   relay.py (default: this tree's)
  PLIST_OUT     where the console folders go               PLIST_SHOTS   where pictures go
  PLIST_LOCK    ladder lock to hold while consoles run (default C:/tmp/mp1/ladder2.lock; "" = none)

  python plist_proof.py room:<kind>:<n>:<tag>[:k=v;k=v]       kind party | coop | world
    options  frames=900  scale=1|2|3  stacked=1  chat=1  handler=1  warp<i>=6@300  star<i>=1  click<i>=<seconds after the list shows every player>@<x>,<y>[|<seconds>@<x>,<y>...]
             (handler=1 installs the TEST click handler on every console; click<i> posts a real left-button message to
              console i's window <seconds> after it started, at framebuffer pixel x,y)
  python plist_proof.py table                                 the stage code table checks (no game)
"""
import ctypes
import ctypes.wintypes as wt
import os
import re
import sys
import time

RIG = os.environ.get("PLIST_RIG")
if not RIG:
    sys.exit("set PLIST_RIG to the directory holding lgc.py, r3gw.py and deskspawn.py")
sys.path.insert(0, RIG)
import lgc as G  # noqa: E402
import r3gw as R  # noqa: E402
import deskspawn as D  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
R.RELAY = os.environ.get("PLIST_RELAY", os.path.join(HERE, "relay", "relay.py"))
SHOTS = os.environ.get("PLIST_SHOTS", os.path.join(os.getcwd(), "shots"))
os.makedirs(SHOTS, exist_ok=True)
say, info = G.say, G.info

U32 = ctypes.WinDLL("user32", use_last_error=True)
WM_LBUTTONDOWN, WM_LBUTTONUP = 0x0201, 0x0202


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
    e["SM64DS_PLIST_LOG"] = "1"
    e["SM64DS_PLAYER_NAME"] = ("Tango", "Opie", "Kirb", "MoMo")[i]
    # one of each character on the four seats
    e["SM64DS_PLAYER_COLOR"] = ("e04040", "40c060", "e0c030", "60a0f0")[i]
    w = o.get("warp%d" % i, "1@300")
    if w:
        e["SM64DS_WARP_SEQ"] = w
    if o.get("star%d" % i):
        e["SM64DS_STAR_FILTER"] = o["star%d" % i]
    if o.get("scale"):
        e["SM64DS_RENDER_SCALE"] = o["scale"]
    if o.get("handler"):
        e["SM64DS_PLIST_TEST_HANDLER"] = "1"
    if o.get("chat"):
        f = int(o.get("frames", "900")) - 400      # late enough to still be on screen at the last frame
        e["SM64DS_CHAT_TEST"] = ("%d:hello from the castle grounds, this line is long enough to wrap around the log;"
                                 "%d:second line;%d:third line" % (f, f + 30, f + 60))
    # the seeded save plays Yoshi; SM64DS_SWITCH picks another character at the first stage boot, so four consoles
    # show all four heads: seat 0 Yoshi, seat 1 Mario, seat 2 Luigi, seat 3 Wario
    sw = o.get("char%d" % i, ("", "0", "1", "2")[i])
    if sw != "":
        e["SM64DS_SWITCH"] = sw
    return e


def find_windows(pid):
    """top-level windows of `pid` on the private desktop the rig made"""
    out = []
    PROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)

    def cb(h, _):
        p = wt.DWORD()
        U32.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid:
            out.append(h)
        return True
    U32.EnumDesktopWindows(D._desk["h"], PROC(cb), 0)
    return out


def post_click(pid, fx, fy, fbw, fbh):
    """a real left-button down + up message at FRAMEBUFFER pixel (fx, fy): the picture (fbw x fbh) is fitted and
    centred in the window's client area, so the client point is the forward map of that fit"""
    hs = find_windows(pid)
    if not hs:
        return None
    r = wt.RECT()
    U32.GetClientRect(hs[0], ctypes.byref(r))
    cw, ch = r.right - r.left, r.bottom - r.top
    if cw <= 0 or ch <= 0:
        # a minimized window reports no client area; the game still presents into the size it was created with
        # (measured: client pixel = 2 x framebuffer pixel at the default 1024 x 768), so use that
        cw, ch = int(os.environ.get("PLIST_CLIENT_W", "1024")), int(os.environ.get("PLIST_CLIENT_H", "768"))
    k = min(cw / float(fbw), ch / float(fbh))
    ox, oy = (cw - fbw * k) / 2.0, (ch - fbh * k) / 2.0
    cx, cy = int(ox + (fx + 0.5) * k), int(oy + (fy + 0.5) * k)
    lp = (cy << 16) | (cx & 0xFFFF)
    for h in hs:
        U32.PostMessageW(h, WM_LBUTTONDOWN, 1, lp)
        U32.PostMessageW(h, WM_LBUTTONUP, 0, lp)
    return (cx, cy, cw, ch)


ROWRE = re.compile(r"^\[plist\] t=(\d+) rows=(\d+)(.*)$", re.M)
ONE = re.compile(r"\| seat=(\d)(\(me\))? name=(.*?) char=(\d) code=(.*?) (here|away)")


def plist_rows(t):
    """[(t, [(seat, me, name, char, code, here)])] from the console's [plist] lines"""
    out = []
    for m in ROWRE.finditer(t):
        rows = [(int(a), bool(b), c, int(d), e, f == "here") for a, b, c, d, e, f in ONE.findall(m.group(3))]
        out.append((int(m.group(1)), rows))
    return out


def shot(con, tag):
    from PIL import Image
    bmp = os.path.join(con.dir, "walk_window_selftest.bmp")
    stk = os.path.join(con.dir, "stack_end.bmp")
    src = stk if os.path.exists(stk) else bmp
    if not os.path.exists(src):
        info("no picture for %s" % tag)
        return None
    png = os.path.join(SHOTS, "%s.png" % tag)
    Image.open(src).save(png)
    info("picture %s (bmp md5 %s)" % (png, G.md5(src)[:8]))
    return png


def run_room(kind, n, tag, o):
    frames = int(o.get("frames", "900"))
    row = "%s_%s" % (kind, tag)
    rp, rf, port, rlog = R.start_relay(row)
    relay = ("127.0.0.1:%d" % port, "PL%d" % (G.PID % 1000))
    cons = []
    t0 = time.time()
    for i in range(n):
        e = env_for(i, relay, kind, o)
        if o.get("stacked"):
            e["SM64DS_DUAL_SCREEN"] = "1"
            e["SM64DS_STACK_BMP"] = os.path.join(RIGOUT(row, i), "stack_end.bmp") + ",%d" % (frames - 2)
        if kind == "world":
            e.pop("SM64DS_PARTY", None)
            e.pop("SM64DS_COOP", None)
        cons.append(G.Con(row, "p%d" % i, e, frames, exe=os.environ.get("PLIST_EXE")))
        time.sleep(0.5)
    # scripted clicks
    pending = []
    for i in range(n):
        for c in (o.get("click%d" % i) or "").split("|"):
            if c:
                secs, _, xy = c.partition("@")
                pending.append([float(secs), i, [int(v) for v in xy.split(",")]])
    dl = time.time() + 1700
    seen_at = {}
    while any(c.p.poll() is None for c in cons) and time.time() < dl:
        for pc in list(pending):
            i = pc[1]
            if i not in seen_at and ("rows=%d" % n) in cons[i].text():
                seen_at[i] = time.time()       # the list now holds every player: clicks count from here
            if i in seen_at and time.time() - seen_at[i] >= pc[0]:
                rs = int(o.get("scale", "1"))
                ok = post_click(cons[i].p.pid, pc[2][0], pc[2][1], 512 * rs, 384 * rs * (2 if o.get("stacked") else 1))
                info("%s: posted a left click to console %d at framebuffer %s -> client (x, y, w, h) %s" % (row, i, pc[2], ok))
                pending.remove(pc)
        time.sleep(0.5)
    G.finish(cons, 60)
    R.stop_relay(rp, rf)
    return row, cons


def RIGOUT(row, i):
    return os.path.join(G.OUT, row, "p%d" % i)


def lock():
    p = os.environ.get("PLIST_LOCK", "C:/tmp/mp1/ladder2.lock")
    if not p:
        return None
    os.environ["SM64DS_BUILD_LOCK_PATH"] = p
    sys.path.insert(0, os.path.join(TREE, "port", "tools"))
    import build_lock
    build_lock.acquire(label="plist1-rows", root=TREE, timeout=14400)
    return build_lock, p


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
        shot(c, "%s_p%d" % (row, i))
    say(all(c.rc == 0 and not G.faults(t) for c, t in zip(cons, texts)), "%s: every console exited 0, no fault" % row)
    return row, cons, texts


def main(argv):
    for a in argv:
        p = a.split(":")
        if p[0] == "room":
            kind, n, tag = p[1], int(p[2]), p[3]
            o = kv(p[4]) if len(p) > 4 else {}
            row, cons, texts = row_room(kind, n, tag, o)
            for i, t in enumerate(texts):
                pr = plist_rows(t)
                info("%s p%d: %d [plist] changes; last rows %s" % (row, i, len(pr), pr[-1][1] if pr else None))
                for ln in re.findall(r"(?m)^\[plist\].*(?:click|handler|head art|TEST).*$|^\[mouse\].*$", t)[:12]:
                    info("%s p%d: %s" % (row, i, ln[:200]))
    bad = [r for r in G.RES if r.startswith("FAIL")]
    print("\n%d PASS %d FAIL" % (len([r for r in G.RES if r.startswith("PASS")]), len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
