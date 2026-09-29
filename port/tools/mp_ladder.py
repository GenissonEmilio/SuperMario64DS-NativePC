#!/usr/bin/env python3
"""THE MULTIPLAYER 1.0 GATE LADDER, one command (run mp1, lane MPPROOF1).

One reproducible ladder for VS lockstep, adventure ghosts, chat and voice,
built from the scout drivers of run mp1 (MPSCOUT mptrial.py + sweep.py,
GHOSTSCOUT ghost_trial.py, CHILDBOOT cbmp.py). Every rung launches N headless
walk_window consoles in one session, reads their logs and prints ONE verdict
line: PASS, FAIL, SKIP (with the reason) or TIMEOUT.

    python port/tools/mp_ladder.py --lock <ladder lockfile> [--exe EXE]
                                   [--tier dev|gate|full] [--only REGEX]
                                   [--skip REGEX] [--assume LANE,...]
                                   [--kit KITDIR --rom ROM.nds] [--out DIR]
    python port/tools/mp_ladder.py --list [--tier full]     # rungs + gating
    python port/tools/mp_ladder.py --probe                  # lane markers only

Exit 0 when every rung that ran is PASS (SKIPs allowed), 1 on any FAIL or a
TIMEOUT that failed its re-run, 2 when the run stopped early (busy flag, lock
timeout, memory).

TIERS. dev = the 2- and 4-console rungs (what a lane runs while developing);
gate = dev + the 8/16-console rungs and the kit rung (the fold's gate run);
full = gate + the whole VS grid (2/4/8/16 x loopback/relay v1 120 pps/relay v2
240 pps x added RTT 0/80/160 x paced/unpaced).

WHAT A VS RUNG CHECKS, per console: rc 0, no FAULT line, the session came up,
the live mask covers every slot, lockstep (a rung that did not ask for rollback
must not run it), every slot's body took at least --minpos distinct positions
in THIS console's world (a motionless body cannot pass), every slot's pad word
was nonzero at least once. Per rung: the dhdiff sweep over EVERY pairing
(parent first, then child pairs, trimmed to the shortest log minus a tail
margin), one input delay on every console, the local relay's drop counters 0.
Match rungs add: exactly one MATCH OVER line per console, identical on every
console, and the "[vs] closing" exit. The chosen input delay and the starved /
subst / late / catchup counters are printed on every VS row.

THE ENVIRONMENT RULES THIS HARNESS KEEPS (the run's standing orders):
  - every console hidden and muted: mp2_proof.spawn (CREATE_NO_WINDOW +
    SW_SHOWMINNOACTIVE) and mp2_proof.env_base (SM64DS_NO_FOCUS,
    SM64DS_MINIMIZED, SM64DS_VOLUME=0); voice rungs add SM64DS_NO_AUDIO=1;
  - one multi-console run at a time machine-wide: every rung holds --lock (a
    build_lock.py lockfile; the ladder refuses to launch without one) for its
    own launch only, so other lanes' rungs interleave between ours;
  - a busy flag: when the last line of --busy-log (default
    ~/.claude/scripts/league_chill.log) contains "chilling", the ladder does
    not start a rung, and a rung already running is killed and recorded as
    ABORTED; --busy-wait S waits up to S seconds for the flag to clear;
  - ports derived from this process's PID, never the shipped default 51765;
  - a TIMEOUT row (a console killed at the deadline) is re-run once, alone,
    after the other rungs; the re-run's verdict is the one that counts and
    both are recorded.

LANE GATING. Rungs whose product code has not landed yet SKIP with the lane
named. Presence is read from the exe under test (a string it must contain),
its .map (a symbol or object file), or a live probe of the relay source.
LANE_MARKERS below is the table; --assume LANE forces a lane's rungs to run.
The later lanes' test knobs and log lines this ladder reads are collected in
LANE CONTRACTS below: they are the ladder's proposal where design.txt names
none, and the owning lane (or the fold) aligns them in one place.

OUTPUT: <out>/<rung>/p<k>/{run.log,env.txt} per console (+ relay.log),
<out>/summary.tsv and <out>/summary.txt. Default <out> is
build/mp_ladder/<UTC stamp> under the repo root.
"""

import argparse
import collections
import datetime
import hashlib
import os
import re
import shutil
import socket
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import mp2_proof as M          # noqa: E402  (quiet spawn + env_base)
import build_lock              # noqa: E402  (the ladder lock)

try:
    import psutil              # kill / suspend rungs and the memory guard
except ImportError:            # pragma: no cover
    psutil = None

DEFAULT_EXE = os.path.join(ROOT, "build", "port", "walk_window.exe")
RELAY_PY = os.path.join(ROOT, "port", "tools", "relay", "relay.py")
DHDIFF = os.path.join(HERE, "dhdiff.py")
SHIPPED_PORT = 51765
KEYS = [0x0040, 0x0080, 0x0020, 0x0010]     # up, down, right, left: pads that move

# ---------------------------------------------------------------------------
# LANE MARKERS: how the ladder knows a lane's code is in the exe under test.
# Each lane is present when ANY of its markers is found.
#   ("str", b"...")  bytes in the exe (wire tags and format strings are there)
#   ("env", "NAME")  an environment knob name the exe reads
#   ("map", "text")  a symbol or object name in the .map beside the exe
#   ("relay", 2)     the relay source answers a version-2 HELLO (live probe)
# ---------------------------------------------------------------------------
LANE_MARKERS = {
    "N1": [("str", b"SD10"), ("env", "SM64DS_SESSION_DIGEST")],   # session check in the ACCEPT
    "N2": [("str", b"SYNK"), ("map", "comms_recv_kind")],         # relay v2 carrier + aux kinds
    "N3": [("map", "port_comms_rounds_ahead"), ("str", b"catchup=")],  # laggard rule
    "R1": [("relay", 2)],                                         # relay v2
    "H1": [("map", "comms_hub.cpp")],                             # hub member transport
    "W2": [("str", b"SYNG")],                                     # ghost frames
    "C1": [("map", "adv_chat.cpp"), ("str", b"SYNT")],            # text chat
    "V1": [("env", "SM64DS_VOICE_ALLOWED")],                      # mic gate + room setting
}
LANE_NAMES = {"N1": "NETCORE1", "N2": "VSAUX1", "N3": "LAGGARD1", "R1": "RELAY2",
              "H1": "HUB1", "W2": "GHOSTWIRE1", "C1": "CHATUI1", "V1": "VOICE1"}

# ---------------------------------------------------------------------------
# LANE CONTRACTS: knobs and log lines of lanes that had not landed when this
# file was written. design.txt fixes the wire; these are test-side details it
# does not name. Change them HERE when the owning lane lands.
# ---------------------------------------------------------------------------
# N1: the 30 Hz round period shows in the adaptive-delay line ("/ 33.33 ms").
N1_PERIOD = "33.33"
# N1: a child that refuses the session exits with this code (design 2.3).
N1_MISMATCH_RC = 3
# N3: the hitch knob on one child: the pump stopped 400 ms every 10 s.
N3_HITCH_ENV = {"SM64DS_COMMS_HITCH": "10000:400"}
# N3: close-line counters (design 2.2 / 4.2).
N3_COUNTERS = ("subst", "late", "catchup")
# H1: a hub console's environment beyond SM64DS_ADVENTURE + relay + code.
HUB_ENV = {"SM64DS_ADVENTURE": "1", "SM64DS_LEVEL": "1", "SM64DS_SYNC_REPORT": "1",
           "SM64DS_ADVENTURE_DIAG": "60", "SM64DS_COMMS_REPORT": "1"}
# C1: the sender's scripted message and the receiver's log line.
CHAT_SEND_ENV = "SM64DS_CHAT_TEST"            # "<frame>:<text>"
CHAT_RX = re.compile(r"^\[chat\] rx seat=(\d+) id=(\d+)", re.M)
# V1 + N2: voice tone rungs read the existing [voice] report (per-seat entries).

# ---------------------------------------------------------------------------
# LOG PARSING
# ---------------------------------------------------------------------------
POS = re.compile(r"^\[vs\] f(\d+) slot(\d+) .*?pos=\(([-\d.]+), ?([-\d.]+), ?([-\d.]+)\)"
                 r"(?:.*?pad=([0-9A-Fa-f]+))?", re.M)
DH = re.compile(r"^\[dh\] f(\d+) .*rounds=(\d+)", re.M)
CLOSE = re.compile(r"\[comms:loopback\] closed after (\d+) rounds; indelay=(-?\d+) "
                   r"starved=(\d+)([^\n]*)")
ADAPT = re.compile(r"adaptive input delay (\d+) frame\(s\) \(was -?\d+\): ceil\((\d+) ms worst "
                   r"\* (\d+)% / (\d+\.\d+) ms\) \+ (-?\d+), floor (-?\d+) cap (\d+)")
LIVE = re.compile(r"\b(?:live|peak)=0x([0-9a-f]+)|live mask 0x([0-9a-f]+)")
SESSION = re.compile(r"session up after (\d+) turns")
SELFTEST = re.compile(r"^selftest: (\d+) frames", re.M)
MATCH = re.compile(r"^\[vs\] MATCH OVER[^\n]*", re.M)
RBLINE = re.compile(r"rollback: live=0x[0-9a-f]+ peak=0x[0-9a-f]+[^\n]*?rewinds=(\d+)[^\n]*?"
                    r"stalled=(\d+)")
VIS = re.compile(r"\[advdiag\][^\n]*slot(\d+)\(ghost\)[^\n]*vis=(-?\d+)")
APPLIED = re.compile(r"\[sync:[^\]]*\][^\n]*\bapplied=(-?\d+)")
LOCALW = re.compile(r"\[sync:[^\]]*\][^\n]*\blocal_writes=(-?\d+)")
VOICE = re.compile(r"^\[voice\] [^:\n]*: on=(\d+) tone=(\d+) dev=(-?\d+) cap=(\d+) tx=(\d+) "
                   r"rx=(\d+) bad=(\d+) dup=(\d+)([^\n]*)$", re.M)
VOICE_SEAT = re.compile(r"\| s(\d+) d=(-?\d+) g=([\d.]+) buf=(\d+)")
LUIGI_SEED = re.compile(r"\[luigi\] SEED f(\d+): slot (\d+) starts as Luigi")
LUIGI_TAG = re.compile(r"\[luigi\] TAG: slot (\d+) converted[^\n]*")
STARVED_EV = re.compile(r"starved at round")


def faults(t):
    return [ln for ln in t.splitlines()
            if ln.startswith("FAULT") or "quarantine c" in ln.lower()
            or "unhandled exception" in ln.lower()]


def parse(t):
    """Everything a verdict reads from one console's log, in one pass."""
    c = {"lines": t.count("\n"), "faults": faults(t)}
    c["session"] = bool(SESSION.search(t))
    st = SELFTEST.findall(t)
    c["selftest"] = int(st[-1]) if st else None
    c["rollback"] = "NetMode ROLLBACK" in t
    mask = 0
    for m in LIVE.finditer(t):
        mask |= int(m.group(1) or m.group(2), 16)
    c["mask"] = mask
    pos = collections.defaultdict(set)
    pad = collections.defaultdict(int)
    for m in POS.finditer(t):
        s = int(m.group(2))
        pos[s].add(m.group(3, 4, 5))
        if m.group(6) and int(m.group(6), 16):
            pad[s] += 1
    c["pos"] = {s: len(v) for s, v in pos.items()}
    c["pad"] = dict(pad)
    cl = CLOSE.findall(t)
    c["close"] = None
    if cl:
        rounds, indelay, starved, tail = cl[-1]
        extra = dict(re.findall(r"\b(\w+)=([\w:,]+)", tail))
        c["close"] = {"rounds": int(rounds), "indelay": int(indelay), "starved": int(starved),
                      "extra": extra}
    ad = ADAPT.findall(t)
    c["adapt"] = ad[-1] if ad else None
    c["match"] = MATCH.findall(t)
    c["closing"] = "[vs] closing" in t
    rb = RBLINE.findall(t)
    c["rb"] = (int(rb[-1][0]), int(rb[-1][1])) if rb else None
    vis = {}
    for m in VIS.finditer(t):
        vis[int(m.group(1))] = int(m.group(2))
    c["vis"] = vis
    ap = APPLIED.findall(t)
    c["applied"] = int(ap[-1]) if ap else None
    lw = LOCALW.findall(t)
    c["local_writes"] = int(lw[-1]) if lw else None
    c["starve_events"] = len(STARVED_EV.findall(t))
    return c


# ---------------------------------------------------------------------------
# ENVIRONMENT GUARDS: the busy flag, the lock, memory, ports
# ---------------------------------------------------------------------------
class Stop(Exception):
    """The ladder cannot go on (busy flag, lock timeout)."""


class Skip(Exception):
    """A rung found at run time that the exe does not carry its lane yet."""


def busy(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            lines = [ln for ln in f.read().splitlines() if ln.strip()]
    except OSError:
        return ""
    last = lines[-1] if lines else ""
    return last if "chilling" in last else ""


def wait_not_busy(path, wait_s):
    end = time.time() + wait_s
    while True:
        b = busy(path)
        if not b:
            return
        if time.time() >= end:
            raise Stop("busy flag says: %s" % b.strip())
        time.sleep(15)


def mem_ok(n):
    if psutil is None:
        return True, "psutil missing, memory not checked"
    avail = psutil.virtual_memory().available
    need = n * 300 * 2**20 + 1024 * 2**20
    return avail >= need, "%d MB free, %d MB wanted for %d consoles" % (
        avail >> 20, need >> 20, n)


PID = os.getpid()


def game_port(i):
    p = 42000 + ((PID + 37 * i) % 700) * 20
    if p <= SHIPPED_PORT < p + 20:
        p += 40
    return p


def relay_port(i):
    return 40000 + ((PID + 11 * i) % 900) * 2


# ---------------------------------------------------------------------------
# LANE PRESENCE
# ---------------------------------------------------------------------------
class Features:
    def __init__(self, exe, relay_py, assume):
        self.exe = exe
        self.relay_py = relay_py
        self.assume = set(assume)
        with open(exe, "rb") as f:
            self.blob = f.read()
        self.md5 = hashlib.md5(self.blob).hexdigest()
        mp = os.path.splitext(exe)[0] + ".map"
        self.map = None
        if os.path.exists(mp):
            with open(mp, "r", encoding="utf-8", errors="replace") as f:
                self.map = f.read()
        self.relay_v = relay_version(relay_py)
        self.have = {}
        self.why = {}
        for lane, marks in LANE_MARKERS.items():
            hit, seen = False, []
            for kind, val in marks:
                if kind == "str":
                    ok = val in self.blob
                elif kind == "env":
                    ok = val.encode() in self.blob
                elif kind == "map":
                    ok = self.map is not None and val in self.map
                else:
                    ok = self.relay_v >= val
                seen.append("%s:%s=%s" % (kind, val.decode() if isinstance(val, bytes) else val,
                                          "yes" if ok else "no"))
                hit = hit or ok
            if lane in self.assume:
                hit = True
                seen.append("assumed")
            self.have[lane] = hit
            self.why[lane] = " ".join(seen)

    def missing(self, needs):
        return [n for n in needs if not self.have.get(n, False)]


def relay_version(relay_py):
    """Start the relay source on 127.0.0.1 and ask it for a version-2 hub seat.
    2 when it answers ACK v2 ok, 1 when it answers anything else, 0 when it
    does not start or answer."""
    if not os.path.exists(relay_py):
        return 0
    port = relay_port(97)
    env = dict(os.environ, SM64DS_RELAY_PORT=str(port), SM64DS_RELAY_BIND="127.0.0.1",
               SM64DS_RELAY_STATS_S="60")
    p = subprocess.Popen([sys.executable, "-u", relay_py], env=env, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, creationflags=M.NO_CONSOLE)
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(0.5)
    ver = 0
    try:
        code = ("P%07d" % (PID % 10**7)).encode()
        hello = b"SMRC" + bytes([2, 2, 0xFF, 0]) + code
        time.sleep(0.8)
        for _ in range(10):
            if p.poll() is not None:
                break
            try:
                s.sendto(hello, ("127.0.0.1", port))
                d, _ = s.recvfrom(64)
            except socket.timeout:
                continue
            except OSError:          # port unreachable while the relay starts
                time.sleep(0.3)
                continue
            if d[:4] == b"SMRA" and len(d) >= 6:
                ver = 2 if (d[4] == 2 and d[5] == 0) else 1
                break
    finally:
        s.close()
        p.kill()
        p.wait()
    return ver


# ---------------------------------------------------------------------------
# RUNGS
# ---------------------------------------------------------------------------
class Rung:
    def __init__(self, name, kind, n, tiers, transport="loop", rtt=0, paced=False,
                 frames=None, needs=(), **opts):
        self.name, self.kind, self.n, self.tiers = name, kind, n, set(tiers)
        self.transport, self.rtt, self.paced = transport, rtt, paced
        self.frames = frames
        self.needs = list(needs) + (["R1"] if transport == "relay2" else [])
        self.opts = opts

    def desc(self):
        return "%s n=%d %s rtt=%d %s" % (self.kind, self.n, self.transport, self.rtt,
                                         "paced" if self.paced else "unpaced")


def build_rungs():
    R = []
    DEV, GATE, FULL = ("dev", "gate", "full"), ("gate", "full"), ("full",)
    gate_grid = {
        (2, "loop", 0, False), (4, "loop", 0, False), (8, "loop", 0, False), (16, "loop", 0, False),
        (2, "relay1", 0, True), (2, "relay1", 80, True), (2, "relay1", 160, True),
        (4, "relay1", 0, True), (4, "relay1", 80, True), (4, "relay1", 160, True),
        (16, "relay1", 80, True),
        (2, "relay2", 80, True), (4, "relay2", 80, True), (4, "relay2", 160, True),
        (16, "relay2", 80, True),
    }
    for n in (2, 4, 8, 16):
        for tr in ("loop", "relay1", "relay2"):
            for rtt in (0, 80, 160):
                for paced in (False, True):
                    key = (n, tr, rtt, paced)
                    tiers = (DEV if n <= 4 else GATE) if key in gate_grid else FULL
                    R.append(Rung("vs%d-%s-rtt%d-%s" % (n, tr, rtt, "p" if paced else "u"),
                                  "vs", n, tiers, tr, rtt, paced))
    # The launcher's shape: no SM64DS_NETMODE. 1.0 runs lockstep online (N1).
    R.append(Rung("vs4-default-mode", "vs", 4, DEV, default_mode=True))
    # Matches to MATCH OVER (star target 1, the vs4_proof rung-7 recipe).
    R.append(Rung("match2-loop", "match", 2, DEV))
    R.append(Rung("match4-loop", "match", 4, DEV))
    R.append(Rung("match4-relay1-rtt80", "match", 4, DEV, "relay1", 80))
    R.append(Rung("match16-loop", "match", 16, GATE))
    R.append(Rung("match16-relay1-rtt80", "match", 16, GATE, "relay1", 80))
    # Leaving, joining late, dying, lagging.
    R.append(Rung("leave4-loop", "leave", 4, DEV))
    R.append(Rung("latejoin4-loop", "late", 4, DEV))
    R.append(Rung("kill4-relay1-rtt80", "kill", 4, DEV, "relay1", 80, True, needs=["N3"]))
    R.append(Rung("laggard4-hitch-relay1-rtt80", "laggard", 4, DEV, "relay1", 80, True,
                  needs=["N3"], hitch="pause"))
    R.append(Rung("laggard4-slowlink-relay1-rtt80", "laggard", 4, DEV, "relay1", 80, True,
                  needs=["N3"], hitch="delay"))
    R.append(Rung("laggard16-hitch-relay1-rtt80", "laggard", 16, GATE, "relay1", 80, True,
                  needs=["N3"], hitch="pause"))
    # Session check and Luigi Infection.
    R.append(Rung("digest2-mismatch", "digest", 2, DEV, needs=["N1"]))
    R.append(Rung("luigi4-loop", "luigi", 4, DEV, seed=7))
    R.append(Rung("luigi2-seeds", "luigiseeds", 2, DEV))
    R.append(Rung("luigi16-relay1-rtt80", "luigi", 16, GATE, "relay1", 80, seed=7))
    # Adventure. adv*: today's transport (ghosts ride the VS carrier).
    # hub*: the 1.0 hub member through relay v2 (H1), ghost frames (W2).
    R.append(Rung("adv2-loop", "adv", 2, DEV))
    R.append(Rung("adv4-loop", "adv", 4, DEV))
    R.append(Rung("adv16-loop", "adv", 16, GATE))
    R.append(Rung("hub2-relay2", "hub", 2, DEV, "relay2", needs=["H1", "W2"]))
    R.append(Rung("hub4-relay2", "hub", 4, DEV, "relay2", needs=["H1", "W2"]))
    R.append(Rung("hub16-relay2", "hub", 16, GATE, "relay2", needs=["H1", "W2"]))
    R.append(Rung("hub4-joinleave-relay2", "hubjoin", 4, DEV, "relay2", needs=["H1", "W2"]))
    R.append(Rung("hub4-pause-relay2", "hubpause", 4, DEV, "relay2", needs=["H1"]))
    # Chat (C1) in VS and in adventure, with 20 % loss.
    R.append(Rung("chat3-vs-relay2", "chat", 3, DEV, "relay2", needs=["C1", "N2"]))
    R.append(Rung("chat3-vs-relay2-loss20", "chat", 3, DEV, "relay2", needs=["C1", "N2"],
                  loss=20))
    R.append(Rung("chat3-hub-relay2", "chathub", 3, DEV, "relay2", needs=["C1", "H1"]))
    # Voice tone: every console hears every other console, attributed by seat.
    R.append(Rung("voice4-loop", "voice", 4, DEV))
    R.append(Rung("voice4-relay2", "voice", 4, DEV, "relay2", needs=["N2", "V1"]))
    R.append(Rung("voice16-relay2", "voice", 16, GATE, "relay2", needs=["N2", "V1"]))
    # The 09-26 host fault re-check on a player-shaped install.
    R.append(Rung("kit2-vs-loop", "kit", 2, GATE, needs=["kit"]))
    return R


# ---------------------------------------------------------------------------
# LAUNCHING
# ---------------------------------------------------------------------------
class LocalRelay:
    def __init__(self, src, port, pps, logpath):
        env = dict(os.environ)
        for k in list(env):
            if k.startswith("SM64DS_RELAY"):
                env.pop(k)
        env.update(SM64DS_RELAY_PORT=str(port), SM64DS_RELAY_BIND="127.0.0.1",
                   SM64DS_RELAY_MAX_CHILDREN="15", SM64DS_RELAY_RATE_PPS=str(pps),
                   SM64DS_RELAY_STATS_S="5")
        self.logpath = logpath
        self.log = open(logpath, "wb")
        self.p = subprocess.Popen([sys.executable, "-u", src], env=env, stdout=self.log,
                                  stderr=subprocess.STDOUT, creationflags=M.NO_CONSOLE)
        time.sleep(1.0)
        if self.p.poll() is not None:
            raise RuntimeError("local relay exited at start (see %s)" % logpath)

    def stop(self):
        self.p.terminate()
        try:
            self.p.wait(5)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait()
        self.log.close()
        with open(self.logpath, "r", encoding="utf-8", errors="replace") as f:
            t = f.read()
        stats = [ln for ln in t.splitlines() if " stats " in ln]
        last = stats[-1] if stats else ""
        return {k: int(v) for k, v in re.findall(r"\b(drop\w*|rx|fwd|full|bad)=(\d+)", last)}


class Run:
    """One rung's launch: consoles, optional relay, timed actions, a deadline."""

    def __init__(self, ladder, rung, idx, subdir=None):
        self.L, self.rung, self.idx = ladder, rung, idx
        self.dir = subdir or os.path.join(ladder.out, rung.name)
        if os.path.isdir(self.dir):
            shutil.rmtree(self.dir, ignore_errors=True)
        os.makedirs(self.dir, exist_ok=True)
        self.port = game_port(idx)
        self.code = "L%d%06d" % (rung.n % 10, (PID * 131 + idx) % 10**6)
        self.relay = None
        self.relay_stats = None
        self.aborted = ""

    def start_relay(self):
        tr = self.rung.transport
        if tr == "loop":
            return None
        src = self.L.relay1_py if tr == "relay1" else self.L.relay_py
        pps = 120 if tr == "relay1" else 240
        self.relay = LocalRelay(src, relay_port(self.idx), pps, os.path.join(self.dir, "relay.log"))
        return "127.0.0.1:%d" % relay_port(self.idx)

    def base_env(self, k, asset_root=None):
        d = os.path.join(self.dir, "p%d" % k)
        os.makedirs(os.path.join(d, "tmp"), exist_ok=True)
        tag = "m%s%d" % (hashlib.md5(self.rung.name.encode()).hexdigest()[:4], k)
        e = M.env_base(asset_root or self.L.asset_root, d, tag)
        return d, e

    def launch(self, specs, timeout, actions=()):
        """specs: [(k, env, cwd, delay_s)]. actions: [(k, after_s, what, arg)] where
        after_s counts from console k's spawn; what in kill | suspend."""
        procs = {}
        spawned = {}
        t0 = time.time()
        stop_evt = threading.Event()

        def guard():
            while not stop_evt.wait(2.0):
                b = busy(self.L.busy_log)
                if b:
                    self.aborted = "busy flag: " + b.strip()
                    for p in procs.values():
                        try:
                            p.kill()
                        except OSError:
                            pass
                    return
        g = threading.Thread(target=guard, daemon=True)
        g.start()

        def act(k, after, what, arg):
            while k not in spawned and not stop_evt.is_set():
                time.sleep(0.2)
            if stop_evt.wait(max(0.0, spawned.get(k, time.time()) + after - time.time())):
                return
            p = procs.get(k)
            if p is None or p.poll() is not None or psutil is None:
                return
            try:
                if what == "kill":
                    p.kill()
                elif what == "suspend":
                    pp = psutil.Process(p.pid)
                    pp.suspend()
                    time.sleep(arg)
                    pp.resume()
            except (OSError, psutil.Error):
                pass
        threads = [threading.Thread(target=act, args=a, daemon=True) for a in actions]
        for th in threads:
            th.start()
        rcs = {}
        try:
            for k, env, cwd, delay in sorted(specs, key=lambda s: s[3]):
                wait = t0 + delay - time.time()
                if wait > 0:
                    time.sleep(wait)
                if self.aborted:
                    break
                with open(os.path.join(cwd, "env.txt"), "w") as f:
                    for key in sorted(env):
                        if key.startswith("SM64DS_"):
                            f.write("%s=%s\n" % (key, env[key]))
                procs[k] = M.spawn(env["_EXE"], cwd, {x: v for x, v in env.items() if x != "_EXE"},
                                   os.path.join(cwd, "run.log"))
                spawned[k] = time.time()
            deadline = t0 + timeout
            for k in sorted(procs):
                rcs[k] = M.finish(procs[k], max(1, deadline - time.time()))
        except BaseException:
            for p in procs.values():
                try:
                    p.kill()
                except OSError:
                    pass
            if self.relay:
                self.relay_stats = self.relay.stop()
            raise
        finally:
            stop_evt.set()
        wall = time.time() - t0
        if self.relay:
            self.relay_stats = self.relay.stop()
        texts = {k: M.text(os.path.join(self.dir, "p%d" % k, "run.log")) for k in procs}
        return rcs, texts, wall


def vs_env(run, k, n, frames, relay, paced, rtt, netmode="lockstep", match=False,
           inject=True, exe=None, asset_root=None, cwd_env=None):
    d, e = run.base_env(k, asset_root)
    e["_EXE"] = exe or run.L.exe
    e.pop("SM64DS_LEVEL", None)
    e["SM64DS_COMMS_ROLE"] = "parent" if k == 0 else "child"
    e["SM64DS_COMMS_PORT"] = str(run.port)
    e["SM64DS_COMMS_REPORT"] = "1"
    if k:
        e["SM64DS_COMMS_SLOT"] = str(k)
    e["SM64DS_VS_MAP"] = "0"
    e["SM64DS_VS_PLAYERS"] = str(n)
    e["SM64DS_VS_PROBE"] = "1"
    e["SM64DS_VS_STATE_HASH"] = "1"
    e["SM64DS_COMMS_FANOUT"] = "1"
    if netmode:
        e["SM64DS_NETMODE"] = netmode
    if match:
        e.update({"SM64DS_VS_BREAKALL": "120", "SM64DS_STAR_TRIGGER": "200,280",
                  "SM64DS_VS_EXIT_ON_END": "1", "SM64DS_VS_END_GRACE": "120",
                  "SM64DS_VS_STAR_TARGET": "1"})
    elif inject:
        e["SM64DS_COMMS_INJECT"] = "key=0x%04x,toggle=45,key2=0x%04x" % (KEYS[k % 4],
                                                                        KEYS[(k + 1) % 4])
    e["SM64DS_WINDOW_SELFTEST"] = str(frames + (parent_surplus(n) if k == 0 and not match else 0))
    if paced:
        e["SM64DS_PACE_SELFTEST"] = "1"
    if rtt:
        e["SM64DS_COMMS_DELAY_MS"] = str(rtt // 2)
    if relay:
        e["SM64DS_COMMS_RELAY"] = relay
        e["SM64DS_COMMS_CODE"] = run.code
    if cwd_env:
        e.update(cwd_env)
    return d, e


def parent_surplus(n):
    """The parent outlives its children by a few seconds of frames, so every
    child's last rounds are served and each child leaves with a BYE."""
    return 45 + 15 * n


def timeout_for(n, frames, paced, extra=0):
    if paced:
        return int((frames + parent_surplus(n)) / 30.0 * 2.5 + 120 + 12 * n + extra)
    return int(120 + 25 * n + frames / 900.0 * 60 + extra)


# ---------------------------------------------------------------------------
# VERDICT HELPERS
# ---------------------------------------------------------------------------
class Checks:
    def __init__(self):
        self.items = []
        self.info = []

    def ok(self, cond, msg):
        self.items.append((bool(cond), msg))
        return cond

    def note(self, msg):
        self.info.append(msg)

    def failed(self):
        return [m for ok, m in self.items if not ok]


def sweep(texts, keys, margin, workdir):
    """dhdiff every pairing among keys (first key against the rest, then pairs),
    each log trimmed to the shortest log's hashed rounds minus margin."""
    caps = {k: max([int(m.group(2)) for m in DH.finditer(texts[k])] or [0]) for k in keys}
    cap = min(caps.values()) - margin
    if cap <= 30:
        return False, "too few hashed rounds (%s)" % ",".join("p%d:%d" % kv for kv in caps.items())
    trimmed = {}
    for k in keys:
        out = os.path.join(workdir, "p%d" % k, "run.log.trim")
        with open(out, "w", encoding="utf-8") as g:
            for line in texts[k].splitlines(True):
                m = DH.match(line)
                if m and int(m.group(2)) > cap:
                    continue
                g.write(line)
        trimmed[k] = out
    first, rest = keys[0], keys[1:]
    pairs = [(first, b) for b in rest] + [(a, b) for i, a in enumerate(rest) for b in rest[i + 1:]]
    bad = []
    for a, b in pairs:
        p = subprocess.run([sys.executable, DHDIFF, trimmed[a], trimmed[b]], capture_output=True,
                           text=True, errors="replace")
        if p.returncode != 0:
            first = [ln.strip() for ln in p.stdout.splitlines() if "DIVERGENCE" in ln]
            last = (first or p.stdout.strip().splitlines() or ["rc %d" % p.returncode])[0][:100]
            bad.append("p%d/p%d: %s" % (a, b, last))
    msg = "SWEEP %d/%d pairings NO DIVERGENCE to round %d" % (len(pairs) - len(bad), len(pairs), cap)
    if bad:
        msg += "; " + " | ".join(bad[:3])
    return not bad, msg


def console_vs(ck, k, c, rc, n, minpos, want_mode="lockstep", slots=None):
    slots = list(range(n)) if slots is None else slots
    ck.ok(rc == 0, "p%d rc=%s" % (k, rc))
    ck.ok(not c["faults"], "p%d faults=%d %s" % (k, len(c["faults"]), (c["faults"][:1] or [""])[0][:120]))
    ck.ok(c["session"], "p%d session up" % k)
    if want_mode == "lockstep":
        ck.ok(not c["rollback"], "p%d runs lockstep" % k)
    full = sum(1 << s for s in slots)
    ck.ok(c["mask"] & full == full, "p%d live mask 0x%x covers 0x%x" % (k, c["mask"], full))
    low = [s for s in slots if c["pos"].get(s, 0) < minpos]
    ck.ok(not low, "p%d every slot moved >= %d distinct positions (low: %s)" % (
        k, minpos, ",".join("s%d:%d" % (s, c["pos"].get(s, 0)) for s in low) or "-"))
    nopad = [s for s in slots if not c["pad"].get(s)]
    ck.ok(not nopad, "p%d every slot's pad word nonzero (zero: %s)" % (
        k, ",".join("s%d" % s for s in nopad) or "-"))


def close_summary(cs):
    parts = []
    for k in sorted(cs):
        cl = cs[k]["close"]
        if not cl:
            parts.append("p%d:-" % k)
            continue
        ex = "".join(" %s=%s" % (x, cl["extra"][x]) for x in N3_COUNTERS if x in cl["extra"])
        parts.append("p%d:d%d/st%d%s" % (k, cl["indelay"], cl["starved"], ex))
    return " ".join(parts)


def delay_row(c, rtt):
    """The adaptive input delay the parent chose, with the formula inputs."""
    a = c["adapt"]
    if not a:
        return None, "no adaptive-delay line"
    frames, worst, safety, period, margin, floor, cap = a
    return a, "delay %s at rtt %d (worst %s ms, period %s ms, floor %s, cap %s)%s" % (
        frames, rtt, worst, period, floor, cap, "" if period == N1_PERIOD else " [pre-N1 60 Hz period]")


def write_settings(d, voice=False):
    import json
    cfg = {}
    if voice:
        cfg = {"VoiceEnabled": True, "VoiceMicDevice": "", "VoiceVolume": 100,
               "VoiceNearRadius": 60000, "VoiceFarRadius": 120000}
    with open(os.path.join(d, "settings.json"), "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2)


# ---------------------------------------------------------------------------
# RUNG BODIES. Each returns (Checks, wall_s, rcs).
# ---------------------------------------------------------------------------
def r_vs(L, run, r):
    n = r.n
    frames = r.frames or 900
    relay = run.start_relay()
    netmode = None if r.opts.get("default_mode") else "lockstep"
    specs = []
    for k in range(n):
        d, e = vs_env(run, k, n, frames, relay, r.paced, r.rtt, netmode=netmode)
        specs.append((k, e, d, 0.0 if k == 0 else 1.0 + 0.3 * k))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, r.paced))
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    if r.opts.get("default_mode") and any(c["rollback"] for c in cs.values()):
        raise Skip("needs N1 (%s): the default online mode is still ROLLBACK on this exe"
                   % LANE_NAMES["N1"])
    for k in range(n):
        console_vs(ck, k, cs[k], rcs.get(k), n, L.minpos)
    delays = sorted({c["close"]["indelay"] for c in cs.values() if c["close"]})
    ck.ok(len(delays) == 1, "one input delay on every console (%s)" % delays)
    margin = max(24, 2 * (delays[-1] if delays else 0) + 8)
    ok, msg = sweep(texts, list(range(n)), margin, run.dir)
    ck.ok(ok, msg)
    relay_checks(ck, run)
    a, row = delay_row(cs[0], r.rtt)
    if relay:
        ck.note(row)
        if a and a[3] == N1_PERIOD and r.rtt in (80, 160) and n <= 4:
            want = {80: 5, 160: 8}[r.rtt]
            ck.ok(int(a[0]) <= want, "1.0 delay at rtt %d: %s <= %d" % (r.rtt, a[0], want))
    ck.note("close " + close_summary(cs))
    ck.note("starve events per console " + ",".join(str(cs[k]["starve_events"]) for k in range(n)))
    return ck, wall, rcs


def relay_checks(ck, run):
    if run.relay_stats is None:
        return
    st = run.relay_stats
    drops = {k: v for k, v in st.items() if k.startswith("drop") and v}
    ck.ok(st.get("drop_rate", 0) == 0, "relay drop_rate=%s" % st.get("drop_rate", "?"))
    ck.note("relay rx=%s fwd=%s drops=%s" % (st.get("rx", "?"), st.get("fwd", "?"), drops or 0))


def r_match(L, run, r):
    n = r.n
    frames = r.frames or 600
    relay = run.start_relay()
    specs = []
    for k in range(n):
        d, e = vs_env(run, k, n, frames, relay, r.paced, r.rtt, match=True)
        specs.append((k, e, d, 0.0 if k == 0 else 1.0 + 0.3 * k))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, r.paced, 60))
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    for k in range(n):
        c = cs[k]
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        ck.ok(not c["faults"], "p%d faults=%d" % (k, len(c["faults"])))
        ck.ok(not c["rollback"], "p%d runs lockstep" % k)
        ck.ok(len(c["match"]) == 1, "p%d one MATCH OVER line (%d)" % (k, len(c["match"])))
        ck.ok(c["closing"], "p%d [vs] closing" % k)
    lines = {c["match"][0] for c in cs.values() if c["match"]}
    ck.ok(len(lines) == 1, "MATCH OVER identical on every console (%d distinct)" % len(lines))
    if lines:
        ck.note(next(iter(lines))[:140])
    ok, msg = sweep(texts, list(range(n)), 24, run.dir)
    ck.ok(ok, msg)
    relay_checks(ck, run)
    return ck, wall, rcs


def r_leave(L, run, r):
    """Child n-1 quits (its selftest ends: a clean BYE) a third of the way in;
    the parent and the others play on to their own end."""
    n, frames = r.n, r.frames or 900
    relay = run.start_relay()
    specs = []
    for k in range(n):
        d, e = vs_env(run, k, n, frames if k < n - 1 else frames // 3, relay, r.paced, r.rtt)
        specs.append((k, e, d, 0.0 if k == 0 else 1.0 + 0.3 * k))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, r.paced))
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    stay = list(range(n - 1))
    for k in range(n):
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        ck.ok(not cs[k]["faults"], "p%d faults=%d" % (k, len(cs[k]["faults"])))
    for k in stay:
        cl = cs[k]["close"]
        ck.ok(cl and cl["rounds"] >= frames - 60, "p%d played on after the leave (%s rounds)"
              % (k, cl["rounds"] if cl else "-"))
        low = [s for s in stay if cs[k]["pos"].get(s, 0) < L.minpos]
        ck.ok(not low, "p%d staying slots moved (low %s)" % (k, low or "-"))
    ok, msg = sweep(texts, stay, 24, run.dir)
    ck.ok(ok, msg)
    ck.note("close " + close_summary(cs))
    return ck, wall, rcs


def r_late(L, run, r):
    """Child n-1 is launched 10 s after the others: seated (or refused out loud),
    and every seated console closes on one input delay after >= 200 rounds."""
    n, frames = r.n, r.frames or 900
    relay = run.start_relay()
    specs = []
    for k in range(n):
        d, e = vs_env(run, k, n, frames, relay, r.paced, r.rtt)
        specs.append((k, e, d, 0.0 if k == 0 else (10.0 if k == n - 1 else 1.0 + 0.3 * k)))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, r.paced, 20))
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    late = n - 1
    seated = cs[late]["session"]
    refused = bool(re.search(r"(?i)refus", texts[late]))
    ck.ok(seated or refused, "late p%d seated=%s refused-aloud=%s" % (late, seated, refused))
    for k in range(n):
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        ck.ok(not cs[k]["faults"], "p%d faults=%d" % (k, len(cs[k]["faults"])))
    keys = list(range(n)) if seated else list(range(n - 1))
    for k in keys:
        cl = cs[k]["close"]
        ck.ok(cl and cl["rounds"] >= 200, "p%d >= 200 rounds (%s)" % (k, cl["rounds"] if cl else "-"))
    delays = sorted({cs[k]["close"]["indelay"] for k in keys if cs[k]["close"]})
    ck.ok(len(delays) == 1, "one input delay among seated consoles (%s)" % delays)
    ok, msg = sweep(texts, list(range(n - 1)), 24, run.dir)
    ck.ok(ok, msg + " (on-time consoles)")
    ck.note("close " + close_summary(cs))
    return ck, wall, rcs


def r_kill(L, run, r):
    """Child n-1 is killed (no BYE) 12 s after its spawn: the host retires it in
    about 3 s and the rest finish rc 0 with a clean sweep (design 4.2 d)."""
    n, frames = r.n, r.frames or 1200
    relay = run.start_relay()
    specs = []
    for k in range(n):
        d, e = vs_env(run, k, n, frames, relay, r.paced, r.rtt)
        specs.append((k, e, d, 0.0 if k == 0 else 1.0 + 0.3 * k))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, r.paced),
                                  actions=[(n - 1, 12.0, "kill", None)])
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    stay = list(range(n - 1))
    for k in stay:
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        ck.ok(not cs[k]["faults"], "p%d faults=%d" % (k, len(cs[k]["faults"])))
        cl = cs[k]["close"]
        ck.ok(cl and cl["rounds"] >= frames - 60, "p%d finished (%s rounds)" % (k, cl["rounds"] if cl else "-"))
    ck.ok(bool(re.search(r"(?i)slot %d[^\n]*retir|retir[^\n]*slot %d" % (n - 1, n - 1), texts[0])),
          "parent retired the killed slot %d" % (n - 1))
    ok, msg = sweep(texts, stay, 24, run.dir)
    ck.ok(ok, msg)
    ck.note("close " + close_summary(cs))
    return ck, wall, rcs


def r_laggard(L, run, r):
    """One child hitches (N3_HITCH_ENV) or has +150 ms one way on its own link.
    The others' stall count must stay at a no-hitch baseline of the same shape;
    the hitching child catches up; every pairing stays in sync."""
    n, frames = r.n, r.frames or 1500
    res = {}
    for arm in ("base", "hitch"):
        sub = Run(L, Rung(r.name + "-" + arm, r.kind, n, r.tiers, r.transport, r.rtt, r.paced),
                  run.idx * 2 + (arm == "hitch"), subdir=os.path.join(run.dir, arm))
        relay = sub.start_relay()
        specs = []
        for k in range(n):
            extra = None
            if arm == "hitch" and k == n - 1:
                extra = dict(N3_HITCH_ENV) if r.opts.get("hitch") == "pause" else \
                    {"SM64DS_COMMS_DELAY_MS": str(r.rtt // 2 + 150)}
            d, e = vs_env(sub, k, n, frames, relay, r.paced, r.rtt, cwd_env=extra)
            specs.append((k, e, d, 0.0 if k == 0 else 1.0 + 0.3 * k))
        rcs, texts, wall = sub.launch(specs, timeout_for(n, frames, r.paced))
        if sub.aborted:
            run.aborted = sub.aborted
        res[arm] = (rcs, texts, {k: parse(t) for k, t in texts.items()}, sub, wall)
    ck = Checks()
    rcs, texts, cs, sub, wall = res["hitch"]
    base = res["base"][2]
    for k in range(n):
        ck.ok(rcs.get(k) == 0, "hitch p%d rc=%s" % (k, rcs.get(k)))
    for k in range(n - 1):
        b, h = base[k]["starve_events"], cs[k]["starve_events"]
        ck.ok(h <= b + max(3, b // 4), "p%d stalls %d vs no-hitch baseline %d" % (k, h, b))
    cl = cs[n - 1]["close"]
    catch = int(cl["extra"].get("catchup", "0").split(",")[0] or 0) if cl else 0
    ck.ok(catch > 0, "the hitching child caught up (catchup=%s)" % catch)
    ok, msg = sweep(texts, list(range(n)), 24, sub.dir)
    ck.ok(ok, msg)
    ck.note("base close " + close_summary(base))
    ck.note("hitch close " + close_summary(cs))
    return ck, wall + res["base"][4], rcs


def r_digest(L, run, r):
    """Parent and child disagree on one setting (Luigi Infection on the child
    only): the child refuses the session with exit code 3, the parent plays on."""
    frames = r.frames or 600
    specs = []
    for k in range(2):
        d, e = vs_env(run, k, 2, frames, None, False, 0)
        if k == 1:
            e["SM64DS_VS_LUIGI_INFECTION"] = "1"
        specs.append((k, e, d, 0.0 if k == 0 else 1.0))
    rcs, texts, wall = run.launch(specs, timeout_for(2, frames, False))
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    ck.ok(rcs.get(1) == N1_MISMATCH_RC, "child refused: rc=%s (want %d)" % (rcs.get(1), N1_MISMATCH_RC))
    ck.ok(rcs.get(0) == 0, "parent rc=%s" % rcs.get(0))
    ck.ok(not cs[0]["faults"] and not cs[1]["faults"], "no faults")
    ck.ok(cs[0]["selftest"] is not None, "parent played on to its selftest end")
    return ck, wall, rcs


def luigi_facts(t):
    seed = LUIGI_SEED.findall(t)
    tags = [m.group(0) for m in LUIGI_TAG.finditer(t)]
    return (seed[0] if seed else None), tags


def r_luigi(L, run, r):
    n, frames = r.n, r.frames or 900
    relay = run.start_relay()
    specs = []
    for k in range(n):
        d, e = vs_env(run, k, n, frames, relay, r.paced, r.rtt,
                      cwd_env={"SM64DS_VS_LUIGI_INFECTION": "1",
                               "SM64DS_VS_LUIGI_SEED": str(r.opts.get("seed", 7))})
        specs.append((k, e, d, 0.0 if k == 0 else 1.0 + 0.3 * k))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, r.paced))
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    for k in range(n):
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        ck.ok(not cs[k]["faults"], "p%d faults=%d" % (k, len(cs[k]["faults"])))
    facts = {k: luigi_facts(texts[k]) for k in range(n)}
    seeds = {f[0] for f in facts.values()}
    ck.ok(len(seeds) == 1 and None not in seeds, "one Luigi seed pick on every console (%s)" % seeds)
    tagsets = {tuple(f[1]) for f in facts.values()}
    ck.ok(len(tagsets) == 1, "identical TAG sequence on every console (%d distinct, %d tags on p0)"
          % (len(tagsets), len(facts[0][1])))
    ok, msg = sweep(texts, list(range(n)), 24, run.dir)
    ck.ok(ok, msg)
    relay_checks(ck, run)
    return ck, wall, rcs


def r_luigiseeds(L, run, r):
    """Two 2-console runs with different seeds: each run agrees with itself,
    and the two first picks differ."""
    picks, wall_all, rcs_all = [], 0.0, {}
    ck = Checks()
    for i, seed in enumerate((1, 2)):
        sub = Run(L, Rung("%s-seed%d" % (r.name, seed), "luigi", 2, r.tiers), run.idx * 2 + i,
                  subdir=os.path.join(run.dir, "seed%d" % seed))
        specs = []
        for k in range(2):
            d, e = vs_env(sub, k, 2, 600, None, False, 0,
                          cwd_env={"SM64DS_VS_LUIGI_INFECTION": "1", "SM64DS_VS_LUIGI_SEED": str(seed)})
            specs.append((k, e, d, 0.0 if k == 0 else 1.0))
        rcs, texts, wall = sub.launch(specs, timeout_for(2, 600, False))
        if sub.aborted:
            run.aborted = sub.aborted
        wall_all += wall
        rcs_all.update({"%d.%d" % (i, k): v for k, v in rcs.items()})
        f = [luigi_facts(texts[k])[0] for k in range(2)]
        ck.ok(f[0] is not None and f[0] == f[1], "seed %d: both consoles pick %s" % (seed, f))
        picks.append(f[0])
    ck.ok(picks[0] != picks[1], "different seeds, different first pick (%s)" % picks)
    return ck, wall_all, rcs_all


def adv_env(run, k, n, frames):
    d, e = run.base_env(k)
    e["_EXE"] = run.L.exe
    e.update({"SM64DS_ADVENTURE": "1", "SM64DS_VS_PLAYERS": str(n), "SM64DS_SYNC_REPORT": "1",
              "SM64DS_ADVENTURE_DIAG": "60", "SM64DS_LEVEL": "1", "SM64DS_COMMS_REPORT": "1",
              "SM64DS_COMMS_ROLE": "parent" if k == 0 else "child",
              "SM64DS_COMMS_PORT": str(run.port), "SM64DS_WINDOW_SELFTEST": str(frames)})
    if k:
        e["SM64DS_COMMS_SLOT"] = str(k)
    return d, e


def seen_all(ck, k, c, others, rc):
    ck.ok(rc == 0, "p%d rc=%s" % (k, rc))
    ck.ok(not c["faults"], "p%d faults=%d" % (k, len(c["faults"])))
    ck.ok(c["local_writes"] in (0, None), "p%d never writes its own body (local_writes=%s)"
          % (k, c["local_writes"]))
    ck.ok((c["applied"] or 0) > 0, "p%d peer frames applied (%s)" % (k, c["applied"]))
    miss = [s for s in others if c["vis"].get(s) != 1]
    ck.ok(not miss, "p%d sees every other console (missing %s)" % (k, miss or "-"))


def r_adv(L, run, r):
    """Today's adventure transport (ghosts on the VS carrier), level 1 for all."""
    n, frames = r.n, r.frames or 360
    specs = []
    for k in range(n):
        d, e = adv_env(run, k, n, frames)
        specs.append((k, e, d, 0.0 if k == 0 else 1.0 + 0.4 * k))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, False, 60))
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    for k in range(n):
        seen_all(ck, k, cs[k], [s for s in range(n) if s != k], rcs.get(k))
    return ck, wall, rcs


def hub_env(run, k, frames, relay):
    d, e = run.base_env(k)
    e["_EXE"] = run.L.exe
    e.update(HUB_ENV)
    e["SM64DS_COMMS_RELAY"] = relay
    e["SM64DS_COMMS_CODE"] = run.code
    e["SM64DS_WINDOW_SELFTEST"] = str(frames)
    return d, e


def hub_console(ck, k, c, rc):
    ck.ok(rc == 0, "p%d rc=%s" % (k, rc))
    ck.ok(not c["faults"], "p%d faults=%d" % (k, len(c["faults"])))
    ck.ok(c["selftest"] is not None, "p%d reached its frame budget" % k)
    ck.ok(c["rb"] is None or c["rb"] == (0, 0), "p%d no rounds: rewinds/stalled %s" % (k, c["rb"]))
    ck.ok(c["starve_events"] == 0, "p%d never starved (%d)" % (k, c["starve_events"]))


def r_hub(L, run, r):
    """Hub members through relay v2: no rounds, every console sees every other.
    Seats are the relay's, so visibility is judged as a count of live ghosts."""
    n, frames = r.n, r.frames or 450
    relay = run.start_relay()
    specs = []
    for k in range(n):
        d, e = hub_env(run, k, frames, relay)
        specs.append((k, e, d, 0.4 * k))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, False, 60))
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    for k in range(n):
        hub_console(ck, k, cs[k], rcs.get(k))
        seen = sum(1 for v in cs[k]["vis"].values() if v == 1)
        ck.ok(seen == n - 1, "p%d sees %d of %d others" % (k, seen, n - 1))
    relay_checks(ck, run)
    return ck, wall, rcs


def r_hubjoin(L, run, r):
    """Console n-1 joins about 10 s in and leaves about 10 s later: the others
    keep running and never stall; while it is present they see it."""
    n, frames = r.n, r.frames or 1200
    relay = run.start_relay()
    specs = []
    for k in range(n):
        d, e = hub_env(run, k, frames if k < n - 1 else 300, relay)
        specs.append((k, e, d, 0.4 * k if k < n - 1 else 10.0))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, False, 60))
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    for k in range(n):
        hub_console(ck, k, cs[k], rcs.get(k))
    for k in range(n - 1):
        seen = sum(1 for v in cs[k]["vis"].values() if v == 1)
        ck.ok(seen == n - 2, "p%d at the end sees the %d who stayed (%d)" % (k, n - 2, seen))
    ck.ok(sum(1 for v in cs[n - 1]["vis"].values() if v == 1) == n - 1,
          "the joiner saw every other console")
    relay_checks(ck, run)
    return ck, wall, rcs


def r_hubpause(L, run, r):
    """Console 1 is suspended for 5 s (the whole process): nobody else waits."""
    n, frames = r.n, r.frames or 600
    relay = run.start_relay()
    specs = []
    for k in range(n):
        d, e = hub_env(run, k, frames, relay)
        specs.append((k, e, d, 0.4 * k))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, False, 60),
                                  actions=[(1, 8.0, "suspend", 5.0)])
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    for k in range(n):
        hub_console(ck, k, cs[k], rcs.get(k))
    relay_checks(ck, run)
    return ck, wall, rcs


def r_chat(L, run, r, hub=False):
    """Console 1 sends one scripted line; every other console shows it once,
    attributed to the sender's seat."""
    n, frames = r.n, r.frames or 600
    relay = run.start_relay()
    specs = []
    for k in range(n):
        if hub:
            d, e = hub_env(run, k, frames, relay)
        else:
            d, e = vs_env(run, k, n, frames, relay, False, 0)
        if k == 1:
            e[CHAT_SEND_ENV] = "150:ladder chat %s" % run.code
        if r.opts.get("loss"):
            e["SM64DS_COMMS_DROP"] = str(r.opts["loss"])
        specs.append((k, e, d, 0.0 if k == 0 else 1.0 + 0.3 * k))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, False))
    ck = Checks()
    for k in range(n):
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        if k == 1:
            continue
        rx = CHAT_RX.findall(texts[k])
        ids = {(s, i) for s, i in rx}
        ck.ok(len(ids) == 1 and len(rx) == 1, "p%d shows the line once (%s)" % (k, rx[:4]))
    if not hub:
        ok, msg = sweep(texts, list(range(n)), 24, run.dir)
        ck.ok(ok, msg + " (while chatting)")
    relay_checks(ck, run)
    return ck, wall, rcs


def r_voice(L, run, r):
    """Every console talks (a generated tone, no device) and listens: each one's
    [voice] report must list every other seat with audio buffered at least once."""
    n, frames = r.n, r.frames or 900
    for shadow in (os.path.join(os.path.dirname(L.exe), "settings.json"),
                   os.path.join(L.asset_root, "settings.json")):
        if os.path.exists(shadow):
            raise Skip("%s would shadow the per-console settings.json" % shadow)
    relay = run.start_relay()
    specs = []
    for k in range(n):
        d, e = vs_env(run, k, n, frames, relay, True, 0)
        write_settings(d, voice=True)
        e.update({"SM64DS_VOICE_TEST_TONE": "1", "SM64DS_VOICE_REPORT": "1", "SM64DS_NO_AUDIO": "1",
                  "SM64DS_VOICE_ALLOWED": "1"})
        specs.append((k, e, d, 0.0 if k == 0 else 1.0 + 0.3 * k))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, True))
    ck = Checks()
    for k in range(n):
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        reps = VOICE.findall(texts[k])
        if not reps:
            ck.ok(False, "p%d no [voice] report" % k)
            continue
        tx, rx = int(reps[-1][4]), int(reps[-1][5])
        heard = set()
        for rep in reps:
            for s, dist, g, buf in VOICE_SEAT.findall(rep[8]):
                if int(buf) > 0 or float(g) > 0:
                    heard.add(int(s))
        miss = [s for s in range(n) if s != k and s not in heard]
        ck.ok(tx > 0 and rx > 0, "p%d tx=%d rx=%d" % (k, tx, rx))
        ck.ok(not miss, "p%d hears every other seat (missing %s)" % (k, miss or "-"))
    ok, msg = sweep(texts, list(range(n)), 24, run.dir)
    ck.ok(ok, msg + " (voice on)")
    relay_checks(ck, run)
    return ck, wall, rcs


def kit_bundle(L):
    """A player-shaped install: the kit staged into a fresh directory, the
    kit's own extractor run on the ROM exactly as play.bat runs it (kit_smoke's
    stage + run_extractor), then the exe under test copied in. Cached per kit
    under <out-root>/kit so a ladder does not re-extract per rung. The ROM copy
    is deleted from the drop folder once extraction is done."""
    import kit_smoke
    import pathlib
    work = pathlib.Path(L.kit_work)
    bundle = work / "bundle"
    if not (bundle / "build" / "assets" / "romdata.bin").exists():
        work.mkdir(parents=True, exist_ok=True)
        bundle = kit_smoke.stage(pathlib.Path(L.kit), pathlib.Path(L.rom), work)
        rc = kit_smoke.run_extractor(bundle)
        for drop in ("PLACE YOUR ROM HERE", "PLACE EU ROM HERE"):
            for f in (bundle / drop).glob("*") if (bundle / drop).is_dir() else ():
                if f.suffix.lower() == ".nds":
                    f.unlink()
        if rc != 0:
            raise Skip("the kit extractor failed (rc %d)" % rc)
    shutil.copy2(L.exe, str(bundle / "walk_window.exe"))
    return str(bundle)


def r_kit(L, run, r):
    """The 09-26 host fault re-check: a direct pair from a player-shaped install
    in the launcher's shape (asset root = the extracted bundle, the bundle's
    exe, no FANOUT, no FAULTS_FATAL, the default net mode)."""
    bundle = kit_bundle(L)
    exe = os.path.join(bundle, "walk_window.exe")
    frames = r.frames or 600
    specs = []
    for k in range(2):
        d, e = vs_env(run, k, 2, frames, None, False, 0, netmode=None, exe=exe, asset_root=bundle)
        e.pop("SM64DS_COMMS_FANOUT", None)
        e.pop("SM64DS_FAULTS_FATAL", None)
        e.pop("SM64DS_VS_STATE_HASH", None)
        e["SM64DS_VS_NAMES"] = "ladderp,ladderc"
        specs.append((k, e, d, 0.0 if k == 0 else 1.0))
    rcs, texts, wall = run.launch(specs, timeout_for(2, frames, False))
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    for k in range(2):
        console_vs(ck, k, cs[k], rcs.get(k), 2, L.minpos, want_mode=None)
    ck.note("kit bundle exe md5 %s" % hashlib.md5(open(exe, "rb").read()).hexdigest())
    return ck, wall, rcs


BODIES = {"vs": r_vs, "match": r_match, "leave": r_leave, "late": r_late, "kill": r_kill,
          "laggard": r_laggard, "digest": r_digest, "luigi": r_luigi, "luigiseeds": r_luigiseeds,
          "adv": r_adv, "hub": r_hub, "hubjoin": r_hubjoin, "hubpause": r_hubpause,
          "chat": r_chat, "chathub": lambda L, run, r: r_chat(L, run, r, hub=True),
          "voice": r_voice, "kit": r_kit}


# ---------------------------------------------------------------------------
# THE LADDER
# ---------------------------------------------------------------------------
class Ladder:
    def __init__(self, a):
        self.a = a
        self.exe = os.path.abspath(a.exe)
        self.asset_root = os.path.abspath(a.asset_root or ROOT)
        self.relay_py = os.path.abspath(a.relay_py)
        self.relay1_py = os.path.abspath(a.relay1_py or a.relay_py)
        self.busy_log = os.path.expanduser(a.busy_log)
        self.minpos = a.minpos
        self.kit, self.rom = a.kit, a.rom
        stamp = datetime.datetime.utcnow().strftime("%Y%m%d-%H%M%S")
        self.out = os.path.abspath(a.out or os.path.join(ROOT, "build", "mp_ladder", stamp))
        self.kit_work = os.path.abspath(a.kit_work or os.path.join(os.path.dirname(self.out), "kit"))
        self.rows = []

    def run_rung(self, r, idx, feats, rerun=False):
        miss = feats.missing([x for x in r.needs if x != "kit"])
        if "kit" in r.needs and not (self.kit and self.rom):
            return "SKIP", 0.0, "needs --kit and --rom (a player-shaped install)", {}
        if miss:
            return "SKIP", 0.0, "needs " + ", ".join("%s (%s)" % (m, LANE_NAMES.get(m, m))
                                                     for m in miss), {}
        ok, why = mem_ok(r.n)
        if not ok:
            raise Stop("memory: " + why)
        wait_not_busy(self.busy_log, self.a.busy_wait)
        os.environ["SM64DS_BUILD_LOCK_PATH"] = self.a.lock
        lock = build_lock.acquire(label="mp_ladder-%s" % r.name, root=ROOT,
                                  timeout=self.a.lock_timeout)
        try:
            run = Run(self, r, idx, os.path.join(self.out, r.name + ("-rerun" if rerun else "")))
            try:
                ck, wall, rcs = BODIES[r.kind](self, run, r)
            except Skip as s:
                return "SKIP", 0.0, str(s), {}
            finally:
                if run.relay and run.relay_stats is None:
                    run.relay_stats = run.relay.stop()
        finally:
            build_lock.release(lock)
        if run.aborted:
            return "ABORTED", wall, run.aborted, rcs
        bad = ck.failed()
        detail = "; ".join(bad[:4]) if bad else "; ".join(m for _, m in ck.items if "SWEEP" in m)
        if ck.info:
            detail += " | " + " | ".join(ck.info)
        with open(os.path.join(run.dir, "verdict.txt"), "w") as f:
            for okk, m in ck.items:
                f.write("%s %s\n" % ("ok  " if okk else "FAIL", m))
            for m in ck.info:
                f.write("info %s\n" % m)
        if any(v == -9 for v in rcs.values()):
            return "TIMEOUT", wall, detail, rcs
        return ("FAIL" if bad else "PASS"), wall, detail, rcs

    def record(self, r, verdict, wall, detail, rcs, note=""):
        row = (r.name, verdict, "%.0f" % wall, r.desc(), note + detail)
        self.rows.append(row)
        print("%-8s %-34s %5ss  %s" % (verdict, r.name, row[2], (note + detail)[:400]), flush=True)
        with open(os.path.join(self.out, "summary.tsv"), "a", encoding="utf-8") as f:
            f.write("\t".join(row) + "\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", default=DEFAULT_EXE)
    ap.add_argument("--asset-root", help="SM64DS_ASSET_ROOT for the consoles (default: this repo)")
    ap.add_argument("--relay-py", default=RELAY_PY, help="relay source for relay2 rungs (240 pps)")
    ap.add_argument("--relay1-py", help="relay source for relay1 rungs (120 pps); default --relay-py."
                    " Point it at a copy of the deployed v1 relay to prove the v1 fallback.")
    ap.add_argument("--lock", default=os.environ.get("SM64DS_LADDER_LOCK"),
                    help="the machine-wide ladder lockfile (or $SM64DS_LADDER_LOCK)")
    ap.add_argument("--lock-timeout", type=int, default=1200)
    ap.add_argument("--busy-log", default="~/.claude/scripts/league_chill.log")
    ap.add_argument("--busy-wait", type=int, default=0, help="seconds to wait for the busy flag")
    ap.add_argument("--tier", default="gate", choices=("dev", "gate", "full"))
    ap.add_argument("--only", help="regex over rung names (applied after --tier)")
    ap.add_argument("--skip", help="regex over rung names to leave out")
    ap.add_argument("--assume", default="", help="comma list of lanes to treat as landed")
    ap.add_argument("--minpos", type=int, default=100, help="distinct positions per slot")
    ap.add_argument("--kit", help="a fresh kit directory (the release zip unpacked, never run)")
    ap.add_argument("--rom", help="the ROM the kit extractor reads")
    ap.add_argument("--kit-work", help="where the extracted player install is cached")
    ap.add_argument("--out")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--probe", action="store_true")
    ap.add_argument("--no-rerun", action="store_true", help="do not re-run TIMEOUT rows")
    a = ap.parse_args(argv)

    if not os.path.exists(a.exe):
        print("no exe at %s" % a.exe)
        return 2
    feats = Features(os.path.abspath(a.exe), os.path.abspath(a.relay_py),
                     [x.strip() for x in a.assume.split(",") if x.strip()])
    print("exe %s md5 %s map=%s relay v%d (%s)" % (a.exe, feats.md5, "yes" if feats.map else "no",
                                                    feats.relay_v, a.relay_py))
    for lane in LANE_MARKERS:
        print("  lane %-2s %-10s %s  [%s]" % (lane, LANE_NAMES[lane],
                                              "present" if feats.have[lane] else "absent ",
                                              feats.why[lane]))
    for shadow in (os.path.join(os.path.dirname(os.path.abspath(a.exe)), "settings.json"),
                   os.path.join(os.path.abspath(a.asset_root or ROOT), "settings.json")):
        if os.path.exists(shadow):
            print("  WARNING: %s is read before each console's own settings.json" % shadow)
    if a.probe:
        return 0
    rungs = [r for r in build_rungs() if a.tier in r.tiers]
    if a.only:
        rungs = [r for r in rungs if re.search(a.only, r.name)]
    if a.skip:
        rungs = [r for r in rungs if not re.search(a.skip, r.name)]
    if a.list:
        for r in rungs:
            miss = feats.missing([x for x in r.needs if x != "kit"])
            if "kit" in r.needs and not (a.kit and a.rom):
                miss.append("kit")
            print("%-34s %-40s %s" % (r.name, r.desc(), ("needs " + ",".join(miss)) if miss else "runs"))
        print("%d rungs in tier %s" % (len(rungs), a.tier))
        return 0
    if not a.lock:
        print("refusing to launch without --lock (or $SM64DS_LADDER_LOCK): one multi-console run "
              "at a time machine-wide")
        return 2
    L = Ladder(a)
    os.makedirs(L.out, exist_ok=True)
    with open(os.path.join(L.out, "summary.tsv"), "w", encoding="utf-8") as f:
        f.write("rung\tverdict\twall_s\tshape\tdetail\n")
    print("out %s, %d rungs, tier %s, ports from pid %d" % (L.out, len(rungs), a.tier, PID), flush=True)
    rerun = []
    stopped = ""
    for idx, r in enumerate(rungs):
        try:
            v, wall, detail, rcs = L.run_rung(r, idx, feats)
        except (Stop, build_lock.BuildLockTimeout) as s:
            stopped = str(s)
            print("STOPPED at %s: %s" % (r.name, stopped), flush=True)
            for rest in rungs[idx:]:
                L.record(rest, "NOTRUN", 0.0, stopped, {})
            break
        L.record(r, v, wall, detail, rcs)
        if v == "ABORTED":
            stopped = detail
            for rest in rungs[idx + 1:]:
                L.record(rest, "NOTRUN", 0.0, stopped, {})
            break
        if v == "TIMEOUT":
            rerun.append((idx, r))
    if rerun and not stopped and not a.no_rerun:
        print("re-running %d TIMEOUT row(s) alone" % len(rerun), flush=True)
        for idx, r in rerun:
            try:
                v, wall, detail, rcs = L.run_rung(r, idx + 1000, feats, rerun=True)
            except (Stop, build_lock.BuildLockTimeout) as s:
                L.record(r, "NOTRUN", 0.0, str(s), {}, note="rerun: ")
                stopped = str(s)
                break
            L.record(r, v, wall, detail, rcs, note="rerun: ")
    final = collections.OrderedDict()
    for name, v, *_ in L.rows:
        final[name] = v
    counts = collections.Counter(final.values())
    line = "LADDER %s: %s" % (os.path.basename(L.out), " ".join("%s=%d" % kv for kv in sorted(counts.items())))
    print(line)
    with open(os.path.join(L.out, "summary.txt"), "w", encoding="utf-8") as f:
        f.write("exe %s md5 %s\n" % (L.exe, feats.md5))
        for lane in LANE_MARKERS:
            f.write("lane %s %s %s\n" % (lane, "present" if feats.have[lane] else "absent", feats.why[lane]))
        for row in L.rows:
            f.write("%-8s %-34s %5ss  %s\n" % (row[1], row[0], row[2], row[4]))
        f.write(line + "\n")
    if stopped:
        return 2
    return 1 if counts.get("FAIL") or counts.get("TIMEOUT") else 0


if __name__ == "__main__":
    sys.exit(main())
