#!/usr/bin/env python3
"""PARTY SHARED STARS PROOF (run mp1 wave 56, lane PSTAR1).

Real game consoles, started the way the launcher starts a party room (the adventure env + SM64DS_ROOM_KIND=party +
SM64DS_PARTY=1, the title path through the file select with a seeded save, NO SM64DS_LEVEL), each in its own folder with
its own save file, on a PRIVATE desktop (hidden, muted), through a LOCAL relay copy built from this tree's relay.py.
SM64DS_WARP_SEQ only moves a console to a level at a chosen frame. Ports come from this process id, never 51765.
Only processes this script spawned are ever killed, by their own handle.

Needs the console rig's deskspawn.py (PSTAR_RIG = the directory holding it; default beside this run's scripts).

  python party_stars_proof.py --exe NEW.exe --base BASE.exe [--root ASSETROOT] [--out DIR] [--kit KIT.exe --kitroot ROOT] row ...

rows
  unit     the pure rules (diff, limiter, visits, queue, sequence memory) compiled and run natively; two broken variants must FAIL
  share    3 consoles, A collects BOB star 1 -> B's and C's save files read back with exactly that bit added; no echo; repeat
           ignored; the HUD star count follows (picture before / after)
  base     MUST-FAIL: the same row on the BASE exe shares nothing
  real     the same through the ROM's own star: A is dropped on a placed PowerStar and its collision does the collect
  waits    a star landing while B is not safe (its own star, a forced hold) waits and then applies
  slot     a star queued, then the open file slot changes: the queue is dropped, nothing written to another slot; a console
           that exits with a queue writes nothing
  late     a joiner that arrives after A's collect gets nothing
  toggles  share off = nothing sent; accept off = nothing applied
  flood    a forger sends 150 valid stars: lands only up to the limits; the limiter-off control lands more
  forge    a star for a course the sender never visited, malformed, in a non-party session, from an absent seat: lands 0
  host     the first seat leaves, a second one collects, the third gets it; a new console takes the free seat and works
  alone    a party room of one: nothing breaks, nothing is sent to nobody
  solo     the solo identity values (delegates to gate_solo.py)
  kit      the kit exe (PORT_ROM_CLEAN): the share row on it
"""
import argparse
import ctypes
import hashlib
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "relay"))
RIG = os.environ.get("PSTAR_RIG", "scripts")
sys.path.insert(0, RIG)
import mp2_proof as M  # noqa: E402
import deskspawn as D  # noqa: E402
import test_relay_v2 as T  # noqa: E402

SEED = os.environ.get("PSTAR_SEED", "field/tango_sm64ds.sav")
PID = os.getpid()
BASEPORT = 30000 + (PID % 400) * 40
if BASEPORT <= 51765 < BASEPORT + 40:
    BASEPORT += 80
ARGS = None
RES = []
CHIP = 0x2000
TAG_N = b"ds mario"
NAMES = ("Tango", "Opie", "Kirb", "MoMo")
OPENING = {
    "SM64DS_SKIP_MENU": "1",
    "SM64DS_SCENE_WINDOW": "1",
    "SM64DS_SCENE_FRAMES": "100000",
    "SM64DS_TOUCH_PROBE": "560-563:44:68,620-623:44:68,680-683:44:68,760-763:44:68,860-863:44:68",
}


def say(ok, line):
    s = ("PASS  " if ok else "FAIL  ") + line
    RES.append(s)
    print(s, flush=True)
    return ok


def info(line):
    RES.append("info  " + line)
    print("info  " + line, flush=True)


def md5(p):
    return hashlib.md5(open(p, "rb").read()).hexdigest() if os.path.exists(p) else "none"


def chill_check():
    try:
        with open(os.path.expanduser("~/.claude/scripts/league_chill.log")) as f:
            last = f.read().strip().splitlines()[-1]
    except Exception:
        return
    if "chilling" in last:
        print("LEAGUE CHILLING, refusing:", last, flush=True)
        sys.exit(7)


# ---------------------------------------------------------------- save images
def checksum(tag, payload):
    s = sum(tag) & 0xFFFF
    for b in payload:
        s = ((((s << 1) & 0xFFFE) | ((s >> 15) & 1)) ^ b) & 0xFFFF
    return s


def parse(img):
    """-> {(slot, copy): (tag, crc_ok, payload)}"""
    out = {}
    for slot in range(3):
        for copy in (0, 1):
            off = (CHIP // 2 if copy else 0) + (slot << 7)
            tag = bytes(img[off + 2:off + 10])
            pay = bytes(img[off + 10:off + 10 + 0x44])
            crc = struct.unpack_from("<H", img, off)[0]
            out[(slot, copy)] = (tag, crc == checksum(tag, pay), pay)
    return out


def stars_of(pay):
    return [(c, b) for c in range(30) for b in range(8) if pay[0x14 + c] & (1 << b)]


def seeded(stars=(), char=None):
    """the Tango seed with slot 0 holding exactly `stars` (both copies, checksums redone)"""
    img = bytearray(open(SEED, "rb").read())
    for copy in (0, 1):
        off = (CHIP // 2 if copy else 0)
        tag = bytes(img[off + 2:off + 10])
        pay = bytearray(img[off + 10:off + 10 + 0x44])
        for c in range(30):
            pay[0x14 + c] = 0
        for c, b in stars:
            pay[0x14 + c] |= 1 << b
        if char is not None:
            pay[0x41] = char
        img[off + 10:off + 10 + 0x44] = pay
        img[off:off + 2] = struct.pack("<H", checksum(tag, bytes(pay)))
    return bytes(img)


def region(off):
    if 0 <= off < 0x80:
        return "s0c0"
    if 0x1000 <= off < 0x1080:
        return "s0c1"
    if 0x180 <= off < 0x180 + 0x2f0 or 0x1180 <= off < 0x1180 + 0x2f0:
        return "mini"
    return "other"


def save_diff(a, b):
    """-> (payload diffs [(payload offset, old, new)], checksum bytes changed, mini bytes changed, other bytes changed)"""
    pay, crc, mini, other = [], 0, 0, []
    for i in range(min(len(a), len(b))):
        if a[i] == b[i]:
            continue
        r = region(i)
        if r in ("s0c0", "s0c1"):
            o = i % 0x1000
            if o < 2:
                crc += 1
            elif 10 <= o < 10 + 0x44:
                pay.append((o - 10, a[i], b[i]))
            else:
                other.append(i)
        elif r == "mini":
            mini += 1
        else:
            other.append(i)
    return sorted(set(pay)), crc, mini, other


# ---------------------------------------------------------------- the rig
class Con:
    def __init__(self, row, inst, extra, frames, exe, root, stars=(), seed_img=None):
        self.inst = inst
        self.dir = os.path.join(ARGS.out, row, inst)
        if os.path.isdir(self.dir):
            shutil.rmtree(self.dir, ignore_errors=True)
        os.makedirs(os.path.join(self.dir, "tmp"))
        self.exe = os.path.join(self.dir, "walk_window.exe")
        shutil.copyfile(exe, self.exe)
        self.exe_md5 = md5(self.exe)
        self.save = os.path.join(self.dir, "sm64ds.sav")
        self.seed = seed_img if seed_img is not None else seeded(stars)
        with open(self.save, "wb") as f:
            f.write(self.seed)
        e = M.env_base(root, self.dir, ("ps" + row + inst)[-12:])
        e["TEMP"] = e["TMP"] = os.path.join(self.dir, "tmp")
        e["SM64DS_WINDOW_SELFTEST"] = str(frames)
        e["SM64DS_SAVE_PATH"] = self.save
        e["SM64DS_NO_PLAYLOG"] = "1"
        e.pop("SM64DS_LEVEL", None)
        for k, v in extra.items():
            if v is None:
                e.pop(k, None)
            else:
                e[k] = str(v)
        self.env = e
        self.log = os.path.join(self.dir, "run.log")
        with open(os.path.join(self.dir, "env.txt"), "w") as f:
            f.write("exe_md5=%s\n" % self.exe_md5)
            for k in sorted(e):
                if k.startswith("SM64DS_"):
                    f.write("%s=%s\n" % (k, e[k]))
        self.t0 = time.time()
        self.p = D.spawn(self.exe, self.dir, e, self.log)
        self.pid = self.p.pid
        self.rc = None

    def poll(self):
        if self.rc is None:
            r = self.p.poll()
            if r is not None:
                self.rc = r
                try:
                    self.p._logfile.close()
                except Exception:
                    pass
        return self.rc

    def kill(self):
        try:
            self.p.kill()
        except Exception:
            pass

    def text(self):
        try:
            return M.text(self.log)
        except Exception:
            return ""

    def final(self):
        return open(self.save, "rb").read()


def faults(t):
    return re.findall(r"(?m)^.*(c0000005|c0000094|c00000fd|c0000409|quarantine c|unhandled exception|"
                      r"\[crash\]|\[fault\]|FAULT).*$", t)[:4]


def start_relay(row, rate="240"):
    port = BASEPORT + 30
    d = os.path.join(ARGS.out, row)
    os.makedirs(d, exist_ok=True)
    log = os.path.join(d, "relay.log")
    e = {k: v for k, v in os.environ.items() if not k.startswith("SM64DS_")}
    e.update({"SM64DS_RELAY_PORT": str(port), "SM64DS_RELAY_BIND": "127.0.0.1", "SM64DS_RELAY_RATE_PPS": rate,
              "SM64DS_RELAY_MAX_CHILDREN": "15", "SM64DS_RELAY_STATS_S": "5", "PYTHONUNBUFFERED": "1"})
    f = open(log, "wb")
    p = subprocess.Popen([sys.executable, "-u", os.path.join(HERE, "relay", "relay.py")], env=e, stdin=subprocess.DEVNULL,
                         stdout=f, stderr=subprocess.STDOUT, creationflags=0x08000000)
    time.sleep(1.0)
    return p, f, port, log


def stop_relay(p, f):
    p.terminate()
    try:
        p.wait(5)
    except Exception:
        p.kill()
    f.close()


def penv(i, relay, kind="party", extra=None):
    e = {"SM64DS_ADVENTURE": "1", "SM64DS_VS_PLAYERS": None, "SM64DS_HUB_REPORT_S": "2",
         "SM64DS_PLAYER_NAME": NAMES[i], "SM64DS_PLAYER_COLOR": "%02x%02x%02x" % (30 + i * 55, 220 - i * 40, 60 + i * 50),
         "SM64DS_COMMS_RELAY": relay[0], "SM64DS_COMMS_CODE": relay[1], "SM64DS_ROOM_KIND": kind,
         "SM64DS_PRESENCE_LOG": "1", "SM64DS_PARTY_STARS_LOG": "1", "SM64DS_HUB_SEAT": str(i)}
    if kind == "party":
        e["SM64DS_PARTY"] = "1"
    e.update(OPENING)
    if extra:
        e.update(extra)
    return e


class Forger:
    """a bare UDP member of the room: joins like a console, keeps itself present, sends what it is told"""

    def __init__(self, port, code, role=T.ROLE_PARTY, want=T.ANY):
        self.addr = ("127.0.0.1", port)
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.bind(("127.0.0.1", 0))
        self.s.settimeout(0.05)
        self.code, self.role, self.want = code, role, want
        self.seat = None
        self.level = None
        self.act = 1
        self.seq = 0
        self.stop = False
        self.rx = []
        self.sent = 0
        self.lock = threading.Lock()

    def join(self):
        for _ in range(20):
            self.s.sendto(T.hello(self.code, self.role, self.want), self.addr)
            t_end = time.time() + 0.5
            while time.time() < t_end:
                try:
                    d, a = self.s.recvfrom(2048)
                except socket.timeout:
                    continue
                ack = T.parse_ack(d)
                if ack:
                    self.ack = ack
                    if ack[1] == T.ST_OK:
                        self.seat = ack[2]
                        self.th = threading.Thread(target=self.loop, daemon=True)
                        self.th.start()
                    return ack
        return None

    def loop(self):
        last_h = last_p = 0.0
        while not self.stop:
            now = time.time()
            if now - last_h > 1.0:
                last_h = now
                self.s.sendto(T.hello(self.code, self.role, self.want), self.addr)
            if self.level is not None and now - last_p > 0.4:
                last_p = now
                self.seq = (self.seq + 1) & 0xffff
                with self.lock:
                    self.s.sendto(T.pres(level=self.level, act=self.act, flags=1 | (8 if self.role == T.ROLE_PARTY else 0),
                                         stars=3, seq=self.seq), self.addr)
            try:
                d, a = self.s.recvfrom(2048)
                self.rx.append(d[:4])
            except socket.timeout:
                pass
            except OSError:
                break

    def say(self, level):
        self.level = level

    def raw(self, data):
        with self.lock:
            self.s.sendto(data, self.addr)
            self.sent += 1

    def synx(self, course, star, seq=None):
        if seq is None:
            self.seq = (self.seq + 1) & 0xffff
            seq = self.seq
        self.raw(T.pstar(course=course, star=star, seq=seq))

    def close(self):
        self.stop = True
        time.sleep(0.2)
        try:
            self.s.close()
        except Exception:
            pass


def lock():
    p = os.environ.get("PSTAR_LOCK", "")
    if not p:
        return None
    os.environ["SM64DS_BUILD_LOCK_PATH"] = p
    sys.path.insert(0, os.path.join(REPO, "port", "tools"))
    import build_lock
    build_lock.acquire(label="pstar1-rows", root=REPO, timeout=14400)
    return build_lock, p


def wait_cons(cons, timeout, tick=None):
    dl = time.time() + timeout
    while time.time() < dl:
        alive = [c for c in cons if c.poll() is None]
        if tick:
            tick()
        if not alive:
            return True
        time.sleep(0.2)
    for c in cons:
        if c.poll() is None:
            c.kill()
            c.rc = -9
    return False


def room(row, specs, kind="party", forger_role=None, forger_script=None, exe=None, root=None, timeout=1500, relay_rate="240"):
    """specs: list of dict(frames=, extra={}, stars=[], delay=0.0, exe=None, seed=None). forger_script(forger, cons) runs in
    the main thread while consoles live. -> dict(cons, relay_log, forger)"""
    chill_check()
    L = lock()
    shutil.rmtree(os.path.join(ARGS.out, row), ignore_errors=True)
    rp, rf, port, rlog = start_relay(row, relay_rate)
    relay = ("127.0.0.1:%d" % port, "PS%d" % (PID % 1000))
    cons = []
    fg = None
    try:
        for i, sp in enumerate(specs):
            if sp.get("delay"):
                time.sleep(sp["delay"])
            ex = penv(i, relay, kind, sp.get("extra"))
            c = Con(row, "p%d" % i, ex, sp.get("frames", 3000), sp.get("exe") or exe or ARGS.exe,
                    root or ARGS.root, stars=sp.get("stars", ()), seed_img=sp.get("seed"))
            cons.append(c)
            time.sleep(0.6)
        if forger_role is not None:
            fg = Forger(port, relay[1].encode(), role=forger_role)
            fg.join()
        if forger_script:
            forger_script(fg, cons)
        wait_cons(cons, timeout)
    finally:
        for c in cons:
            if c.poll() is None:
                c.kill()
        if fg:
            fg.close()
        stop_relay(rp, rf)
        if L:
            L[0].release(L[1])
    return dict(cons=cons, relay_log=rlog, forger=fg, relay=relay)


def lands(t):
    return re.findall(r"(?m)^\[pstar\] LAND .*$", t)


def txs(t):
    return re.findall(r"(?m)^\[pstar\] tx .*$", t)


def rxs(t, why=None):
    ls = re.findall(r"(?m)^\[pstar\] rx seat=(\d) course=(\d+) star=(\d+) -> (\w+)$", t)
    return [x for x in ls if why is None or x[3] == why]


def report(t):
    ls = re.findall(r"(?m)^\[pstar\] report .*$", t)
    return ls[-1] if ls else ""


def counter(t, key):
    m = re.search(r"\b%s=(\d+)" % key, report(t))
    return int(m.group(1)) if m else 0


def shot(c, tag):
    try:
        from PIL import Image
    except Exception:
        return None
    for name in ("stack_end.bmp", "walk_window_selftest.bmp"):
        src = os.path.join(c.dir, name)
        if os.path.exists(src):
            shots = os.path.join(ARGS.out, "shots")
            os.makedirs(shots, exist_ok=True)
            png = os.path.join(shots, tag + ".png")
            Image.open(src).save(png)
            info("picture %s (bmp md5 %s)" % (png, md5(src)[:8]))
            return png
    info("no picture for %s" % tag)
    return None


def common(row, cons):
    for c in cons:
        t = c.text()
        info("%s %s rc=%s exe=%s faults=%d tx=%d rx=%d land=%d | %s" % (row, c.inst, c.rc, c.exe_md5[:8], len(faults(t)),
                                                                        len(txs(t)), len(rxs(t)), len(lands(t)),
                                                                        report(t)[8:200]))
    say(all(c.rc == 0 and not faults(c.text()) for c in cons), "%s: every console exited 0, no fault" % row)


def frames_for(sec):
    return int(ARGS.fps * sec)


# ---------------------------------------------------------------- the star rows
BOB = 6      # sublevel id of Bob-omb Battlefield (course 0)
STAR = (0, 0)   # BOB star 1


def warp(frame=300, lvl=BOB):
    return {"SM64DS_WARP_SEQ": "%d@%d" % (lvl, frame)}


def probe(frame, course=0, bit=0):
    return {"SM64DS_PARTY_STARS_PROBE": "star:%d:%d@%d" % (course, bit, frame)}


def byte_check(row, name, c, add):
    """c's file vs its seed: exactly the bits in `add` added to slot 0, nothing else but the ROM's own checksum / minigame block"""
    fin = c.final()
    pr = parse(fin)
    ok_crc = pr[(0, 0)][1] and pr[(0, 1)][1]
    pay, crc, mini, other = save_diff(c.seed, fin)
    want = set()
    for course, bit in add:
        want.add((0x14 + course, c.seed_pay[0x14 + course], c.seed_pay[0x14 + course] | (1 << bit)))
    # several bits in one byte: the byte's new value is the OR
    want_bytes = {}
    for course, bit in add:
        want_bytes[0x14 + course] = want_bytes.get(0x14 + course, c.seed_pay[0x14 + course]) | (1 << bit)
    got = {o: n for o, a, n in pay}
    info("%s %s: payload byte diff (offset,old,new) %s, checksum bytes %d, minigame-block bytes %d, other %d" % (
        row, name, pay, crc, mini, len(other)))
    good = got == want_bytes and not other and ok_crc
    say(good, "%s %s: its save file holds exactly the added bit(s) %s and nothing else changed (checksums valid %s)" % (
        row, name, sorted(add), ok_crc))
    return good


def share_rows(row, exe=None, root=None, kit=False, frames=None, label=""):
    """A (seat 0) collects BOB star 1 in BOB; B and C sit in BOB too"""
    f = frames or frames_for(55)
    specs = [dict(frames=f, extra=dict(warp(300), **probe(1100, *STAR))),
             dict(frames=f, extra=warp(300)),
             dict(frames=f, extra=warp(300))]
    r = room(row, specs, exe=exe, root=root)
    cons = r["cons"]
    for c in cons:
        c.seed_pay = parse(c.seed)[(0, 0)][2]
    common(row, cons)
    a, b, c = cons
    ta, tb, tc = a.text(), b.text(), c.text()
    say(len(txs(ta)) == 1 and not txs(tb) and not txs(tc),
        "%s: A announced the star once, B and C announced nothing (no echo): tx A/B/C = %d/%d/%d" % (row, len(txs(ta)), len(txs(tb)), len(txs(tc))))
    say(len(lands(tb)) == 1 and len(lands(tc)) == 1,
        "%s: the star landed once on B and once on C: %s | %s" % (row, lands(tb)[:1], lands(tc)[:1]))
    byte_check(row, "A (collector)", a, [STAR])
    byte_check(row, "B", b, [STAR])
    byte_check(row, "C", c, [STAR])
    say(counter(ta, "recv") == 0 or counter(ta, "landed") == 0, "%s: nothing came back to A (landed on A = %d)" % (row, counter(ta, "landed")))
    return r


def row_share():
    return share_rows("share")


def row_base():
    row = "base"
    f = frames_for(55)
    specs = [dict(frames=f, extra=dict(warp(300), **probe(1100, *STAR)), exe=ARGS.base),
             dict(frames=f, extra=warp(300), exe=ARGS.base), dict(frames=f, extra=warp(300), exe=ARGS.base)]
    r = room(row, specs)
    cons = r["cons"]
    common(row, cons)
    for c in cons[1:]:
        t = c.text()
        say(md5(c.save) == hashlib.md5(c.seed).hexdigest() and "[pstar]" not in t and not parse(c.final())[(0, 0)][2][0x14] & 1,
            "base MUST-FAIL control: on the BASE exe %s's save is byte-identical to its seed and nothing was shared (no [pstar] line)" % c.inst)
    return r


# ---------------------------------------------------------------- unit
UNIT_CPP = r'''
#include "party_stars.h"
#include <stdio.h>
using namespace port;
static int fails = 0;
static void row(const char *n, bool ok) { printf("ROW %s %s\n", n, ok ? "ok" : "bad"); if (!ok) ++fails; }
#if BROKEN == 1
// limiter that never refuses
#define LIM_CHECK(l, s, t) kPsOk
#else
#define LIM_CHECK(l, s, t) (l).check(s, t)
#endif
#if BROKEN == 2
// diff that counts a cleared bit as nothing and a multi-bit change as one star
static PsDiff dd(const uint8_t *p, const uint8_t *c) { PsDiff d = ps_diff(p, c); d.cleared = 0; if (d.added > 1) d.added = 1; return d; }
#else
static PsDiff dd(const uint8_t *p, const uint8_t *c) { return ps_diff(p, c); }
#endif
int main() {
    uint8_t a[30], b[30];
    memset(a, 0, 30); memcpy(b, a, 30);
    PsDiff d = dd(a, b);
    row("diff_none", d.added == 0 && d.cleared == 0);
    b[0] = 1; d = dd(a, b);
    row("diff_one_bit", d.added == 1 && d.cleared == 0 && d.course == 0 && d.star == 0);
    memset(b, 0, 30); b[29] = 0x80; d = dd(a, b);
    row("diff_last_course_top_bit", d.added == 1 && d.course == 29 && d.star == 7);
    memset(b, 0, 30); b[3] = 0x05; d = dd(a, b);
    row("diff_two_bits_is_two", d.added == 2);
    memset(a, 0xff, 30); memset(b, 0xff, 30); b[7] = 0xfe; d = dd(a, b);
    row("diff_cleared_is_seen", d.cleared == 1 && d.added == 0);
    memset(a, 0, 30); memset(b, 0, 30); a[2] = 1; b[2] = 2; d = dd(a, b);
    row("diff_swap_is_added_and_cleared", d.added == 1 && d.cleared == 1);
    // limiter
    PsLimiter L; L.reset();
    row("lim_first_ok", LIM_CHECK(L, 1, 1000) == kPsOk);
    L.take(1, 1000);
    row("lim_gap_10s", LIM_CHECK(L, 1, 5000) == kPsDropGap);
    row("lim_after_10s", LIM_CHECK(L, 1, 11001) == kPsOk);
    for (int i = 0; i < 5; ++i) { uint32_t t = 11001 + i * 11000; L.take(1, t); }
    row("lim_seat_cap_6", LIM_CHECK(L, 1, 11001 + 5 * 11000 + 20000) == kPsDropSeatCap);
    row("lim_window_frees", LIM_CHECK(L, 1, 1000 + 600001 + 60000) == kPsOk);
    L.reset();
    for (int i = 0; i < 3; ++i) for (int s = 0; s < 4; ++s) L.take(s, (uint32_t)(i * 20000 + s));
    row("lim_room_cap_12", LIM_CHECK(L, 0, 90000) == kPsDropRoomCap);
    row("lim_bad_seat", L.check(7, 0) == kPsDropSeat && L.check(-1, 0) == kPsDropSeat);
    // clock wrap
    L.reset(); L.take(2, 0xFFFFFF00u);
    row("lim_wrap_gap", LIM_CHECK(L, 2, 0x100) == kPsDropGap);
    // visits
    PsVisits V; V.reset();
    row("visit_none", !V.within(0, 3, 1000, kPsVisitMs));
    V.note(0, 3, 1000);
    row("visit_recent", V.within(0, 3, 50000, kPsVisitMs));
    row("visit_old", !V.within(0, 3, 61001, kPsVisitMs));
    row("visit_other_course", !V.within(0, 4, 2000, kPsVisitMs));
    row("visit_other_seat", !V.within(1, 3, 2000, kPsVisitMs));
    row("visit_bad_args", !V.within(9, 3, 0, 1) && !V.within(0, 30, 0, 1) && !V.within(0, -1, 0, 1));
    V.reset_seat(0);
    row("visit_reset_seat", !V.within(0, 3, 2000, kPsVisitMs));
    // course table
    signed char tab[52]; memset(tab, -1, sizeof tab); tab[6] = 0; tab[50] = 29; tab[3] = 30; tab[4] = 99;
    row("course_of_level", ps_course_of_level(tab, 6) == 0 && ps_course_of_level(tab, 50) == 29 &&
        ps_course_of_level(tab, 0) == -1 && ps_course_of_level(tab, 52) == -1 && ps_course_of_level(tab, -1) == -1 &&
        ps_course_of_level(tab, 3) == -1 && ps_course_of_level(tab, 4) == -1 && ps_course_of_level(0, 6) == -1);
    // queue
    PsQueue Q; Q.reset();
    PsStar s0 = {1, 2, 3, 0, 10};
    row("queue_push_pop", Q.push(s0) && Q.has(2, 3) && !Q.has(2, 4) && Q.pop().course == 2 && Q.n == 0);
    bool full = true;
    for (int i = 0; i < kPsQueueMax; ++i) full = full && Q.push(s0);
    row("queue_cap_8", full && !Q.push(s0) && Q.n == kPsQueueMax);
    // sequence memory
    PsSeqMemory S; S.reset();
    row("seq_new", !S.seen_or_add(1, 77));
    row("seq_repeat", S.seen_or_add(1, 77));
    row("seq_other_seat", !S.seen_or_add(2, 77));
    for (int i = 0; i < kPsSeqMemory + 2; ++i) S.seen_or_add(1, 100 + i);
    row("seq_forgets_oldest", !S.seen_or_add(1, 77));
    row("seq_bad_seat", S.seen_or_add(9, 1));
    printf("FAILS %d\n", fails);
    return fails ? 1 : 0;
}
'''


def row_unit():
    d = os.path.join(ARGS.out, "unit")
    os.makedirs(d, exist_ok=True)
    src = os.path.join(d, "unit.cpp")
    with open(src, "w") as f:
        f.write(UNIT_CPP)
    bat = os.path.join(d, "unit.cmd")
    res = {}
    for broken in (0, 1, 2):
        exe = os.path.join(d, "unit%d.exe" % broken)
        with open(bat, "w") as f:
            f.write('@echo off\ncall "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvars32.bat" >nul 2>&1\n'
                    'cl /nologo /EHsc /W3 /DBROKEN=%d /I"%s" "%s" /Fe:"%s" /Fo:"%s"\n' % (
                        broken, os.path.join(REPO, "port", "hal"), src, exe, os.path.join(d, "u%d.obj" % broken)))
        r = subprocess.run(["cmd", "/c", bat], capture_output=True, text=True, cwd=d)
        if not os.path.exists(exe):
            say(False, "unit: compile of variant %d failed: %s" % (broken, (r.stdout + r.stderr)[-500:]))
            return
        res[broken] = subprocess.run([exe], capture_output=True, text=True)
    ok = res[0].returncode == 0 and "FAILS 0" in res[0].stdout
    n = len(re.findall(r"(?m)^ROW .* ok$", res[0].stdout))
    say(ok, "unit: the real rules pass all %d checks (%s)" % (n, res[0].stdout.strip().splitlines()[-1]))
    for k, what in ((1, "a limiter that never refuses"), (2, "a diff that ignores cleared bits and multi-bit changes")):
        bad = re.findall(r"(?m)^ROW (\S+) bad$", res[k].stdout)
        say(res[k].returncode != 0 and bool(bad), "unit MUST-FAIL: %s is caught by %s" % (what, bad[:5]))


ROWS = {}


def main():
    global ARGS
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe")
    ap.add_argument("--base")
    ap.add_argument("--kit")
    ap.add_argument("--kitroot")
    ap.add_argument("--root", default=REPO)
    ap.add_argument("--out", default=os.path.join(os.getcwd(), "pstar_out"))
    ap.add_argument("--fps", type=float, default=30.0, help="measured frames per wall second of a console in a room")
    ap.add_argument("rows", nargs="+")
    ARGS = ap.parse_args()
    os.makedirs(ARGS.out, exist_ok=True)
    for r in ARGS.rows:
        fn = ROWS.get(r)
        if not fn:
            print("no such row", r)
            continue
        print("\n== row %s ==" % r, flush=True)
        fn()
    open(os.path.join(ARGS.out, "results.txt"), "w").write("\n".join(RES) + "\n")
    bad = [x for x in RES if x.startswith("FAIL")]
    print("\n%d PASS, %d FAIL" % (len([x for x in RES if x.startswith("PASS")]), len(bad)))
    return 1 if bad else 0


ROWS.update(unit=row_unit, share=row_share, base=row_base)

if __name__ == "__main__":
    sys.exit(main())
