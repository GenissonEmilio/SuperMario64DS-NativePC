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
                                   [--hold-s S] [--busy-wait S]
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
was nonzero at least once, and the SESSION LASTED THE RUN: its close line
reaches the frame budget (a match: its MATCH OVER frame) minus a margin. A
session that drops part-way leaves every console playing on alone, rc 0, every
body moving and every log agreeing up to the drop, so only the round count
tells. Per rung: the dhdiff sweep over EVERY pairing (parent first, then child
pairs, trimmed to the shortest log minus a tail margin) whose window must also
reach the budget, one input delay on every console, and every local relay
counter that means a lost datagram (every drop_*, bad, full) at 0. Match rungs
add the session and live-mask checks, exactly one MATCH OVER line per console,
identical on every console, and the "[vs] closing" exit. The chosen input delay and the starved /
subst / late / catchup counters are printed on every VS row.

SAVES. Every console gets its own SM64DS_SAVE_PATH (the default,
<exe folder>/save/sm64ds.sav, would be one file shared by every console of a
one-machine run), and that shared default is hashed around every rung: any
write to it is a red. The save rows: adventure consoles each collect a star
and save their own valid cartridge; a VS match leaves the host's real save
byte-identical and joiners write none.

THE ENVIRONMENT RULES THIS HARNESS KEEPS (the run's standing orders):
  - every console hidden and muted: mp2_proof.spawn (CREATE_NO_WINDOW +
    SW_SHOWMINNOACTIVE) and mp2_proof.env_base (SM64DS_NO_FOCUS,
    SM64DS_MINIMIZED, SM64DS_VOLUME=0); voice rungs add SM64DS_NO_AUDIO=1;
  - one multi-console run at a time machine-wide: every rung holds --lock (a
    build_lock.py lockfile; the ladder refuses to launch without one) for its
    own launch only, so other lanes' rungs interleave between ours; --hold-s S
    keeps it across consecutive rungs for up to S seconds when the queue is
    long (every rung otherwise goes to the back of the line);
  - an optional busy flag: --busy-log FILE (default $SM64DS_BUSY_LOG; unset
    means no busy flag at all). When the last line of that file contains
    "chilling" (the machine's owner needs it quiet), the ladder does not start
    a rung, and a rung already running is killed and recorded as ABORTED;
    --busy-wait S waits up to S seconds for the flag to clear;
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

ARENA MODE (run mp1, lane ARENAGATE1, phase A6): the arena-* rungs prove the new VS
(referee on the relay, own body at 0 frames, puppets, stars, hits, time-up / first-to-N,
leave) and the lock2-lat-control rung is the lockstep latency control. They are listed
and judged in the ARENA MODE RUNGS block below; every console on them is started with
the launcher's own arena env (no SM64DS_LEVEL, no selftest), judged by arena_logs.py
(each judge has a must-FAIL control run against the real logs), and the real-launcher
rungs go through arena_e2e.py. Extra arguments: --base-exe (the solo gate's base build),
--launcher-repo / --kit-exe / --e2e-assets / --e2e-extracted / --e2e-rom (the MpE2E
rungs). The fold lane's arena gate is   --only '^(arena|lock2-lat)' --tier gate   plus
the lockstep fallback rungs unchanged (--only '^(vs[24]-loop|match|timeup|leave4|latejoin|luigi4)').

OUTPUT: <out>/<rung>/p<k>/{run.log,env.txt} per console (+ relay.log),
<out>/summary.tsv and <out>/summary.txt. Default <out> is
build/mp_ladder/<UTC stamp> under the repo root.
"""

import argparse
import collections
import math
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
import arena_logs as AL        # noqa: E402  (arena-mode log readers + judges, lane ARENAGATE1)

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
    # text chat: the map marker only ("SYNT" is in the exe from VSAUX1's kind table)
    "C1": [("map", "adv_chat.cpp")],
    "V1": [("env", "SM64DS_VOICE_ALLOWED")],                      # mic gate + room setting
}
LANE_NAMES = {"N1": "NETCORE1", "N2": "VSAUX1", "N3": "LAGGARD1", "R1": "RELAY2",
              "H1": "HUB1", "W2": "GHOSTWIRE1", "C1": "CHATUI1", "V1": "VOICE1"}
# ARENA MODE (run mp1 phase A, lane ARENAGATE1): the five lanes the arena rungs wait on.
LANE_MARKERS.update({
    "A0": [("str", b"[vsarena] installed"), ("env", "SM64DS_VS_NETMODE")],     # ARENACORE1: core + SYNS
    "A1": [("env", "SM64DS_PUPPET_LOG")],                                      # PUPPET1: other seats' bodies
    "A2": [("relayarena", 1)],                                                 # REFEREE1: the relay's arena session + referee
    "A3": [("env", "SM64DS_ARENA_RIG"), ("str", b"[vsarena-ref]")],            # STARREF1: referee client (stars, hits, MATCH OVER)
})
LANE_NAMES.update({"A0": "ARENACORE1", "A1": "PUPPET1", "A2": "REFEREE1", "A3": "STARREF1"})

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
# W2: every hub member's periodic report; vis = the seats it hears, drawn = the
# seats it draws a body for (both masks by relay seat, its own bit clear).
GHOST_TICK = re.compile(r"^\[ghost:tick\] hub=1 seat=(\d+) [^\n]*? vis=0x([0-9a-f]+) "
                        r"drawn=0x([0-9a-f]+)", re.M)
# hal/comms_conductor.cpp kHubBodies = 4: a hub console seats its own player
# and three ghost bodies, handed each frame to the three nearest players; the
# rest of the room stays on the wire (names, chat, voice) with no body.
HUB_DRAWN = 3
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
    drawn_at = collections.defaultdict(int)
    for m in VIS.finditer(t):
        vis[int(m.group(1))] = int(m.group(2))
        f = re.match(r"\[advdiag\] f(\d+)", m.group(0))
        if f and m.group(2) == "1":
            drawn_at[int(f.group(1))] += 1
    c["vis"] = vis
    c["drawn_max"] = max(drawn_at.values(), default=0)
    c["ghost_ticks"] = [(int(a), int(b, 16), int(w, 16)) for a, b, w in GHOST_TICK.findall(t)]
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


class Aborted(Exception):
    """The busy flag came up while a rung's consoles were running or starting."""


def busy(path):
    """The busy flag's last line when it says "chilling", else "". No path
    (no --busy-log and no $SM64DS_BUSY_LOG) means never busy."""
    if not path:
        return ""
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


def file_digest(path):
    try:
        with open(path, "rb") as f:
            return hashlib.md5(f.read()).hexdigest()[:12]
    except OSError:
        return "absent"


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
        self.relay_arena = relay_arena(relay_py) if self.relay_v >= 2 else 0
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
                elif kind == "relayarena":
                    ok = self.relay_arena >= val
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


def relay_arena(relay_py):
    """1 when the relay source accepts an ARENA hello (HELLO v2 role 3, REFEREE1's
    arena_wire.h): an ACK with status 0. A relay that predates arena mode answers
    status 2 (the game logs it and retries every 5 s, never falling to lockstep)."""
    if not os.path.exists(relay_py):
        return 0
    port = relay_port(96)
    env = dict(os.environ, SM64DS_RELAY_PORT=str(port), SM64DS_RELAY_BIND="127.0.0.1",
               SM64DS_RELAY_STATS_S="60")
    p = subprocess.Popen([sys.executable, "-u", relay_py], env=env, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, creationflags=M.NO_CONSOLE)
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(0.5)
    ok = 0
    try:
        code = ("Q%07d" % (PID % 10**7)).encode()
        hello = b"SMRC" + bytes([2, 3, 0xFF, 0]) + code
        time.sleep(0.8)
        for _ in range(10):
            if p.poll() is not None:
                break
            try:
                s.sendto(hello, ("127.0.0.1", port))
                d, _ = s.recvfrom(64)
            except socket.timeout:
                continue
            except OSError:
                time.sleep(0.3)
                continue
            if d[:4] == b"SMRA" and len(d) >= 6:
                ok = 1 if (d[4] == 2 and d[5] == 0) else 0
                break
    finally:
        s.close()
        p.kill()
        p.wait()
    return ok


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
    # Matches to the CLOCK (no star target): the ROM's own time-up and its scene-7
    # request, answered by the port's MATCH OVER path (lane VSTIMEUP1).
    R.append(Rung("timeup2-loop", "timeup", 2, DEV))
    R.append(Rung("timeup4-relay1-rtt80", "timeup", 4, DEV, "relay1", 80))
    R.append(Rung("timeup2-luigi-loop", "timeup", 2, DEV, luigi=True))
    R.append(Rung("timeup2-kit", "timeup", 2, GATE, needs=["kit"], kit=True))
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
    # adv*-loop and adv2-save-loop run adventure over the VS lockstep carrier,
    # which VSAUX1's guard removed: they retire once N2 is in the exe, and the
    # hub* rungs (hub2/4/16, joinleave, pause, hub2-save) cover adventure.
    R.append(Rung("adv2-loop", "adv", 2, DEV, retired_by=["N2"]))
    R.append(Rung("adv4-loop", "adv", 4, DEV, retired_by=["N2"]))
    R.append(Rung("adv16-loop", "adv", 16, GATE, retired_by=["N2"]))
    R.append(Rung("hub2-relay2", "hub", 2, DEV, "relay2", needs=["H1", "W2"]))
    R.append(Rung("hub4-relay2", "hub", 4, DEV, "relay2", needs=["H1", "W2"]))
    R.append(Rung("hub16-relay2", "hub", 16, GATE, "relay2", needs=["H1", "W2"]))
    R.append(Rung("hub4-joinleave-relay2", "hubjoin", 4, DEV, "relay2", needs=["H1", "W2"]))
    R.append(Rung("hub4-pause-relay2", "hubpause", 4, DEV, "relay2", needs=["H1"]))
    # Saves: every console its own cartridge (adventure saves a star each; a VS
    # match leaves the host's save byte-identical and joiners write none).
    R.append(Rung("adv2-save-loop", "advsave", 2, DEV, retired_by=["N2"]))
    R.append(Rung("hub2-save-relay2", "advsave", 2, DEV, "relay2", needs=["H1"], hub=True))
    R.append(Rung("vs2-save-untouched", "vssave", 2, DEV))
    R.append(Rung("vs4-save-untouched-relay1", "vssave", 4, DEV, "relay1"))
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
    return R + arena_rungs()


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
                   SM64DS_RELAY_STATS_S="1")   # 1 s: seating shows in the counters
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
        # (endpoints, drop_target) per stats line, oldest first, for alone_target_drops().
        self.series = []
        for ln in stats:
            ep, dt = re.search(r"\bendpoints=(\d+)", ln), re.search(r"\bdrop_target=(\d+)", ln)
            if ep:
                self.series.append((int(ep.group(1)), int(dt.group(1)) if dt else 0))
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
        # EVERY CONSOLE ITS OWN CARTRIDGE. The default save is <exe folder>/save/
        # sm64ds.sav, so consoles launched from one exe would share one file;
        # on real machines every player has their own. The shared default is
        # hashed around every rung (Ladder.run_rung) so a write to it is a red.
        e["SM64DS_SAVE_PATH"] = os.path.join(d, "sm64ds.sav")
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
        if self.aborted:
            # Some consoles may never have been spawned; no body can judge this.
            raise Aborted(self.aborted)
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


def sweep(texts, keys, margin, workdir, want=None):
    """dhdiff every pairing among keys (first key against the rest, then pairs),
    each log trimmed to the shortest log's hashed rounds minus margin. With
    `want` (the rounds the session had to last) the compared window must also
    reach want minus the margins: a session that dropped early and left every
    console playing alone agrees with itself up to the drop, and that is a red."""
    caps = {k: max([int(m.group(2)) for m in DH.finditer(texts[k])] or [0]) for k in keys}
    cap = min(caps.values()) - margin
    if cap <= 30:
        return False, "too few hashed rounds (%s)" % ",".join("p%d:%d" % kv for kv in caps.items())
    if want is not None and cap < want - margin - LAST_MARGIN:
        return False, "SWEEP window reached only round %d of the %d the session had to last (%s)" % (
            cap, want, ",".join("p%d:%d" % kv for kv in caps.items()))
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
                           text=True, errors="replace", creationflags=M.NO_CONSOLE)
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


# A console's session must reach the frame budget minus this many rounds (1 s).
LAST_MARGIN = 30


def lasted(ck, cs, keys, want, margin=LAST_MARGIN):
    """Every console's session lasted the run: its close line reaches `want`
    rounds minus a margin. Catches a session that drops part-way while every
    console plays on alone (the consoles still exit rc 0, every body still moves,
    and the logs agree up to the drop)."""
    for k in keys:
        cl = cs[k]["close"]
        got = cl["rounds"] if cl else None
        ck.ok(got is not None and got >= want - margin,
              "p%d session lasted the run (%s of %d rounds)" % (k, got, want))


def session_up(ck, k, c, n):
    ck.ok(c["session"], "p%d session up" % k)
    full = (1 << n) - 1
    ck.ok(c["mask"] & full == full, "p%d live mask 0x%x covers 0x%x" % (k, c["mask"], full))


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
        if not L.feats.have.get("N1"):
            raise Skip("needs N1 (%s): the default online mode is still ROLLBACK on this exe"
                       % LANE_NAMES["N1"])
        ck.ok(False, "N1 is in this exe but the default online mode still runs ROLLBACK")
    for k in range(n):
        console_vs(ck, k, cs[k], rcs.get(k), n, L.minpos)
    lasted(ck, cs, range(n), frames)
    delays = sorted({c["close"]["indelay"] for c in cs.values() if c["close"]})
    ck.ok(len(delays) == 1, "one input delay on every console (%s)" % delays)
    margin = max(24, 2 * (delays[-1] if delays else 0) + 8)
    ok, msg = sweep(texts, list(range(n)), margin, run.dir, want=frames)
    ck.ok(ok, msg)
    relay_checks(ck, run)
    a, row = delay_row(cs[0], r.rtt)
    if relay:
        ck.note(row)
        if a and a[3] == N1_PERIOD:
            # Judged against the formula on the RTT the game MEASURED, not the
            # nominal added RTT: the test rig's delay shim holds each datagram to a
            # Windows timer tick (15.6 ms), so "160 ms" measures about 187 ms and
            # delay 9 is the right answer there (review VSAUX1 fold note 6).
            frames_, worst, safety, period, margin_, floor, cap = a
            want = int(math.ceil(int(worst) * int(safety) / 100.0 / float(period))) + int(margin_)
            want = max(int(floor), min(int(cap), want))
            ck.ok(int(frames_) == want, "1.0 delay: %s = ceil(%s ms * %s%% / %s ms) + %s clamped "
                  "[%s, %s] = %d (nominal rtt %d)" % (frames_, worst, safety, period, margin_,
                                                      floor, cap, want, r.rtt))
    ck.note("close " + close_summary(cs))
    ck.note("starve events per console " + ",".join(str(cs[k]["starve_events"]) for k in range(n)))
    return ck, wall, rcs


def alone_target_drops(series):
    """drop_target counts a datagram the relay had nobody to send to. The first
    console into a room is alone until the second one joins, and what it sends
    meanwhile lands there: the relay is right to drop it and nothing was lost.
    Returns how much drop_target grew over stats intervals that began with fewer
    than 2 endpoints (the local relay writes a stats line every second, so the
    seating shows). Growth over an interval that began with 2 or more endpoints
    stays a drop."""
    alone = 0
    for (ep0, dt0), (_, dt1) in zip(series, series[1:]):
        if ep0 < 2 and dt1 > dt0:
            alone += dt1 - dt0
    return alone


def relay_checks(ck, run):
    if run.relay_stats is None:
        return
    st = dict(run.relay_stats)
    alone = alone_target_drops(getattr(run.relay, "series", None) or [])
    if alone and st.get("drop_target"):
        ck.note("relay drop_target: %d sent while a console sat alone in its room (nobody to "
                "send to; not a drop)" % min(alone, st["drop_target"]))
        st["drop_target"] = max(0, st["drop_target"] - alone)
    bad = {k: v for k, v in st.items() if (k.startswith("drop") or k in ("bad", "full")) and v}
    ck.ok(bool(st) and not bad, "relay dropped nothing (%s)" % (
        ", ".join("%s=%d" % kv for kv in sorted(bad.items())) if bad else
        ("no stats line" if not st else "every drop counter, bad and full 0")))
    ck.note("relay rx=%s fwd=%s" % (st.get("rx", "?"), st.get("fwd", "?")))


def match_frame(cs):
    """The frame the match ended on (MATCH OVER f<N>); the session must reach it.
    A console with no MATCH OVER line counts as a match that never ended."""
    ends = [int(re.search(r"MATCH OVER f(\d+)", c["match"][0]).group(1)) for c in cs.values()
            if c["match"] and re.search(r"MATCH OVER f(\d+)", c["match"][0])]
    return max(ends) if ends else 10**6


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
        session_up(ck, k, c, n)
        ck.ok(len(c["match"]) == 1, "p%d one MATCH OVER line (%d)" % (k, len(c["match"])))
        ck.ok(c["closing"], "p%d [vs] closing" % k)
    lines = {c["match"][0] for c in cs.values() if c["match"]}
    ck.ok(len(lines) == 1, "MATCH OVER identical on every console (%d distinct)" % len(lines))
    if lines:
        ck.note(next(iter(lines))[:140])
    end = match_frame(cs)
    delays = [c["close"]["indelay"] for c in cs.values() if c["close"]]
    lasted(ck, cs, range(n), end, margin=10 + 2 * max(delays or [0]))
    ok, msg = sweep(texts, list(range(n)), 24, run.dir, want=end)
    ck.ok(ok, msg)
    relay_checks(ck, run)
    return ck, wall, rcs


CLOCK_END = re.compile(r"^\[vs\] f\d+ (?:TIME UP|LUIGI INFECTION OVER)[^\n]*", re.M)
ABORT = re.compile(r"^(?:FATAL[^\n]*|\[stage\] slot 3[^\n]*|\[cross\][^\n]*)", re.M)


# The floating score popup (actor 330, daObjNumber_c) a star taker gets. Its
# spawn point comes from func_ov002_020e8244, which reads the star's rendered
# model matrix (+0x320), so each window puts it at its own height: measured
# p0 posy 2316829 against p1 1592241 at f328, the only field that differs, and
# the world hash agrees again from f364 when the popup is gone. Presentation,
# like the cull bits comms_sync.cpp already masks; it is in the base build too.
POPUP_ID = 330


def sweep_popup_only(texts, keys, workdir):
    """After a red sweep: is every split frame explained by the score popup
    alone? Needs the level-2 digest. Each pairing must agree on n and the order
    hash on every common frame, and where the world hash differs, every actor
    whose digest differs must be a POPUP_ID. -> (ok, msg)."""
    sys.path.insert(0, HERE)
    import dhdiff
    logs = {k: dhdiff.load(os.path.join(workdir, "p%d" % k, "run.log.trim")) for k in keys}
    first, rest = keys[0], keys[1:]
    pairs = [(first, b) for b in rest] + [(a, b) for i, a in enumerate(rest) for b in rest[i + 1:]]
    split = set()
    for a, b in pairs:
        fa, _, da, _ = logs[a]
        fb, _, db, _ = logs[b]
        for f in sorted(set(fa) & set(fb)):
            if fa[f] == fb[f]:
                continue
            if fa[f][0] != fb[f][0] or fa[f][2] != fb[f][2]:
                return False, "p%d/p%d f%d actor count or order differs" % (a, b, f)
            if f not in da or f not in db:
                return False, "p%d/p%d f%d split with no digest" % (a, b, f)
            odd = [u for u in set(da[f]) | set(db[f]) if da[f].get(u) != db[f].get(u)]
            # the class ids come from BOTH consoles: taking them from one side let a uid
            # holding another class on the other side pass as the popup (REVVSTIMEUP1 M7)
            ids = ({da[f][u][0] for u in odd if u in da[f]}
                   | {db[f][u][0] for u in odd if u in db[f]})
            if not odd or ids != {POPUP_ID}:
                return False, "p%d/p%d f%d actors %s differ" % (a, b, f, sorted(ids))
            split.add(f)
    if not split:
        return True, "no split"
    return True, "every split frame is the score popup alone (actor %d, %d frame(s) f%d..f%d)" % (
        POPUP_ID, len(split), min(split), max(split))


def r_timeup(L, run, r):
    """A match played to the CLOCK (lane VSTIMEUP1): the match rung's recipe with
    no star target, so the ROM's own time-up ends it (f2791) and its own
    Stage::Behavior asks for scene 7. Every console must end through the port's
    MATCH OVER path, answer that request (results_screen=port), print one
    identical clock-end line and one identical MATCH OVER, and exit 0: no FATAL,
    no slot-3 dispatch, no refused crossing. luigi=True keeps Luigi Infection on
    with motionless bodies, so its clock end (survivor-timeout) is the one taken.
    kit=True runs the pair from a player-shaped install (the kit rung's shape)."""
    n = r.n
    frames = r.frames or 3300
    kit = r.opts.get("kit")
    bundle = kit_bundle(L) if kit else None
    exe = os.path.join(bundle, "walk_window.exe") if kit else None
    relay = None if kit else run.start_relay()
    specs = []
    for k in range(n):
        d, e = vs_env(run, k, n, frames, relay, r.paced, r.rtt, match=True,
                      netmode=None if kit else "lockstep", exe=exe, asset_root=bundle)
        e.pop("SM64DS_VS_STAR_TARGET", None)
        if kit:
            e.pop("SM64DS_COMMS_FANOUT", None)
            e.pop("SM64DS_FAULTS_FATAL", None)
            e.pop("SM64DS_VS_STATE_HASH", None)
        if not kit:
            e["SM64DS_VS_STATE_HASH"] = "2"     # the digest names a split actor
        if r.opts.get("luigi"):
            e["SM64DS_VS_LUIGI_INFECTION"] = "1"
            e["SM64DS_VS_LUIGI_SEED"] = "7"
        e["SM64DS_VS_NAMES"] = ",".join("lad%d" % j for j in range(n))
        specs.append((k, e, d, 0.0 if k == 0 else 1.0 + 0.3 * k))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, r.paced, 60))
    cs = {k: parse(t) for k, t in texts.items()}
    want_win = ("survivor-timeout",) if r.opts.get("luigi") else ("time-up",)
    ck = Checks()
    ends = {}
    for k in range(n):
        c = cs[k]
        t = texts[k]
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        ck.ok(not c["faults"], "p%d faults=%d" % (k, len(c["faults"])))
        ab = ABORT.findall(t)
        ck.ok(not ab, "p%d no FATAL / slot-3 / refused crossing %s" % (k, [x[:70] for x in ab[:1]]))
        ck.ok(not c["rollback"], "p%d runs lockstep" % k)
        session_up(ck, k, c, n)
        ce = CLOCK_END.findall(t)
        ends[k] = ce[0] if ce else ""
        ck.ok(len(ce) == 1 and "TARGET REACHED" not in t,
              "p%d the clock ended it (%s)" % (k, (ce[0][:40] if ce else "no clock-end line")))
        ck.ok(len(c["match"]) == 1, "p%d one MATCH OVER line (%d)" % (k, len(c["match"])))
        m = c["match"][0] if c["match"] else ""
        w = re.search(r"\bwin=(\S+)", m)
        ck.ok(bool(w) and w.group(1) in want_win and "results_screen=port" in m,
              "p%d MATCH OVER win=%s results_screen=port" % (k, w.group(1) if w else "?"))
        ck.ok(c["closing"], "p%d [vs] closing" % k)
    ck.ok(len(set(ends.values())) == 1 and all(ends.values()),
          "clock-end line identical on every console (%d distinct)" % len(set(ends.values())))
    lines = {c["match"][0] for c in cs.values() if c["match"]}
    ck.ok(len(lines) == 1, "MATCH OVER identical on every console (%d distinct)" % len(lines))
    if lines:
        ck.note(next(iter(lines))[:160])
    end = match_frame(cs)
    delays = [c["close"]["indelay"] for c in cs.values() if c["close"]]
    lasted(ck, cs, range(n), end, margin=10 + 2 * max(delays or [0]))
    if not kit:
        ok, msg = sweep(texts, list(range(n)), 24, run.dir, want=end)
        if not ok and "DIVERGENCE AT FRAME" in msg:
            pok, pmsg = sweep_popup_only(texts, list(range(n)), run.dir)
            ok, msg = pok, msg + " -> " + pmsg
        ck.ok(ok, msg)
        relay_checks(ck, run)
    else:
        ck.note("kit bundle exe md5 %s" % hashlib.md5(open(exe, "rb").read()).hexdigest())
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
    ok, msg = sweep(texts, stay, 24, run.dir, want=frames)
    ck.ok(ok, msg)
    ck.note("close " + close_summary(cs))
    return ck, wall, rcs


def r_late(L, run, r):
    """Child n-1 is launched 10 s after the others. On the 09-28 base this is a
    DELAYED START rather than a join into a running match: the conductor holds
    the world seat until all SM64DS_VS_PLAYERS consoles are in, so everybody
    starts together once the late one arrives. The late console is seated (or
    refused out loud), every seated console's session lasts the run on one
    input delay, and every seated pairing stays in sync."""
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
    refused = not seated and bool(re.search(r"(?i)refus", texts[late]))
    ck.ok(seated or refused, "late p%d %s" % (late, "seated" if seated else
                                              ("refused out loud" if refused else "neither seated nor refused")))
    for k in range(n):
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        ck.ok(not cs[k]["faults"], "p%d faults=%d" % (k, len(cs[k]["faults"])))
    keys = list(range(n)) if seated else list(range(n - 1))
    lasted(ck, cs, keys, frames)
    delays = sorted({cs[k]["close"]["indelay"] for k in keys if cs[k]["close"]})
    ck.ok(len(delays) == 1, "one input delay among seated consoles (%s)" % delays)
    ok, msg = sweep(texts, keys, 24, run.dir, want=frames)
    ck.ok(ok, msg + " (seated consoles)")
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
    ok, msg = sweep(texts, stay, 24, run.dir, want=frames)
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
    lasted(ck, cs, range(n), frames)
    ok, msg = sweep(texts, list(range(n)), 24, sub.dir, want=frames)
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
        session_up(ck, k, cs[k], n)
    lasted(ck, cs, range(n), frames)
    facts = {k: luigi_facts(texts[k]) for k in range(n)}
    seeds = {f[0] for f in facts.values()}
    ck.ok(len(seeds) == 1 and None not in seeds, "one Luigi seed pick on every console (%s)" % seeds)
    # The parent outlives its children (parent_surplus), so it may tag more
    # after they leave: the sequences must agree over the common prefix.
    common = min(len(f[1]) for f in facts.values())
    tagsets = {tuple(f[1][:common]) for f in facts.values()}
    ck.ok(len(tagsets) == 1, "identical TAG sequence on every console over the first %d tags "
          "(%d distinct; p0 saw %d)" % (common, len(tagsets), len(facts[0][1])))
    ok, msg = sweep(texts, list(range(n)), 24, run.dir, want=frames)
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
    # A player's pace (30 fps), as a real hub runs. The hub rungs are timed on
    # the wall clock (0.4 s start stagger, a joiner 10 s in, [ghost:tick] and
    # the body fade), so unpaced consoles made the verdict depend on how busy
    # the box was: on a quiet box hub16 and hub4-joinleave ended before the
    # last joiner was heard or the leaver had gone.
    e["SM64DS_PACE_SELFTEST"] = "1"
    return d, e


def hub_console(ck, k, c, rc):
    ck.ok(rc == 0, "p%d rc=%s" % (k, rc))
    ck.ok(not c["faults"], "p%d faults=%d" % (k, len(c["faults"])))
    ck.ok(c["selftest"] is not None, "p%d reached its frame budget" % k)
    ck.ok(c["rb"] is None or c["rb"] == (0, 0), "p%d no rounds: rewinds/stalled %s" % (k, c["rb"]))
    ck.ok(c["starve_events"] == 0, "p%d never starved (%d)" % (k, c["starve_events"]))


def heard_all(ck, k, c, n):
    """Hub visibility past HUB_DRAWN others: the console must have heard every
    other console at once (one [ghost:tick] whose vis mask holds n-1 seats, its
    own clear) and, on that tick, drawn bodies for exactly min(HUB_DRAWN, n-1)
    of them, all among the heard."""
    pop = lambda v: bin(v).count("1")
    ticks = c["ghost_ticks"]
    best = max(ticks, key=lambda x: (pop(x[1]), x[1])) if ticks else None
    if best is None:
        ck.ok(False, "p%d hears every other console at once (no [ghost:tick] line)" % k)
        return
    seat, vis, drawn = best
    ck.ok(pop(vis) == n - 1 and not (vis >> seat) & 1,
          "p%d hears all %d others at once (seat %d vis=0x%04x)" % (k, n - 1, seat, vis))
    want = min(HUB_DRAWN, n - 1)
    ck.ok(pop(drawn) == want and not drawn & ~vis,
          "p%d draws the %d nearest of them (drawn=0x%04x)" % (k, want, drawn))


def r_hub(L, run, r):
    """Hub members through relay v2: no rounds, every console hears every other
    and draws bodies for the HUB_DRAWN nearest. Seats are the relay's, so what is
    drawn is judged as a count of live ghost slots."""
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
        if n - 1 <= HUB_DRAWN:
            seen = sum(1 for v in cs[k]["vis"].values() if v == 1)
            ck.ok(seen == n - 1, "p%d sees %d of %d others" % (k, seen, n - 1))
        else:
            # Past HUB_DRAWN the last report is no measure: the consoles end at
            # staggered times, and a leaver's body fades 0.7-1.2 s before the
            # next nearest is drawn. Judge the most drawn at one report instead.
            ck.ok(cs[k]["drawn_max"] == HUB_DRAWN, "p%d draws %d ghosts at once (the %d nearest "
                  "of %d others)" % (k, cs[k]["drawn_max"], HUB_DRAWN, n - 1))
            heard_all(ck, k, cs[k], n)
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
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    for k in range(n):
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        ck.ok(not cs[k]["faults"], "p%d faults=%d" % (k, len(cs[k]["faults"])))
        if not hub:
            session_up(ck, k, cs[k], n)
        if k == 1:
            continue
        rx = CHAT_RX.findall(texts[k])
        ids = {(s, i) for s, i in rx}
        ck.ok(len(ids) == 1 and len(rx) == 1, "p%d shows the line once (%s)" % (k, rx[:4]))
    if not hub:
        lasted(ck, cs, range(n), frames)
        ok, msg = sweep(texts, list(range(n)), 24, run.dir, want=frames)
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
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    lasted(ck, cs, range(n), frames)
    for k in range(n):
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        ck.ok(not cs[k]["faults"], "p%d faults=%d" % (k, len(cs[k]["faults"])))
        session_up(ck, k, cs[k], n)
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
    ok, msg = sweep(texts, list(range(n)), 24, run.dir, want=frames)
    ck.ok(ok, msg + " (voice on)")
    relay_checks(ck, run)
    return ck, wall, rcs


def run_extractor_hidden(bundle):
    """kit_smoke.run_extractor's exact play.bat invocation (the script read as
    text and run as a script block), launched with no console window: from a
    console-less parent the plain call flashes a PowerShell window."""
    script = os.path.join(str(bundle), "extract_assets.ps1")
    cmd = ("$ErrorActionPreference='Stop'; "
           "& ([scriptblock]::Create([IO.File]::ReadAllText("
           "'%s'))) -Destination '%s'" % (script, bundle))
    p = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
                        "-OutputFormat", "Text", "-WindowStyle", "Hidden", "-Command", cmd],
                       cwd=str(bundle), capture_output=True, text=True, errors="replace",
                       creationflags=M.NO_CONSOLE)
    for line in (p.stdout or "").splitlines()[-4:]:
        print("       | " + line)
    return p.returncode


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
        rc = run_extractor_hidden(bundle)
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
    lasted(ck, cs, range(2), frames)
    ck.note("kit bundle exe md5 %s" % hashlib.md5(open(exe, "rb").read()).hexdigest())
    return ck, wall, rcs


def save_records(path):
    """Valid FileSaveData slots in a cartridge image, by the ROM's own framing
    (save_proof.py's reader): tag + recomputed checksum on BOTH copies."""
    import save_proof as S
    try:
        with open(path, "rb") as f:
            img = f.read()
    except OSError:
        return None, []
    good = []
    for slot in range(3):
        a = S.parse_record(img, slot << 7)
        b = S.parse_record(img, S.HALF + (slot << 7))
        if a[0] and b[0] and a[1] == a[2] and b[1] == b[2] and a[4] == b[4]:
            good.append(slot)
    return len(img), good


def r_advsave(L, run, r):
    """Adventure: each console collects a star in Bob-omb Battlefield (the ROM's
    own touch gate) and answers "save and continue" (twostar_fade_proof's
    recipe, staggered per console). Each console's OWN cartridge then holds a
    valid record and nobody wrote anyone else's."""
    n, frames = r.n, r.frames or 800
    relay = run.start_relay()
    specs = []
    for k in range(n):
        d, e = hub_env(run, k, frames, relay) if r.opts.get("hub") else adv_env(run, k, n, frames)
        off = 40 * k
        e.update({"SM64DS_LEVEL": "6", "SM64DS_SELFTEST_IDLE": "1",
                  "SM64DS_STAR_TRIGGER": str(200 + off),
                  "SM64DS_HOST_PAD": ",".join("1000@%d-%d" % (f, f + 6) for f in range(300, frames, 50)),
                  "SM64DS_TOUCH_PROBE": "%d-%d:128:56" % (412 + off, 418 + off),
                  "SM64DS_SAVE_TRACE": "1"})
        specs.append((k, e, d, 0.0 if k == 0 else 1.0 + 0.4 * k))
    rcs, texts, wall = run.launch(specs, timeout_for(n, frames, False, 60))
    cs = {k: parse(t) for k, t in texts.items()}
    ck = Checks()
    for k in range(n):
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        ck.ok(not cs[k]["faults"], "p%d faults=%d" % (k, len(cs[k]["faults"])))
        left = bool(re.search(r"\[lvl\] change: level 6 -> 1,", texts[k]))
        ck.ok(left, "p%d star exit back to the castle" % k)
        size, good = save_records(os.path.join(run.dir, "p%d" % k, "sm64ds.sav"))
        ck.ok(size == 0x2000 and good, "p%d own save %s bytes, valid slots %s" % (k, size, good))
    return ck, wall, rcs


def r_vssave(L, run, r):
    """VS: the host's cartridge holds a real save (written by the game's own
    SaveFile path in a one-console pre-run); after a match on it, the host's
    file is byte-identical and no joiner created or changed a cartridge."""
    frames = r.frames or 600
    host_sav = os.path.join(run.dir, "p0", "sm64ds.sav")
    d, e = run.base_env(0)
    pre = os.path.join(run.dir, "seed")
    os.makedirs(os.path.join(pre, "tmp"), exist_ok=True)
    e.pop("SM64DS_LEVEL", None)
    e.update({"SM64DS_SCENE": "1", "SM64DS_WINDOW_SELFTEST": "120", "SM64DS_SAVE_PROBE": "write",
              "SM64DS_SAVE_PROBE_FRAME": "60", "SM64DS_SAVE_PATH": host_sav,
              "TEMP": os.path.join(pre, "tmp"), "TMP": os.path.join(pre, "tmp")})
    rc = M.run_one(L.exe, pre, e, os.path.join(pre, "run.log"), timeout=240)
    size, good = save_records(host_sav)
    if rc != 0 or not good:
        return_ck = Checks()
        return_ck.ok(False, "seed save: rc=%s size=%s valid=%s" % (rc, size, good))
        return return_ck, 0.0, {"seed": rc}
    before = file_digest(host_sav)
    relay = run.start_relay()
    specs = []
    for k in range(r.n):
        d, e = vs_env(run, k, r.n, frames, relay, False, 0, match=True)
        specs.append((k, e, d, 0.0 if k == 0 else 1.0 + 0.3 * k))
    rcs, texts, wall = run.launch(specs, timeout_for(r.n, frames, False, 60))
    ck = Checks()
    for k in range(r.n):
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
    ck.ok(file_digest(host_sav) == before, "host save byte-identical after the match (%s -> %s)"
          % (before, file_digest(host_sav)))
    for k in range(1, r.n):
        sz, gd = save_records(os.path.join(run.dir, "p%d" % k, "sm64ds.sav"))
        ck.ok(not gd, "p%d wrote no save record (size %s, valid %s)" % (k, sz, gd))
    ck.note("seed save valid slots %s md5 %s" % (good, before))
    return ck, wall, rcs


BODIES = {"vs": r_vs, "advsave": r_advsave, "vssave": r_vssave, "match": r_match, "timeup": r_timeup, "leave": r_leave, "late": r_late, "kill": r_kill,
          "laggard": r_laggard, "digest": r_digest, "luigi": r_luigi, "luigiseeds": r_luigiseeds,
          "adv": r_adv, "hub": r_hub, "hubjoin": r_hubjoin, "hubpause": r_hubpause,
          "chat": r_chat, "chathub": lambda L, run, r: r_chat(L, run, r, hub=True),
          "voice": r_voice, "kit": r_kit}


# ---------------------------------------------------------------------------
# ARENA MODE RUNGS (run mp1, lane ARENAGATE1, plan phase A6)
#
# Every console of an arena rung is started with the environment
# LobbyLaunch.BuildChildEnv gives a real arena plan (arena_env: SM64DS_VS_MAP /
# _PLAYERS / _NETMODE / COMMS_RELAY / _CODE / _ROLE / _SLOT / _STAR_TARGET /
# _NAMES / SESSION_DIGEST / ADVENTURE=0, NO SM64DS_LEVEL, NO selftest, the game
# paced as a player's), against a local copy of REFEREE1's relay with the
# referee in it. Test knobs only shorten or script what a player would do
# (ARENA_TIME_S, ARENA_RIG, HOST_PAD, VS_EXIT_ON_END, HUB_DELAY_MS / _LOSS_PCT).
# Judges live in arena_logs.py (pure, selftested, each with a must-FAIL control
# this file runs against the real logs); this file launches and watches.
#
# THE ARENA RUNGS, how each is judged (the verdict line names the failed item):
#   arena2-mesh-loop / arena4-mesh-loop   A0 core on the test mesh (no referee):
#       every SYNS received == the sender's logged one, monotonic, only my slot
#       reads a pad, every seat heard.
#   arena{2,4}-lat-relay                  own body 0 frames on EVERY console
#       through the relay at skewed delay (HUB_DELAY_MS); lock2-lat-control:
#       the same probe on the LOCKSTEP carrier must read 3+ (the judge can go red).
#   arena{2,4}-{stars,time}-relay-*       a whole match through the referee at
#       skewed RTT: one session up, one START, ONE byte-identical referee MATCH
#       OVER and marker on every console (no two winners), the relay's own
#       referee log says the same, puppets driven (A1), rc 0, no fault.
#   arena{2,4}-cam-relay                  each console films its OWN slot.
#   arena4-leave-{kill,quit}-relay        a console leaves mid-match holding
#       stars: one LEAVE everywhere, spill == the ledger's stars, the match goes
#       on to one MATCH OVER, the leaver scores 0; arena4-rejoin-relay: it comes
#       back and every console sees JOIN mid-match.
#   arena{2,4}-loss{5,20}-relay           5 % / 20 % datagram loss on every console
#       (SM64DS_HUB_LOSS_PCT): the match still reaches one identical MATCH OVER.
#   arena-solo-alone                      one console alone in a 2-seat world: no
#       match starts, no crash, no 'connection lost'.
#   arena-solo-gate                       arena code inert without its env: the
#       solo identity / static / traps / [vs] probe rows of arenacore1's gate
#       against --base-exe (arena_solo_gate.py).
#   arena2-time-kit                       the same match from a kit-built exe in a
#       player-shaped install (--kit --rom).
#   arena2-e2e-60s / arena2-e2e-kit       REAL LAUNCHERS (MpE2E vsarena on a
#       private desktop, arena_e2e.py): a match PAST 60 s (A4's watchdog) and the
#       same with the kit exe. Need --launcher-repo.
# The lockstep fallback is the existing vs / match / timeup / leave rungs, untouched.
# ---------------------------------------------------------------------------
def close_windows(pid):
    """WM_CLOSE to every top-level window of pid (a player closing the game)."""
    import ctypes
    from ctypes import wintypes
    u = ctypes.windll.user32
    hits = []
    PROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    def cb(h, _):
        p = wintypes.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid:
            hits.append(h)
        return True
    u.EnumWindows(PROC(cb), 0)
    for h in hits:
        u.PostMessageW(h, 0x0010, 0, 0)
    return len(hits)


def console_text(d):
    """run.log (stdout) then every playlog/play_*.log (stderr, which the shipped
    path reopens onto the flight recorder), as one text."""
    t = M.text(os.path.join(d, "run.log")) if os.path.exists(os.path.join(d, "run.log")) else ""
    pl = os.path.join(d, "playlog")
    if os.path.isdir(pl):
        for fn in sorted(os.listdir(pl)):
            if fn.startswith("play_") and fn.endswith(".log"):
                t += "\n" + M.text(os.path.join(pl, fn))
    return t


DIRS_PAD = [0x0008, 0x0001, 0x0004, 0x0002]      # right up left down (XInput dpad)
BTN_A, BTN_B = 0x1000, 0x2000


def pad_script(k, start=600, span=1800):
    """SM64DS_HOST_PAD grammar <hex>@<f0>[-<f1>]: walk, jump, punch, run-jump."""
    ent, f, i = [], start, k
    while f < start + span:
        d = DIRS_PAD[i % 4]
        ent += ["%04x@%d-%d" % (d, f, f + 55), "%04x@%d" % (BTN_A, f + 60),
                "%04x@%d" % (BTN_B, f + 80), "%04x@%d-%d" % (d | BTN_A, f + 95, f + 125)]
        f += 150
        i += 1
    return ",".join(ent)


def lat_pad(start=1000, gap=75, count=8):
    """SM64DS_PROBE_INPUT: A presses spaced a landing apart, after the countdown (the latency
    probe reads the ROM's own key word, which SM64DS_HOST_PAD's XInput path did not reach)."""
    return ",".join("%d:A" % (start + gap * i) for i in range(count))


def arena_env(run, key, seat, n, relay, o, exe=None, asset_root=None):
    """The child environment of an arena plan (LobbyLaunch.BuildChildEnv) plus
    the rung's test knobs. key names the console's directory, seat its body."""
    d, e = run.base_env(key, asset_root)
    e["_EXE"] = exe or run.L.exe
    e.pop("SM64DS_LEVEL", None)                  # LobbyLaunch clears it: the real path
    e.pop("SM64DS_WINDOW_SELFTEST", None)
    target = o.get("target", 0)
    e["SM64DS_VS_MAP"] = str(o.get("map", 0))
    e["SM64DS_VS_NETMODE"] = "arena"
    e["SM64DS_COMMS_RELAY"] = relay
    e["SM64DS_COMMS_CODE"] = run.code
    e["SM64DS_COMMS_ROLE"] = "parent" if seat == 0 else "child"
    e["SM64DS_COMMS_PORT"] = str(run.port)
    e["SM64DS_VS_PLAYERS"] = str(n)
    e["SM64DS_ADVENTURE"] = "0"
    if seat:
        e["SM64DS_COMMS_SLOT"] = str(seat)
    if target:
        e["SM64DS_VS_STAR_TARGET"] = str(target)
    w = 4 if n <= 4 else 16
    e["SM64DS_VS_NAMES"] = ",".join([("P%d" % i) if i < n else "" for i in range(w)])
    e["SM64DS_SESSION_DIGEST"] = str(AL.lobby_digest(o.get("map", 0), n, target)[0])
    e["SM64DS_PLAYER_NAME"] = "lad%d" % seat
    # test knobs: end the process with the match, shorten / script what a player does
    e["SM64DS_VS_EXIT_ON_END"] = "1"
    e["SM64DS_VS_END_GRACE"] = "30"
    e["SM64DS_HUB_REPORT_S"] = "5"
    e["SM64DS_ARENA_REFLOG"] = "1"
    e["SM64DS_PUPPET_LOG"] = "1"
    if o.get("time_s"):
        e["SM64DS_ARENA_TIME_S"] = str(o["time_s"])
    if o.get("rig"):
        e["SM64DS_ARENA_RIG"] = "race:%d" % (int(hashlib.md5(run.rung.name.encode()).hexdigest()[:4], 16) % 997)
    if o.get("lat"):
        e["SM64DS_ARENA_LATENCY"] = "1"
        e["SM64DS_PROBE_INPUT"] = lat_pad()
    elif o.get("pad", True):
        e["SM64DS_HOST_PAD"] = pad_script(seat)
    delays = o.get("delays")
    if delays and delays[seat]:
        e["SM64DS_HUB_DELAY_MS"] = str(delays[seat])
    if o.get("loss"):
        e["SM64DS_HUB_LOSS_PCT"] = str(o["loss"])
    if o.get("cam"):
        e["SM64DS_CAM_WATCH"] = "1"
        e["SM64DS_VS_PROBE"] = "1"
    return d, e


def tasks_lower():
    try:
        return subprocess.run(["tasklist"], capture_output=True, text=True,
                              creationflags=M.NO_CONSOLE).stdout.lower()
    except OSError:
        return ""


class Fleet:
    """The consoles of one arena rung: started on a schedule, watched while they
    run (poll(fleet, t) may close, kill or start a console), killed at the
    deadline. W43 / W47: while League runs the consoles are IDLE priority on half
    the cores, while VRChat runs on a quarter."""

    def __init__(self, run):
        self.run = run
        self.procs, self.dirs, self.sched = {}, {}, []
        self.rcs, self.how = {}, {}
        self.t0 = None
        t = tasks_lower()
        self.vr, self.lg = "vrchat.exe" in t, "league of legends.exe" in t

    def add(self, key, env, d, delay):
        self.sched.append((delay, key, env, d))

    def start(self, key, env, d):
        with open(os.path.join(d, "env.txt"), "w") as f:
            for k in sorted(env):
                if k.startswith("SM64DS_"):
                    f.write("%s=%s\n" % (k, env[k]))
        p = M.spawn(os.path.abspath(env["_EXE"]), d, {x: v for x, v in env.items() if x != "_EXE"},
                    os.path.join(d, "run.log"))
        self.procs[key], self.dirs[key] = p, d
        if psutil is not None and (self.vr or self.lg):
            try:
                pp = psutil.Process(p.pid)
                pp.nice(psutil.IDLE_PRIORITY_CLASS)
                n = os.cpu_count() or 4
                pp.cpu_affinity(list(range(max(1, n // 4) if self.vr else max(1, n // 2))))
            except (psutil.Error, OSError, AttributeError):
                pass

    def alive(self, key):
        p = self.procs.get(key)
        return p is not None and p.poll() is None

    def text(self, key):
        return console_text(self.dirs[key]) if key in self.dirs else ""

    def close(self, key):
        self.how[key] = "closed"
        return close_windows(self.procs[key].pid)

    def kill(self, key):
        self.how[key] = "killed"
        try:
            self.procs[key].kill()
        except OSError:
            pass

    def run_until(self, timeout, poll=None):
        self.t0 = time.time()
        last_busy = 0.0
        while True:
            now = time.time() - self.t0
            for x in [x for x in self.sched if x[0] <= now]:
                self.sched.remove(x)
                self.start(x[1], x[2], x[3])
            if poll:
                poll(self, now)
            if not self.sched and not any(p.poll() is None for p in self.procs.values()):
                break
            if now >= timeout:
                for k, p in self.procs.items():
                    if p.poll() is None:
                        self.how.setdefault(k, "deadline")
                        p.kill()
                break
            if time.time() - last_busy > 2.0:
                last_busy = time.time()
                b = busy(self.run.L.busy_log)
                if b:
                    self.run.aborted = "busy flag: " + b.strip()
                    for p in self.procs.values():
                        try:
                            p.kill()
                        except OSError:
                            pass
                    break
            time.sleep(0.4)
        for k, p in self.procs.items():
            self.rcs[k] = M.finish(p, 20)
        if self.run.aborted:
            raise Aborted(self.run.aborted)
        return time.time() - self.t0

    def texts(self, keys=None):
        return {k: self.text(k) for k in (keys if keys is not None else self.procs)}


def stop_relay(run):
    """Stop the local relay and return its log text (the referee's own record)."""
    if run.relay and run.relay_stats is None:
        run.relay_stats = run.relay.stop()
    try:
        return M.text(run.relay.logpath) if run.relay else ""
    except OSError:
        return ""


def judge_into(ck, items):
    for okk, m in items:
        ck.ok(okk, m)


def control(ck, name, judge, mutated):
    """MUST-FAIL CONTROL: the judge, fed logs with one thing broken, must go red."""
    if mutated is None:
        ck.note("control %s: nothing to break in these logs" % name)
        return
    desc, texts = mutated
    red = [m for okk, m in judge(texts) if not okk]
    ck.ok(bool(red), "control (%s): the judge %s" % (
        desc, "went red as it must (%s)" % red[0][:70] if red else "STAYED GREEN"))


def match_controls(ck, texts, rcs, n, expect, rt, lane):
    judge = lambda t: AL.judge_match(t, rcs, list(range(n)), expect, rt=rt, lane=lane)
    for kind in ("two-winners", "second-marker", "deaf", "lost"):
        control(ck, kind, judge, AL.mutate(kind, texts))


def r_arena(L, run, r):
    """A whole arena match through the relay's referee: see the table above."""
    n, o = r.n, dict(r.opts)
    kit = o.get("kit")
    bundle = kit_bundle(L) if kit else None
    exe = os.path.join(bundle, "walk_window.exe") if kit else None
    relay = run.start_relay()
    fl = Fleet(run)
    for k in range(n):
        d, e = arena_env(run, k, k, n, relay, o, exe=exe, asset_root=bundle)
        fl.add(k, e, d, 0.0 if k == 0 else 0.4 * k)
    wall = fl.run_until(o.get("timeout", 420))
    rt = stop_relay(run)
    texts, rcs = fl.texts(), fl.rcs
    ck = Checks()
    target = o.get("target", 0)
    expect = {"reason": 2 if target else 1, "first_to": target}
    lane = {"A1": L.feats.have.get("A1")}
    judge_into(ck, AL.judge_match(texts, rcs, list(range(n)), expect, rt=rt, lane=lane))
    if not kit:
        relay_checks(ck, run)
    match_controls(ck, texts, rcs, n, expect, rt, lane)
    F = {k: AL.facts(texts[k]) for k in texts}
    ck.note("delays %s loss %s%% target %s time %ss" % (o.get("delays"), o.get("loss", 0), target or "-",
                                                        o.get("time_s", "ROM")))
    if F[0]["over_ref"]:
        ck.note("MATCH OVER ref " + " ".join(F[0]["over_ref"][0][:7]))
    ck.note("p0 saw %d stars taken, %d hits, %d spills" % (len(F[0]["taken"]), len(AL.HIT.findall(texts[0])),
                                                           len(F[0]["spill"])))
    if kit:
        ck.note("kit bundle exe md5 %s" % hashlib.md5(open(exe, "rb").read()).hexdigest())
    return ck, wall, rcs


def r_arenacam(L, run, r):
    """THE CAMERA ROW: every console films its OWN slot (a lockstep sweep never
    looks at the camera; the 09-30 child filmed slot 0). The consoles play for
    secs and are closed like a player quitting."""
    n, o = r.n, dict(r.opts)
    o["cam"] = True
    relay = run.start_relay()
    fl = Fleet(run)
    for k in range(n):
        d, e = arena_env(run, k, k, n, relay, o)
        e.pop("SM64DS_VS_EXIT_ON_END", None)
        fl.add(k, e, d, 0.0 if k == 0 else 0.4 * k)
    secs = o.get("secs", 70)

    def poll(f, t):
        if t >= secs:
            for k in list(f.procs):
                if f.alive(k) and k not in f.how:
                    f.close(k)
    wall = fl.run_until(secs + 60, poll)
    stop_relay(run)
    texts = fl.texts()
    rcs = {k: (0 if fl.how.get(k) == "closed" else v) for k, v in fl.rcs.items()}
    ck = Checks()
    am = bool(L.feats.have.get("A1"))
    judge_into(ck, AL.judge_camera(texts, rcs, others_move=am))
    control(ck, "camera", lambda t: AL.judge_camera(t, rcs, others_move=am), AL.mutate("camera", texts))
    relay_checks(ck, run)
    return ck, wall, rcs


def r_arenalat(L, run, r):
    """OWN-BODY LATENCY through the relay at skewed delay: the press-to-answer
    readout of every console reads 0 frames (a frame-count row, not a timing
    row). lockstep=True is the control: the same probe on the lockstep carrier
    through the same relay, which must read 3+ frames."""
    n, o = r.n, dict(r.opts)
    o["lat"] = True
    relay = run.start_relay()
    fl = Fleet(run)
    lockstep = o.get("lockstep")
    for k in range(n):
        d, e = arena_env(run, k, k, n, relay, o)
        e.pop("SM64DS_VS_EXIT_ON_END", None)
        if lockstep:
            for x in ("SM64DS_VS_NETMODE", "SM64DS_SESSION_DIGEST"):
                e.pop(x, None)
            e["SM64DS_NETMODE"] = "lockstep"
            e["SM64DS_COMMS_FANOUT"] = "1"
            e["SM64DS_COMMS_REPORT"] = "1"
            if o.get("delays") and o["delays"][k]:
                e.pop("SM64DS_HUB_DELAY_MS", None)
                e["SM64DS_COMMS_DELAY_MS"] = str(o["delays"][k])
        fl.add(k, e, d, 0.0 if k == 0 else 0.4 * k)
    secs = o.get("secs", 110)

    def poll(f, t):
        done = len(f.procs) == n and all(
            len(AL.LAT.findall(f.text(k))) + len(AL.NOANS.findall(f.text(k))) >= 8 for k in f.procs)
        if t >= secs or done:
            for k in list(f.procs):
                if f.alive(k) and k not in f.how:
                    f.close(k)
    wall = fl.run_until(secs + 90, poll)
    stop_relay(run)
    texts = fl.texts()
    rcs = {k: (0 if fl.how.get(k) == "closed" else v) for k, v in fl.rcs.items()}
    ck = Checks()
    want_zero = not lockstep
    judge_into(ck, AL.judge_latency(texts, rcs, want_zero=want_zero))
    if want_zero:
        control(ck, "late-body", lambda t: AL.judge_latency(t, rcs), AL.mutate("late-body", texts))
    relay_checks(ck, run)
    ck.note("delays %s" % (o.get("delays"),))
    return ck, wall, rcs


def r_arenamesh(L, run, r):
    """A0's core on the TEST MESH (SM64DS_HUB_PORT, no relay, no referee): every
    SYNS received equals the sender's logged one, own slot only reads a pad.
    Selftest world (the mesh has no lobby path)."""
    n = r.n
    base = 30000 + (PID % 1800) * 16
    if base <= SHIPPED_PORT < base + 16:
        base += 32
    fl = Fleet(run)
    frames = r.frames or 1200
    for k in range(n):
        d, e = run.base_env(k)
        e["_EXE"] = L.exe
        e.pop("SM64DS_LEVEL", None)
        e.update({"SM64DS_VS_MAP": "0", "SM64DS_VS_PROBE": "1", "SM64DS_PACE_SELFTEST": "1",
                  "SM64DS_VS_NETMODE": "arena", "SM64DS_VS_PLAYERS": str(n),
                  "SM64DS_WINDOW_SELFTEST": str(frames), "SM64DS_HUB_REPORT_S": "5",
                  "SM64DS_COMMS_ROLE": "parent" if k == 0 else "child",
                  "SM64DS_HUB_PORT": str(base), "SM64DS_ARENA_LOG": "1"})
        if k:
            e["SM64DS_COMMS_SLOT"] = str(k)
        d1 = ["RIGHT", "LEFT", "UP", "DOWN"][k % 4]
        d2 = ["UP", "DOWN", "LEFT", "RIGHT"][k % 4]
        e["SM64DS_PROBE_INPUT"] = ",".join(["150-230:%s" % d1, "240:A", "250-330:%s" % d2, "360:A",
                                            "370-450:%s+A" % d1, "480:A", "490-560:%s" % d2, "600:A"])
        fl.add(k, e, d, 0.4 * k)
    wall = fl.run_until(timeout_for(n, frames, True, 120))
    texts, rcs = fl.texts(), fl.rcs
    ck = Checks()
    judge_into(ck, AL.judge_wire(texts, rcs, n))
    control(ck, "wire", lambda t: AL.judge_wire(t, rcs, n), AL.mutate("wire", texts))
    ck.note("test mesh: no referee, no lobby env (the relay rungs cover the real entry path)")
    return ck, wall, rcs


def r_arenasolo(L, run, r):
    """ONE console alone in a 2-seat arena world (the HARD note's 'solo alone in
    a world'): the absent seat's body is held, no match starts (the referee needs
    two READY seats), nothing crashes, the referee is never reported lost."""
    relay = run.start_relay()
    o = dict(r.opts)
    fl = Fleet(run)
    d, e = arena_env(run, 0, 0, 2, relay, o)
    e.pop("SM64DS_VS_EXIT_ON_END", None)
    fl.add(0, e, d, 0.0)
    secs = o.get("secs", 55)

    def poll(f, t):
        if t >= secs and f.alive(0) and 0 not in f.how:
            f.close(0)
    wall = fl.run_until(secs + 60, poll)
    stop_relay(run)
    texts = fl.texts()
    rcs = {k: (0 if fl.how.get(k) == "closed" else v) for k, v in fl.rcs.items()}
    ck = Checks()
    judge_into(ck, AL.judge_solo_alone(texts[0], rcs[0]))
    control(ck, "match-appears", lambda t: AL.judge_solo_alone(t[0], rcs[0]),
            ("a MATCH OVER marker appears", {0: texts[0] + "\n[vs] MATCH OVER f9 win=time-up scores=0,0,0,0 "
             "players=2 total=0 pending_scene=0 results_screen=port winner=0\n"}))
    return ck, wall, rcs


def r_arenaleave(L, run, r):
    """A console leaves MID-MATCH holding stars (killed: a crash, or closed: a
    player quitting) and the match goes on; with rejoin=True it comes back."""
    n, o = r.n, dict(r.opts)
    leaver = n - 1
    relay = run.start_relay()
    fl = Fleet(run)
    o["rig"] = True
    for k in range(n):
        d, e = arena_env(run, k, k, n, relay, o)
        fl.add(k, e, d, 0.0 if k == 0 else 0.4 * k)
    state = {"left_at": None, "rejoined": False}

    def poll(f, t):
        if state["left_at"] is None:
            # leave once the LEAVER holds a star in the referee's ledger (so a leave spills something)
            t0 = f.text(0) if 0 in f.dirs else ""
            held = AL.ledger_before_leave(t0, leaver) if ("winner=%d " % leaver) in t0 else 0
            if f.alive(leaver) and held >= 1 and "[vsarena] START" in t0 and t > 20:
                (f.kill if o.get("how") == "kill" else f.close)(leaver)
                state["left_at"] = t
            elif t > o.get("giveup", 120) and f.alive(leaver):
                f.kill(leaver)                          # never scored: the rung says so
                state["left_at"] = t
                state["never"] = True
        elif o.get("rejoin") and not state["rejoined"] and not f.alive(leaver) and t > state["left_at"] + 5:
            t0 = f.text(0)
            if AL.LEAVE.search(t0) and "[vsarena] MATCH OVER ref" not in t0:
                d, e = arena_env(run, 10 + leaver, leaver, n, relay, o)
                f.start(10 + leaver, e, d)
                state["rejoined"] = True
    wall = fl.run_until(o.get("timeout", 300), poll)
    rt = stop_relay(run)
    texts, rcs = fl.texts(), fl.rcs
    stay = list(range(n - 1))
    ck = Checks()
    ck.ok(not state.get("never"), "the leaver scored a star before it left (a leave with nothing to spill "
                                  "proves nothing)")
    ck.ok(fl.how.get(leaver) in ("killed", "closed"), "the leaver went away on purpose (%s)" % fl.how.get(leaver))
    for k in stay:
        ck.ok(rcs.get(k) == 0, "p%d rc=%s" % (k, rcs.get(k)))
        ck.ok(not AL.FAULT.search(texts[k]), "p%d no fault" % k)
    rej = leaver if o.get("rejoin") else None
    judge_into(ck, AL.judge_leave(texts, rcs, stay, leaver, rt=rt, rejoined=rej))
    # the staying consoles still end on ONE verdict (the relay's referee ends only one match)
    verdict_items = AL.judge_match({k: texts[k] for k in stay}, rcs, stay, {"reason": None}, rt=None,
                                   lane={"A1": L.feats.have.get("A1")})
    judge_into(ck, [x for x in verdict_items if "byte-identical" in x[1] or "exactly one referee" in x[1]
                    or "never lost" in x[1]])
    if o.get("rejoin"):
        j = 10 + leaver
        ck.ok(j in texts and "[vsarena] installed" in texts.get(j, ""), "the returning console started and "
                                                                        "installed arena mode")
        ck.ok(not AL.FAULT.search(texts.get(j, "")), "the returning console had no fault")
    control(ck, "no-spill", lambda t: AL.judge_leave(t, rcs, stay, leaver, rt=rt),
            AL.mutate("no-spill", {k: texts[k] for k in stay}))
    relay_checks(ck, run)
    ck.note("leaver left at %.0fs (%s)" % (state["left_at"] or -1, fl.how.get(leaver)))
    return ck, wall, rcs


def r_arenasolo_gate(L, run, r):
    """The arena code is inert without its env: arena_solo_gate.py's rows on the
    exe under test against --base-exe (the frozen stage-5 build)."""
    import arena_solo_gate as G
    if not L.a.base_exe:
        raise Skip("needs --base-exe (the stage-5 / INT69B game build to compare against)")
    lines, okk = G.run_rows(L.a.base_exe, L.exe, L.asset_root, run.dir)
    ck = Checks()
    for ln in lines:
        ck.note(ln)
    ck.ok(okk, "arena_solo_gate: every row identical to the base (%d rows)" % len(lines))
    return ck, 0.0, {}


def r_arenae2e(L, run, r):
    """Real launchers (MpE2E vsarena), a match PAST 60 s so A4's launch watchdog
    is exercised; kit=True runs the kit build."""
    import arena_e2e as E
    if not L.a.launcher_repo:
        raise Skip("needs --launcher-repo (the private launcher checkout with tests/MpE2E built)")
    kit = r.opts.get("kit")
    if kit and not L.a.kit_exe:
        raise Skip("needs --kit-exe (the kit-built walk_window.exe) and --kit-assets for it")
    return E.run_rung(L, run, r, kit, Checks, Skip)


BODIES.update({"arena": r_arena, "arenacam": r_arenacam, "arenalat": r_arenalat,
               "arenamesh": r_arenamesh, "arenasolo": r_arenasolo, "arenaleave": r_arenaleave,
               "arenasolo_gate": r_arenasolo_gate, "arenae2e": r_arenae2e})


def arena_rungs():
    """The arena rung table (names are what the fold lane greps)."""
    R = []
    DEV, GATE = ("dev", "gate", "full"), ("gate", "full")
    A = ["A0", "A2", "A3"]
    skew2, skew4 = (20, 63), (20, 35, 55, 63)
    R.append(Rung("arena2-mesh-loop", "arenamesh", 2, DEV, needs=["A0"]))
    R.append(Rung("arena4-mesh-loop", "arenamesh", 4, DEV, needs=["A0"]))
    R.append(Rung("arena2-lat-relay-skew20-63", "arenalat", 2, DEV, "relay2", needs=["A0", "A2"], delays=skew2))
    R.append(Rung("arena4-lat-relay-skew", "arenalat", 4, GATE, "relay2", needs=["A0", "A2"], delays=skew4))
    R.append(Rung("lock2-lat-control-relay-skew20-63", "arenalat", 2, GATE, "relay2", needs=["A0", "R1"],
                  delays=skew2, lockstep=True))
    R.append(Rung("arena2-stars-relay-rtt0", "arena", 2, DEV, "relay2", needs=A, target=3, time_s=60, rig=True))
    R.append(Rung("arena2-stars-relay-skew20-63", "arena", 2, DEV, "relay2", needs=A, target=3, time_s=60,
                  rig=True, delays=skew2))
    R.append(Rung("arena2-time-relay-skew20-63", "arena", 2, DEV, "relay2", needs=A, time_s=30, rig=True,
                  delays=skew2))
    R.append(Rung("arena4-stars-relay-skew", "arena", 4, GATE, "relay2", needs=A, target=3, time_s=60, rig=True,
                  delays=skew4))
    R.append(Rung("arena4-time-relay-skew", "arena", 4, GATE, "relay2", needs=A, time_s=30, rig=True,
                  delays=skew4))
    R.append(Rung("arena2-loss5-relay", "arena", 2, GATE, "relay2", needs=A, time_s=30, rig=True, loss=5,
                  delays=skew2))
    R.append(Rung("arena4-loss20-relay", "arena", 4, GATE, "relay2", needs=A, time_s=30, rig=True, loss=20,
                  delays=skew4))
    R.append(Rung("arena2-cam-relay", "arenacam", 2, DEV, "relay2", needs=["A0", "A2"], delays=skew2))
    R.append(Rung("arena4-cam-relay", "arenacam", 4, GATE, "relay2", needs=["A0", "A2"], delays=skew4))
    R.append(Rung("arena4-leave-kill-relay", "arenaleave", 4, GATE, "relay2", needs=A, time_s=75, how="kill",
                  delays=skew4))
    R.append(Rung("arena4-leave-quit-relay", "arenaleave", 4, GATE, "relay2", needs=A, time_s=75, how="close",
                  delays=skew4))
    R.append(Rung("arena4-rejoin-relay", "arenaleave", 4, GATE, "relay2", needs=A, time_s=90, how="kill",
                  rejoin=True, delays=skew4))
    R.append(Rung("arena-solo-alone", "arenasolo", 2, DEV, "relay2", needs=["A0", "A2"]))
    R.append(Rung("arena-solo-gate", "arenasolo_gate", 1, GATE, needs=["A0"]))
    R.append(Rung("arena2-time-kit", "arena", 2, GATE, "relay2", needs=A + ["kit"], time_s=30, rig=True,
                  delays=skew2, kit=True))
    R.append(Rung("arena2-e2e-60s", "arenae2e", 2, GATE, needs=A, secs=75))
    R.append(Rung("arena2-e2e-kit", "arenae2e", 2, GATE, needs=A, secs=75, kit=True))
    return R


# ---------------------------------------------------------------------------
# THE LADDER
# ---------------------------------------------------------------------------
def release_lock(path):
    """build_lock.release, made sure. Its os.remove swallows OSError, and on
    Windows a delete fails while another waiter has the lockfile open to read
    the holder; with several lanes queued that is common, and a silent failed
    release left this ladder waiting on its own lock (and every lane behind
    it). Retry until the file no longer names this process; refuse to go on
    if it still does."""
    end = time.time() + 30
    while True:
        build_lock.release(path)
        pid = build_lock.holder(path)[0]
        if pid != os.getpid():
            return
        if time.time() >= end:
            raise Stop("could not release the ladder lock %s" % path)
        time.sleep(0.1)


def asset_root_for(a):
    """The consoles read build/assets/ (the NitroFS index, romdata) from their
    asset root, and it must be the one the exe was built against."""
    if a.asset_root:
        return os.path.abspath(a.asset_root)
    exe_dir = os.path.dirname(os.path.abspath(a.exe))
    tree = os.path.dirname(os.path.dirname(exe_dir))
    if os.path.basename(exe_dir) == "port" and os.path.exists(
            os.path.join(tree, "build", "assets", "nitrofs.tsv")):
        return tree
    return ROOT


class Ladder:
    def __init__(self, a):
        self.a = a
        self.exe = os.path.abspath(a.exe)
        self.asset_root = asset_root_for(a)
        self.relay_py = os.path.abspath(a.relay_py)
        self.relay1_py = os.path.abspath(a.relay1_py or a.relay_py)
        self.busy_log = os.path.expanduser(a.busy_log) if a.busy_log else ""
        self.minpos = a.minpos
        self.kit, self.rom = a.kit, a.rom
        stamp = time.strftime("%Y%m%d-%H%M%S", time.gmtime())
        self.out = os.path.abspath(a.out or os.path.join(ROOT, "build", "mp_ladder", stamp))
        self.kit_work = os.path.abspath(a.kit_work or os.path.join(os.path.dirname(self.out), "kit"))
        self.shared_save = os.path.join(os.path.dirname(self.exe), "save", "sm64ds.sav")
        self.rows = []
        self.lock, self.lock_t = None, 0.0

    def take_lock(self, r):
        """The busy flag is read before queueing AND again once the lock is
        held: a match can start while this ladder waits in line, and a rung
        must not launch into it. Busy after the lock: release, wait, re-queue.
        With --hold-s, a lock taken for one rung is kept for the next ones
        until it has been held that long (a batch), then goes back in line."""
        os.environ["SM64DS_BUILD_LOCK_PATH"] = self.a.lock
        if self.lock:
            fresh = time.time() - self.lock_t < self.a.hold_s
            if fresh and not busy(self.busy_log) and build_lock.holder(self.lock)[0] == os.getpid():
                return self.lock
            self.drop_lock()
        end = time.time() + self.a.busy_wait
        while True:
            wait_not_busy(self.busy_log, max(0, end - time.time()))
            lock = build_lock.acquire(label="mp_ladder-%s" % r.name, root=ROOT,
                                      timeout=self.a.lock_timeout)
            if not busy(self.busy_log):
                self.lock, self.lock_t = lock, time.time()
                return lock
            release_lock(lock)

    def drop_lock(self, force=True):
        """Release unless a batch (--hold-s) is still running."""
        if not self.lock:
            return
        if force or time.time() - self.lock_t >= self.a.hold_s or busy(self.busy_log):
            release_lock(self.lock)
            self.lock = None

    def run_rung(self, r, idx, feats, rerun=False):
        gone = [x for x in r.opts.get("retired_by", ()) if feats.have.get(x)]
        if gone:
            return "SKIP", 0.0, "retired: %s is in this exe (adventure over the VS carrier is gone; "                 "the hub* rungs cover it)" % ", ".join(LANE_NAMES.get(x, x) for x in gone), {}
        miss = feats.missing([x for x in r.needs if x != "kit"])
        if "kit" in r.needs and not (self.kit and self.rom):
            return "SKIP", 0.0, "needs --kit and --rom (a player-shaped install)", {}
        if miss:
            return "SKIP", 0.0, "needs " + ", ".join("%s (%s)" % (m, LANE_NAMES.get(m, m))
                                                     for m in miss), {}
        ok, why = mem_ok(r.n)
        if not ok:
            raise Stop("memory: " + why)
        self.take_lock(r)
        shared_before = file_digest(self.shared_save)
        failed, run = True, None
        try:
            run = Run(self, r, idx, os.path.join(self.out, r.name + ("-rerun" if rerun else "")))
            try:
                ck, wall, rcs = BODIES[r.kind](self, run, r)
                failed = False
            except Skip as s:
                failed = False
                return "SKIP", 0.0, str(s), {}
            except Aborted as s:
                return "ABORTED", 0.0, str(s), {}
            finally:
                if run.relay and run.relay_stats is None:
                    run.relay_stats = run.relay.stop()
        finally:
            self.drop_lock(force=failed or run is None or bool(run.aborted))
        shared_after = file_digest(self.shared_save)
        ck.ok(shared_before == shared_after, "the shared default save beside the exe untouched "
              "(%s -> %s)" % (shared_before, shared_after))
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

    def record_notrun(self, r, feats, why):
        """After a stop, a rung whose lane is absent is still a SKIP, not a NOTRUN."""
        miss = feats.missing([x for x in r.needs if x != "kit"])
        if "kit" in r.needs and not (self.kit and self.rom):
            miss.append("kit")
        if miss:
            self.record(r, "SKIP", 0.0, "needs " + ", ".join(miss), {})
        else:
            self.record(r, "NOTRUN", 0.0, why, {})

    def record(self, r, verdict, wall, detail, rcs, note=""):
        row = (r.name, verdict, "%.0f" % wall, r.desc(), note + detail)
        self.rows.append(row)
        print("%-8s %-34s %5ss  %s" % (verdict, r.name, row[2], (note + detail)[:400]), flush=True)
        with open(os.path.join(self.out, "summary.tsv"), "a", encoding="utf-8") as f:
            f.write("\t".join(row) + "\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", default=DEFAULT_EXE)
    ap.add_argument("--asset-root", help="SM64DS_ASSET_ROOT for the consoles (default: the tree the "
                    "exe was built in when it sits at <tree>/build/port/, else this repo)")
    ap.add_argument("--relay-py", default=RELAY_PY, help="relay source for relay2 rungs (240 pps)")
    ap.add_argument("--relay1-py", help="relay source for relay1 rungs (120 pps); default --relay-py."
                    " Point it at a copy of the deployed v1 relay to prove the v1 fallback.")
    ap.add_argument("--lock", default=os.environ.get("SM64DS_LADDER_LOCK"),
                    help="the machine-wide ladder lockfile (or $SM64DS_LADDER_LOCK)")
    ap.add_argument("--lock-timeout", type=int, default=1200)
    ap.add_argument("--hold-s", type=int, default=0,
                    help="keep the lock across consecutive rungs for up to this many seconds (a batch), "
                    "then queue again; 0 = release after every rung")
    ap.add_argument("--busy-log", default=os.environ.get("SM64DS_BUSY_LOG", ""),
                    help="busy-flag file (default $SM64DS_BUSY_LOG; unset = no busy flag)")
    ap.add_argument("--busy-wait", type=int, default=0, help="seconds to wait for the busy flag")
    ap.add_argument("--tier", default="gate", choices=("dev", "gate", "full"))
    ap.add_argument("--only", help="regex over rung names (applied after --tier)")
    ap.add_argument("--skip", help="regex over rung names to leave out")
    ap.add_argument("--assume", default="", help="comma list of lanes to treat as landed")
    ap.add_argument("--minpos", type=int, default=100, help="distinct positions per slot")
    ap.add_argument("--sizes", default="", help="comma list of extra console counts (2..16) to run "
                    "as vs<N>-loop-rtt0-u rungs in any tier, e.g. to bracket a size that fails")
    ap.add_argument("--kit", help="a fresh kit directory (the release zip unpacked, never run)")
    ap.add_argument("--rom", help="the ROM the kit extractor reads")
    ap.add_argument("--kit-work", help="where the extracted player install is cached")
    ap.add_argument("--base-exe", help="the frozen stage-5 game exe the arena solo gate compares against")
    ap.add_argument("--launcher-repo", default=os.environ.get("SM64DS_LAUNCHER_REPO", ""),
                    help="the private launcher checkout (tests/MpE2E built) for the arena e2e rungs")
    ap.add_argument("--kit-exe", help="the kit-built (PORT_ROM_CLEAN) walk_window.exe for arena2-e2e-kit")
    ap.add_argument("--e2e-assets", help="build/assets dir for the e2e rungs (the kit exe needs the kit "
                    "build's own romdata pair); default: <asset root>/build/assets")
    ap.add_argument("--kit-assets", help="build/assets dir holding the KIT build's own romdata pair "
                    "(arena2-e2e-kit; the dev assets crash a kit exe)")
    ap.add_argument("--e2e-extracted", default="", help="extracted/ for the e2e peers' bundles")
    ap.add_argument("--e2e-rom", default="", help="the .nds for the e2e peers' bundles (default --rom)")
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
    if not a.busy_log:
        print("WARNING: NO BUSY FLAG: --busy-log and $SM64DS_BUSY_LOG are both unset, so nothing "
              "pauses this ladder for other work on the machine")
    aroot = asset_root_for(a)
    print("asset root %s" % aroot)
    for shadow in (os.path.join(os.path.dirname(os.path.abspath(a.exe)), "settings.json"),
                   os.path.join(aroot, "settings.json")):
        if os.path.exists(shadow):
            print("  WARNING: %s is read before each console's own settings.json" % shadow)
    if not os.path.exists(os.path.join(aroot, "build", "assets", "nitrofs.tsv")):
        print("the asset root has no build/assets/nitrofs.tsv: pass --asset-root <the tree the exe "
              "was built in>")
        return 2
    if a.probe:
        return 0
    rungs = [r for r in build_rungs() if a.tier in r.tiers]
    have = {r.name for r in rungs}
    for s in filter(None, (x.strip() for x in a.sizes.split(","))):
        n = max(2, min(16, int(s)))
        if "vs%d-loop-rtt0-u" % n not in have:
            rungs.append(Rung("vs%d-loop-rtt0-u" % n, "vs", n, ("dev", "gate", "full")))
            have.add(rungs[-1].name)
    if a.only:
        rungs = [r for r in rungs if re.search(a.only, r.name)]
    if a.skip:
        rungs = [r for r in rungs if not re.search(a.skip, r.name)]
    if a.list:
        for r in rungs:
            miss = feats.missing([x for x in r.needs if x != "kit"])
            if "kit" in r.needs and not (a.kit and a.rom):
                miss.append("kit")
            if [x for x in r.opts.get("retired_by", ()) if feats.have.get(x)]:
                miss = ["retired"]
            print("%-34s %-40s %s" % (r.name, r.desc(), ("needs " + ",".join(miss)) if miss else "runs"))
        print("%d rungs in tier %s" % (len(rungs), a.tier))
        return 0
    if not a.lock:
        print("refusing to launch without --lock (or $SM64DS_LADDER_LOCK): one multi-console run "
              "at a time machine-wide")
        return 2
    L = Ladder(a)
    L.feats = feats
    os.makedirs(L.out, exist_ok=True)
    with open(os.path.join(L.out, "summary.tsv"), "w", encoding="utf-8") as f:
        f.write("rung\tverdict\twall_s\tshape\tdetail\n")
    print("out %s, %d rungs, tier %s, ports from pid %d" % (L.out, len(rungs), a.tier, PID), flush=True)
    rerun = []
    stopped = ""
    idx, aborts = 0, 0
    while idx < len(rungs):
        r = rungs[idx]
        try:
            v, wall, detail, rcs = L.run_rung(r, idx + 100 * aborts, feats)
        except (Stop, build_lock.BuildLockTimeout) as s:
            stopped = str(s)
            print("STOPPED at %s: %s" % (r.name, stopped), flush=True)
            for rest in rungs[idx:]:
                L.record_notrun(rest, feats, stopped)
            break
        L.record(r, v, wall, detail, rcs)
        if v == "ABORTED" and aborts < 3:
            # the busy flag came up mid-rung: the rung is run again once it
            # clears (run_rung waits for that, bounded by --busy-wait)
            aborts += 1
            continue
        if v == "TIMEOUT":
            rerun.append((idx, r))
        idx, aborts = idx + 1, 0
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
    L.drop_lock()
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
