#!/usr/bin/env python3
"""SM64DS port UDP relay.

Two players behind ordinary home routers cannot reach each other directly.
Both of them dial OUT to this relay instead, which needs no port forwarding
on either side. The relay pairs them by session code and forwards their
packets. It never looks inside a forwarded packet.

Wire contract (frozen, shared with the game side):

  HELLO      16 bytes  "SMRC" | version u8 = 1 | role u8 (0 parent, 1 child)
                       | reserved u16 | code 8B ASCII, right padded with NUL
  HELLO-ACK  16 bytes  "SMRA" | version u8 = 1 | status u8 (0 ok, 1 full,
                       2 bad request) | reserved u16 | code echoed 8B

Clients repeat HELLO about once a second until they get an ACK. That doubles
as the NAT keepalive. A repeat HELLO from an endpoint we already know is
idempotent: it is re-ACKed and the pairing is left alone.

A session is one code with one parent and up to three children. Once paired,
every datagram that is not a HELLO is forwarded verbatim: parent to all
children, child to the parent only. Payloads are opaque here, always.

An address we have never paired gets no benefit of the doubt. The only thing
it has any business sending is a HELLO, so a 16 byte datagram from it is read
as one and anything else is dropped without a word. A reply is therefore never
larger than what arrived, which is what stops this being useful as an
amplifier. The flip side, and the game side has to respect it: a forwarded
packet must not be exactly 16 bytes starting with "SMRC".

VERSION 2 (run mp1, lane RELAY2; NETARCH1 design section 3). Everything above
is version 1 and a version 1 HELLO is still served exactly as it always was.
A version 2 HELLO is the same 16 bytes with the reserved field put to work:

  HELLO v2   "SMRC" | version u8 = 2 | role u8 (0 parent, 1 child, 2 member)
             | want_seat u8 | flags u8 | code 8B
  ACK v2     "SMRA" | version u8 = 2 | status u8 | seat u8 | 0 | code 8B
             status 0 ok, 1 full, 2 bad request, 3 version mismatch,
             4 seat taken, 5 wrong session kind

A code is a version 1 or a version 2 session by its first HELLO and never
mixes. A version 2 session is a STAR (parent + children, the VS lockstep
shape) or a HUB (members only, no host: adventure code rooms and world rooms),
again by its first HELLO. Every seat has a number, and in a version 2 session
the relay is no longer blind: it classifies each datagram by its first four
bytes and its exact length against the kind table below (KINDS), drops
anything else, routes it by the table, and OVERWRITES the sender's seat into
the datagram (offset 6 for 'MP2L', offset 5 for every 'SYN*' kind), so no
client can speak as another seat. That is the one byte the relay writes;
everything else still goes through untouched and is never logged.

ARENA SESSIONS (run mp1, lane REFEREE1; VSSYNC1 plan section 4). A version 2
HELLO with role 3 opens or joins an ARENA session: hub-shaped (seats 0..15,
nobody hosts) with a REFEREE inside the relay (referee.py) that owns every
outcome two players can disagree about -- stars, hits, the score, the clock
and the winner. An arena session has its own kind table (ARENA_KINDS): state
and object frames are forwarded to every other seat AND read by the referee,
claims go to the referee only and are never forwarded, the referee's own
event frames ('SYNR') and clock pongs come from the relay alone (a seat that
sends one is dropped and struck), and a state frame that fails the referee's
range checks is not forwarded at all. Star and hub sessions never see any of
this: their path through the relay is unchanged. The wire is
arena_contract.py's.

Seats in an arena session (run mp1, lane REJOIN1): a HELLO naming a seat
(want_seat 0..15) gets it when it is free. When it is held by a console that
has sent nothing (no datagram, no HELLO) for SEAT_RECLAIM_S, the returning
console takes it at once: a console that crashed and restarted comes back on a
new source port well inside the 10 s idle expiry. The old holder is dropped as
a leave first, so the referee's leave / spill / join rules run in that order.
A seat whose console was heard inside SEAT_RECLAIM_S is never given away, so a
live seat cannot be taken by anyone who only knows the room code. Once the
first READY has fixed the match's body count, a seat number or an any-seat
HELLO that would land past it is refused with status 1 (full) and the log says
why; before that the room is as open as a hub.

Configuration (environment):

  SM64DS_RELAY_PORT     UDP port to bind          (default 41234)
  SM64DS_RELAY_BIND     address to bind           (default 0.0.0.0)
  SM64DS_RELAY_IDLE_S   per endpoint idle expiry  (default 90)
  SM64DS_RELAY_STATS_S  stats line interval       (default 60)
  SM64DS_RELAY_MAX_CHILDREN
                        children one session holds (default 3, so four people)
  SM64DS_RELAY_RATE_PPS sustained packets per second, per source address
                        (default 120, unchanged). SET 240 FOR VOICE SESSIONS:
                        a seat running proximity voice chat sends ~117 a
                        second and 120 leaves it no headroom at all. See the
                        budget breakdown at RATE_PPS below.
  SM64DS_RELAY_RATE_BURST
                        token bucket depth (default: the same as RATE_PPS)
  SM64DS_RELAY_RATE_PPS_V2
                        the same budget for an endpoint in a version 2
                        session (default 240); the per-kind caps in KINDS
                        apply inside it
  SM64DS_RELAY_HUB_IDLE_S
                        idle expiry of a hub member (default 10; members send
                        a HELLO every second as their keepalive); arena
                        members use the same window
  SM64DS_REFEREE_KICK   1 = the referee kicks a seat at 3 strikes in 10 s
                        (default off: phase A logs strikes only)
  SM64DS_REFEREE_SEED   tests only: a fixed match seed and clock base

Python 3 standard library only. No dependencies, no state on disk.
"""

import asyncio
import importlib.util
import os
import socket
import sys
import time


def clamp_int(raw, default, lo, hi):
    """An integer knob, clamped. A junk value is the default, never a crash --
    the same rule the lobby service uses, so the two read alike."""
    try:
        return max(lo, min(hi, int(raw)))
    except (TypeError, ValueError):
        return default


# ---------------------------------------------------------------- protocol

MAGIC_HELLO = b"SMRC"
MAGIC_ACK = b"SMRA"
PROTO_VERSION = 1
HELLO_LEN = 16

ROLE_PARENT = 0
ROLE_CHILD = 1

STATUS_OK = 0
STATUS_FULL = 1
STATUS_BAD = 2

# ---- version 2 (design section 3) -----------------------------------------
PROTO_V2 = 2
ROLE_MEMBER = 2             # a hub member; only version 2 knows this role
WANT_ANY = 0xFF             # want_seat: "any free seat"
STATUS_VERSION = 3          # this code is a session of the other version
STATUS_SEAT_TAKEN = 4       # the named seat is held by another endpoint
STATUS_WRONG_KIND = 5       # a star role into a hub code, or the reverse

KIND_STAR = "star"
KIND_HUB = "hub"
HUB_SEATS = 16              # a hub session holds seats 0..15, nobody hosts
ROLE_ARENA = 3              # an arena member (hub-shaped, refereed)
KIND_ARENA = "arena"

# ---- THE KIND TABLE (design section 3) --------------------------------------
#
# In a version 2 session this table is the whole of what the relay forwards.
# A datagram is classified by its first four bytes AND its exact length; a tag
# not in the table, a length the table does not list for that tag, a kind the
# session's shape does not route, or a sender over that kind's cap is DROPPED
# and counted under its reason. Nothing else is ever forwarded.
#
# Routes:
#   STAR  parent -> every child, child -> the parent (the VS lockstep star)
#   ALL   every other endpoint of the session
#   SEAT6 only the seat named at byte 6 (a pong goes back to its pinger)
#   UP    child -> the parent; the parent's own is dropped
#   None  the kind does not exist in that session shape: DROP
#
# STAMP is the byte the relay overwrites with the sender's seat. Caps are
# packets per second per sender, bucket depth one second, inside the
# per-source budget RATE_PPS_V2.
R_STAR = "star"
R_ALL = "all"
R_SEAT6 = "seat6"
R_UP = "up"


class Kind(object):
    __slots__ = ("tag", "name", "lo", "hi", "exact", "star", "hub", "cap",
                 "stamp")

    def __init__(self, tag, name, lengths, star, hub, cap, stamp):
        self.tag = tag
        self.name = name
        if isinstance(lengths, range):
            self.lo, self.hi, self.exact = lengths.start, lengths.stop - 1, None
        else:
            self.lo, self.hi = min(lengths), max(lengths)
            self.exact = frozenset(lengths)
        self.star = star
        self.hub = hub
        self.cap = cap
        self.stamp = stamp

    def length_ok(self, n):
        if self.exact is not None:
            return n in self.exact
        return self.lo <= n <= self.hi


KINDS = {}
for _k in (
        #     tag      name        lengths            star     hub     cap stamp
        Kind(b"MP2L", "lockstep", (0x90, 0x214),     R_STAR,  None,   75, 6),
        Kind(b"SYNG", "ghost",    (44,),             None,    R_ALL,  35, 5),
        Kind(b"SYNI", "ident",    range(24, 65),     R_ALL,   R_ALL,   3, 5),
        Kind(b"SYNT", "chat",     range(12, 133),    R_ALL,   R_ALL,   8, 5),
        Kind(b"SYNV", "voice",    (176, 340),        R_ALL,   R_ALL,  30, 5),
        Kind(b"SYNP", "ping",     (12,),             R_ALL,   R_ALL,   6, 5),
        Kind(b"SYNQ", "pong",     (12,),             R_SEAT6, R_SEAT6, 6, 5),
        Kind(b"SYNK", "clock",    (24,),             R_UP,    None,    4, 5),
        Kind(b"SYN1", "sync4",    (56,),             R_STAR,  None,   35, 5),
):
    KINDS[_k.tag] = _k
del _k
KIND_ORDER = tuple(KINDS)   # stats line order, the table's own

# The reasons a version 2 datagram is dropped, in stats line order.
DROP_REASONS = ("unknown", "length", "route", "cap", "target", "session_cap")

# ---- THE ARENA KIND TABLE (arena_contract.py ARENA_KINDS) -------------------
#
# An arena session forwards exactly this; star and hub sessions never read it.
# Routes:
#   A_ALL     every other seat (as a hub)
#   A_ALLREF  every other seat AND the referee reads it; a frame that fails
#             the referee's range checks is dropped, not forwarded
#   A_REF     the referee only, never forwarded
#   A_PING    12 bytes: a peer ping to everyone (as a hub); 20 bytes with
#             byte 6 = 0xFE: the referee's clock ping, answered by the relay
#   A_PONG    12 bytes: to the seat at byte 6 (as a hub); 20: referee-only
#   A_NONE    referee-originated only; a seat sending it is struck
A_ALL = "all"
A_ALLREF = "all+ref"
A_REF = "ref"
A_PING = "ping"
A_PONG = "pong"
A_NONE = "none"
ARENA_KINDS = {}
for _tag, _lengths, _route, _cap in (
        (b"SYNS", (48,), A_ALLREF, 35),
        (b"SYNO", (32,), A_ALLREF, 35),
        (b"SYNE", (24,), A_REF, 20),
        (b"SYNR", (272,), A_NONE, 0),
        (b"SYNP", (12, 20), A_PING, 6),
        (b"SYNQ", (12, 20), A_PONG, 6),
        (b"SYNI", tuple(range(24, 65)), A_ALL, 3),
        (b"SYNT", tuple(range(12, 133)), A_ALL, 8),
        (b"SYNV", (176, 340), A_ALL, 30),
):
    ARENA_KINDS[_tag] = (_route, Kind(_tag, "arena", _lengths, None, None,
                                      _cap, 5))
del _tag, _lengths, _route, _cap
ARENA_ORDER = tuple(ARENA_KINDS)
ARENA_DROPS = ("unknown", "length", "route", "field", "cap", "target",
               "session_cap")
REFEREE_TICK_S = 0.01       # verdict / clock / heartbeat granularity
REFEREE_KICK = os.environ.get("SM64DS_REFEREE_KICK") == "1"
REFEREE_SEED = (clamp_int(os.environ.get("SM64DS_REFEREE_SEED"), None, 0,
                          0xFFFFFFFF)
                if os.environ.get("SM64DS_REFEREE_SEED") else None)


def _load_referee():
    """referee.py from this file's own directory (the deploy copies both).
    None when it is missing: arena HELLOs are then refused as bad requests,
    the answer an older relay gives, and nothing else changes."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "referee.py")
    try:
        spec = importlib.util.spec_from_file_location("sm64ds_referee", path)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod
    except Exception as exc:     # missing or broken: no arena mode
        print("referee.py not loaded (%s); arena sessions disabled" % exc,
              file=sys.stderr, flush=True)
        return None


referee = _load_referee()

# ------------------------------------------------------------------ limits

MAX_PAYLOAD = 700           # bytes; larger datagrams are dropped in silence

# ---- THE PACKET RATE, AND WHY IT IS A KNOB NOW (lane VOICE) --------------
#
# THE DEFAULT IS UNCHANGED. A relay nobody reconfigures rate-limits exactly as
# it did: 120 packets a second sustained per source address, burst 120.
#
# WHAT MADE IT TIGHT. The budget is per SOURCE ADDRESS and it counts everything
# one endpoint sends, so a seat's traffic is the sum of every kind on its one
# socket:
#
#     lockstep input record   up to 60/s   (one per frame, plus republishes
#                                           at kPublishResendMs while a round
#                                           is open)
#     sync snapshots          30/s         (SM64DS_SYNC_HZ, default 30)
#     RTT probes               2/s         ('SYNP' at ~2 Hz)
#     ------------------------------------
#     before voice           ~92/s
#     voice                   25/s         (two 20 ms frames per datagram)
#     ------------------------------------
#     with voice            ~117/s   against a budget of 120
#
# Three packets of headroom is not headroom. A republish burst, a frame the
# loop ran long on, or a peer that talks while the sync layer is catching up
# puts a seat over the line, and what the relay does then is DROP -- silently,
# by design, because a rate limiter that explained itself would be a
# reflection amplifier. So a voice session on a stock relay would lose
# lockstep records under load and look like a desync.
#
# BATCHING IS ALREADY DOING ITS HALF. Voice packs two 20 ms frames into one
# datagram, which is 25 packets a second where one frame per datagram would be
# 50. Without that, a voice seat is ~142/s and no plausible budget is enough.
#
# THE RECOMMENDATION, and the operator sets it: SM64DS_RELAY_RATE_PPS=240 for a
# relay carrying voice sessions. That is double the old budget for a seat whose
# steady state is ~117, which leaves the same proportional headroom the old
# number left a seat at ~92. The burst follows the sustained rate unless it is
# set on its own, because a bucket shallower than one second of traffic turns
# an ordinary republish burst into a drop.
RATE_PPS = clamp_int(os.environ.get("SM64DS_RELAY_RATE_PPS"), 120, 30, 2000)
RATE_BURST = clamp_int(os.environ.get("SM64DS_RELAY_RATE_BURST"),
                       RATE_PPS, 30, 4000)
# Version 2 sessions get their own budget (design section 3: 240 for v2, v1
# keeps its 120). The sum of the per-kind caps a real seat can reach is ~150
# (30 lockstep + 25 voice + pings + ghosts in a hub), so 240 is headroom, and
# the per-kind caps in KINDS are what stops one kind eating the rest.
RATE_PPS_V2 = clamp_int(os.environ.get("SM64DS_RELAY_RATE_PPS_V2"),
                        240, 30, 2000)
RATE_BURST_V2 = RATE_PPS_V2

# ---- THE PER-SESSION FORWARD BUDGET (run mp1, REVRELAY2 finding H2) --------
#
# Every limit above is per SENDER. A hub fans each datagram out to up to 15
# others, so sixteen ordinary sockets each sending at every per-kind cap drive
# one hub to ~19800 forwards a second (82 fanned-out datagrams/s x 15 + pongs,
# x 16 seats), about 5.5x the worst v1 session. This caps what ONE version 2
# session can make the relay send, counted in copies sent, one second deep.
#
# SIZED FROM THE WORST LEGITIMATE ROOM (design sections 3 and 6), a full
# 16-seat hub with EVERY seat talking at once:
#     per seat in:  ghost 15 + ping 2 + identity 0.5 + chat 2 (1 message per
#                   1.5 s, sent 3 times) + voice 25          = 44.5 /s
#     fanned to 15 others: 44.5 x 15 x 16 seats              = 10680 /s
#     pongs: every seat answers 15 pingers at 2 Hz, 1 copy    =   480 /s
#     total                                                   ~ 11160 /s
# (a normal room with 4 talkers is ~5600/s, measured). 15000 leaves a third
# over that absolute worst case and cuts a hostile hub from ~19800 to 15000.
# A 16-player VS star is far below it (~900/s of lockstep plus its aux).
# Version 1 sessions are not budgeted here: they stay exactly as they were.
# What this cannot do is stop MANY hostile sessions; that is the per-address
# work in the handshake proposal (findings H1/H8).
SESSION_FWD_PPS = clamp_int(os.environ.get("SM64DS_RELAY_SESSION_FWD_PPS"),
                            15000, 100, 1000000)
SESSION_FWD_BURST = SESSION_FWD_PPS
# A sender's per-kind allowances are kept by source ADDRESS (finding H3: a
# re-HELLO into another seat used to hand out fresh ones). They are forgotten
# this long after the address last spent one and holds no seat; any value
# over one second (the depth of every kind bucket) is exact.
KIND_CAPS_IDLE_S = 5.0
# THE STAR HOST'S LOCKSTEP CAP (run mp1, REVVSAUX1 S6). The kind table caps
# 'MP2L' at 75 a second per sender, which fits every child (one block a round
# plus republishes). The host of a VS star is different: it sends every
# aggregate AND republishes it while a round is open, and in a paced 16-player
# match through the relay it hit 75 and lost up to ~13 percent of its lockstep
# datagrams, which the 15 joiners then waited for (+19 to +64 percent starve
# waits). Seat 0 of a star gets this cap instead; a relay copy at 150 dropped
# nothing. It stays inside the per-source budget (RATE_PPS_V2) and the
# session budget (SESSION_FWD_PPS); every other seat and kind is unchanged.
HOST_MP2L_PPS = clamp_int(os.environ.get("SM64DS_RELAY_HOST_MP2L_PPS"),
                          150, 1, 2000)
# 64 -> 256 for version 2: an always-on world room is a session of its own.
MAX_SESSIONS = 256

# HOW MANY PEOPLE ONE SESSION HOLDS. A knob since the player-count dial landed,
# and the DEFAULT IS UNCHANGED, so a relay nobody reconfigures behaves exactly
# as it did: one parent and three children.
#
# WHY THIS IS SAFE TO MAKE A KNOB AT ALL. The relay does not read the game's
# packets. It checks a 16-byte HELLO, then forwards opaque bytes from the
# parent to every child and from a child to the parent. It has no opinion about
# slots, no per-player state, and no fixed-width field that counts players. So
# the number of children really is a number here, unlike everywhere else the
# four shows up.
#
# WHAT RAISING IT DOES NOT DO. It does not make the GAME play more than four.
# The game's own wire packet carries an 8-bit live mask, one byte of slot
# numbering, and a fixed four-block payload; the ROM's own code counts to four
# in a dozen places. This knob removes the relay from that list and nothing
# else. See the ceiling map in the lobby's README.
#
# The two numbers are DERIVED FROM ONE so they can never disagree: one parent
# plus this many children is what a session holds. The old file carried them
# separately (3 and 4) and a raise would have had to remember both.
MAX_CHILDREN = clamp_int(
    os.environ.get("SM64DS_RELAY_MAX_CHILDREN"), 3, 1, 15)
MAX_ENDPOINTS_PER_SESSION = MAX_CHILDREN + 1
BAD_ACK_INTERVAL_S = 1.0    # at most one status 2 per second per endpoint
BAD_IGNORE_S = 10.0         # then that endpoint is ignored for this long
BUCKET_IDLE_S = 30.0        # forget a rate bucket after this much quiet
MAX_BUCKETS = 8192          # hard ceiling on tracked source addresses
SWEEP_INTERVAL_S = 1.0

DEFAULT_PORT = 41234
DEFAULT_BIND = "0.0.0.0"
DEFAULT_IDLE_S = 90.0
DEFAULT_STATS_S = 60.0
DEFAULT_HUB_IDLE_S = 10.0
# An arena seat whose console has been silent this long (no datagram, no HELLO)
# is given to a console that asks for it by number; a live seat never is. 3 s:
# a 1.5-3 s Wi-Fi outage must not cost a player their seat to anyone who has the
# room code, and a real restart takes 4+ s to its first HELLO. It is also the
# game hub's presence timeout (comms_hub.cpp kHubPresenceMs), so by the time a
# seat can be reclaimed every other console has already seen its holder leave.
SEAT_RECLAIM_S = 3.0

# ------------------------------------------------------------------- utils


def log(msg):
    """One line to stdout. systemd picks these up; journalctl shows them."""
    ts = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    print("%s %s" % (ts, msg), flush=True)


def short_addr(addr):
    """A logging form of a client address with the host part blunted.

    We keep enough to tell two clients apart in a log and to spot a region,
    and drop the rest. Full client addresses never reach the log.
    """
    try:
        host = addr[0]
        port = addr[1]
    except Exception:
        return "?"
    if ":" in host:
        groups = host.split(":")
        head = ":".join(g for g in groups[:2] if g)
        host = (head + "::x") if head else "::x"
    else:
        octets = host.split(".")
        if len(octets) == 4:
            host = "%s.%s.x.x" % (octets[0], octets[1])
        else:
            host = "x"
    return "%s#%d" % (host, port)


def parse_code(raw):
    """Validate the 8 byte code field. Returns str, or None if unusable.

    Right padded with NUL, printable ASCII, no spaces, no interior NUL.
    """
    if len(raw) != 8:
        return None
    stripped = raw.rstrip(b"\x00")
    if not stripped:
        return None
    for b in stripped:
        if b < 0x21 or b > 0x7E:
            return None
    return stripped.decode("ascii")


def rss_kb():
    """Resident set size of this process in KB, or None if we cannot tell."""
    try:
        if sys.platform.startswith("linux"):
            with open("/proc/self/statm", "r") as fh:
                pages = int(fh.read().split()[1])
            return pages * (os.sysconf("SC_PAGE_SIZE") // 1024)
        if os.name == "nt":
            import ctypes

            class _PMC(ctypes.Structure):
                _fields_ = [
                    ("cb", ctypes.c_ulong),
                    ("PageFaultCount", ctypes.c_ulong),
                    ("PeakWorkingSetSize", ctypes.c_size_t),
                    ("WorkingSetSize", ctypes.c_size_t),
                    ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                    ("PagefileUsage", ctypes.c_size_t),
                    ("PeakPagefileUsage", ctypes.c_size_t),
                ]

            kernel32 = ctypes.WinDLL("kernel32")
            psapi = ctypes.WinDLL("psapi")
            # Without these the 64 bit process handle is passed as a 32 bit
            # int and the call quietly fails.
            kernel32.GetCurrentProcess.restype = ctypes.c_void_p
            psapi.GetProcessMemoryInfo.argtypes = [
                ctypes.c_void_p, ctypes.POINTER(_PMC), ctypes.c_ulong]
            psapi.GetProcessMemoryInfo.restype = ctypes.c_int
            counters = _PMC()
            counters.cb = ctypes.sizeof(_PMC)
            ok = psapi.GetProcessMemoryInfo(
                kernel32.GetCurrentProcess(), ctypes.byref(counters),
                counters.cb)
            if not ok:
                return None
            return int(counters.WorkingSetSize) // 1024
        import resource
        maxrss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        if sys.platform == "darwin":
            return int(maxrss) // 1024
        return int(maxrss)
    except Exception:
        return None


# ------------------------------------------------------------------ tables


class Endpoint(object):
    # version / seat / caps are version 2 only; a version 1 endpoint keeps
    # version 1, seat None and no caps, and nothing on its path reads them.
    # A version 2 endpoint's caps dict is the one RelayProtocol.kind_caps
    # holds for its address, so it outlives the seat (finding H3).
    __slots__ = ("addr", "code", "role", "last_seen", "version", "seat",
                 "caps")

    def __init__(self, addr, code, role, now, version=PROTO_VERSION,
                 seat=None, caps=None):
        self.addr = addr
        self.code = code
        self.role = role
        self.last_seen = now
        self.version = version
        self.seat = seat
        if version == PROTO_VERSION:
            self.caps = None
        else:
            self.caps = {} if caps is None else caps


class Session(object):
    """A version 1 session: the star, blind, exactly as it always was."""
    __slots__ = ("code", "parent", "children", "created")
    version = PROTO_VERSION

    def __init__(self, code, now):
        self.code = code
        self.parent = None
        self.children = []
        self.created = now

    def size(self):
        return (1 if self.parent is not None else 0) + len(self.children)

    def members(self):
        out = list(self.children)
        if self.parent is not None:
            out.append(self.parent)
        return out


class SessionV2(object):
    """A version 2 session: a STAR (seat 0 is the parent, 1..MAX_CHILDREN the
    children) or a HUB (seats 0..HUB_SEATS-1, all members, nobody hosts)."""
    __slots__ = ("code", "kind", "seats", "created", "fwd_tokens",
                 "fwd_stamp", "referee")
    version = PROTO_V2

    def __init__(self, code, kind, now):
        self.code = code
        self.kind = kind
        self.seats = {}         # seat -> addr
        self.created = now
        self.referee = None     # arena sessions only (referee.Referee)
        # the per-session forward budget (SESSION_FWD_PPS), in copies sent
        self.fwd_tokens = float(SESSION_FWD_BURST)
        self.fwd_stamp = now

    def budget_ok(self, copies, now):
        """Spend `copies` of the session's forward budget, or refuse the
        whole datagram (never a partial fan-out: every recipient of a
        datagram gets it or none does)."""
        elapsed = now - self.fwd_stamp
        if elapsed > 0:
            self.fwd_tokens = min(float(SESSION_FWD_BURST),
                                  self.fwd_tokens + elapsed * SESSION_FWD_PPS)
            self.fwd_stamp = now
        if self.fwd_tokens < copies:
            return False
        self.fwd_tokens -= copies
        return True

    def size(self):
        return len(self.seats)

    def capacity(self):
        if self.kind in (KIND_HUB, KIND_ARENA):
            return HUB_SEATS
        return MAX_ENDPOINTS_PER_SESSION

    def free_seat(self, lo, hi):
        """Lowest free seat in lo..hi inclusive, or None."""
        for seat in range(lo, hi + 1):
            if seat not in self.seats:
                return seat
        return None


class Bucket(object):
    """Token bucket, one per source address."""
    __slots__ = ("tokens", "stamp")

    def __init__(self, now, burst=None):
        self.tokens = float(RATE_BURST if burst is None else burst)
        self.stamp = now


# --------------------------------------------------------------- the relay


class RelayProtocol(asyncio.DatagramProtocol):

    def __init__(self, idle_s, hub_idle_s=DEFAULT_HUB_IDLE_S):
        self.idle_s = idle_s
        self.hub_idle_s = hub_idle_s
        self.transport = None
        self.sessions = {}      # code -> Session
        self.endpoints = {}     # addr -> Endpoint
        self.buckets = {}       # addr -> Bucket
        self.bad_ack_at = {}    # addr -> monotonic of last status 2 sent
        self.ignored = {}       # addr -> monotonic when the ignore ends
        self.kind_caps = {}     # addr -> {tag: [tokens, stamp]} (v2 senders)
        # counters, all cumulative
        self.n_rx = 0
        self.n_fwd = 0
        self.n_ack = 0
        self.n_drop_rate = 0
        self.n_drop_size = 0
        self.n_drop_unpaired = 0
        self.n_drop_ignored = 0
        self.n_bad = 0
        self.n_full = 0
        self.n_expired = 0
        # version 2 counters: HELLO refusals by status, per-reason drops, and
        # per kind [rx, fwd, drop] (fwd counts copies sent, like n_fwd)
        self.n_seat_taken = 0
        self.n_vmix = 0
        self.n_wrong_kind = 0
        self.v2_drop = dict((r, 0) for r in DROP_REASONS)
        self.kstats = dict((tag, [0, 0, 0]) for tag in KIND_ORDER)
        # socket errors the OS reported (error_received), logged once
        self.n_reset = 0
        self.reset_logged = False
        # arena sessions: per kind [rx, fwd, drop], drops by reason, and what
        # the referee itself sent (event frames + clock pongs)
        self.arenas = {}        # code -> SessionV2, the arena ones
        self.akstats = dict((tag, [0, 0, 0]) for tag in ARENA_ORDER)
        self.a_drop = dict((r, 0) for r in ARENA_DROPS)
        self.n_ref_tx = 0

    # ---- asyncio hooks

    def connection_made(self, transport):
        self.transport = transport

    def error_received(self, exc):
        # Windows raises WSAECONNRESET on the socket when a previous sendto
        # hit a closed port (an ICMP port-unreachable coming back). It says
        # nothing about the socket's health. run() turns that report off with
        # SIO_UDP_CONNRESET; this is the second line of defence, on any
        # platform, for a report that still gets through.
        #
        # THE TRAP (run mp1, HUB1 S1): on the Windows proactor loop an error
        # from a pending receive is handed to this method and the transport
        # then does NOT post another receive. The relay goes deaf for good,
        # every seat times out on every screen, and nothing is logged. So: log
        # the first one, count them all, and re-arm the receive if it died.
        # A selector loop (Linux) keeps reading on its own and is untouched.
        self.n_reset += 1
        if not self.reset_logged:
            self.reset_logged = True
            log("socket error from the OS (%s: %s); ignored, still receiving"
                " (logged once, counted as reset=)"
                % (type(exc).__name__, exc))
        transport = self.transport
        if (transport is not None
                and hasattr(transport, "_loop_reading")
                and getattr(transport, "_read_fut", False) is None):
            asyncio.get_running_loop().call_soon(self._rearm_reading,
                                                 transport)

    @staticmethod
    def _rearm_reading(transport):
        # Proactor datagram transports only (the hasattr check above). Checked
        # again here: the loop may have re-armed itself in between.
        if transport.is_closing() or transport._read_fut is not None:
            return
        transport._loop_reading()

    def datagram_received(self, data, addr):
        self.on_datagram(data, addr, time.monotonic())

    def on_datagram(self, data, addr, now):
        """One datagram at time `now`. Split from the asyncio hook so a test
        can drive the relay with its own clock."""
        self.n_rx += 1

        until = self.ignored.get(addr)
        if until is not None:
            if until > now:
                self.n_drop_ignored += 1
                return
            del self.ignored[addr]

        if not self._rate_ok(addr, now):
            self.n_drop_rate += 1
            return

        endpoint = self.endpoints.get(addr)
        if endpoint is None:
            # A stranger. The only thing a stranger has any business sending
            # is a HELLO, so a 16 byte datagram is read as one and anything
            # else is dropped without a word. That also means a reply can
            # never be larger than what arrived.
            if len(data) == HELLO_LEN:
                self._on_hello(data, addr, now)
            else:
                self.n_drop_unpaired += 1
            return

        if len(data) == HELLO_LEN and data[:4] == MAGIC_HELLO:
            self._on_hello(data, addr, now)
            return

        if endpoint.version == PROTO_V2:
            self._forward_v2(data, addr, now, endpoint)
            return
        self._forward(data, addr, now, endpoint)

    # ---- rate limiting

    def _rate_ok(self, addr, now):
        # The budget follows the session version of the endpoint at this
        # address: a version 2 seat gets RATE_PPS_V2, everybody else
        # (version 1 seats and strangers) the version 1 budget, unchanged.
        endpoint = self.endpoints.get(addr)
        if endpoint is not None and endpoint.version == PROTO_V2:
            pps, burst = RATE_PPS_V2, RATE_BURST_V2
        else:
            pps, burst = RATE_PPS, RATE_BURST
        bucket = self.buckets.get(addr)
        if bucket is None:
            if len(self.buckets) >= MAX_BUCKETS:
                # Tracking table is full. Fail closed rather than grow.
                return False
            bucket = Bucket(now, burst)
            self.buckets[addr] = bucket
        else:
            elapsed = now - bucket.stamp
            if elapsed > 0:
                bucket.tokens = min(
                    float(burst), bucket.tokens + elapsed * pps)
                bucket.stamp = now
        if bucket.tokens < 1.0:
            return False
        bucket.tokens -= 1.0
        return True

    # ---- sending

    def _send_ack(self, addr, status, code_raw, request_len,
                  version=PROTO_VERSION, seat=0):
        """Reply to a HELLO.

        Never sends more bytes than we just received from this endpoint.
        An unpaired stranger can therefore never use us as an amplifier.
        A version 1 ACK is byte for byte what it always was (reserved zero);
        a version 2 ACK carries the seat the relay will stamp at byte 6.
        """
        if len(code_raw) != 8:
            code_raw = (code_raw + b"\x00" * 8)[:8]
        if version == PROTO_V2:
            reply = (MAGIC_ACK + bytes((PROTO_V2, status, seat & 0xFF, 0))
                     + code_raw)
        else:
            reply = (MAGIC_ACK + bytes((PROTO_VERSION, status)) + b"\x00\x00"
                     + code_raw)
        if len(reply) > request_len:
            return
        try:
            self.transport.sendto(reply, addr)
        except Exception:
            return
        self.n_ack += 1

    # ---- HELLO

    def _reject(self, data, addr, now, code_raw=None, version=PROTO_VERSION):
        """Malformed HELLO: answer at most once, then go quiet on them."""
        last = self.bad_ack_at.get(addr)
        if last is None or (now - last) >= BAD_ACK_INTERVAL_S:
            self.bad_ack_at[addr] = now
            self.n_bad += 1
            if code_raw is None:
                code_raw = data[8:16] if len(data) >= 16 else b""
            self._send_ack(addr, STATUS_BAD, code_raw, len(data),
                           version=version)
            log("bad request from %s len=%d" % (short_addr(addr), len(data)))
        self.ignored[addr] = now + BAD_IGNORE_S

    def _on_hello(self, data, addr, now):
        version = data[4]
        role = data[5]
        code_raw = data[8:16]
        if data[:4] != MAGIC_HELLO:
            self._reject(data, addr, now, code_raw=b"\x00" * 8)
            return
        if version == PROTO_V2:
            self._on_hello_v2(data, addr, now)
            return
        if version != PROTO_VERSION or role not in (ROLE_PARENT, ROLE_CHILD):
            self._reject(data, addr, now)
            return
        code = parse_code(code_raw)
        if code is None:
            self._reject(data, addr, now, code_raw=b"\x00" * 8)
            return
        # The reserved field is not policed: we ignore what arrives in it and
        # always send zeros back.

        endpoint = self.endpoints.get(addr)
        if endpoint is not None and (endpoint.code != code
                                     or endpoint.role != role
                                     or endpoint.version != PROTO_VERSION):
            # Same endpoint asking for something else. Drop the old seat.
            self._detach(endpoint, "rejoin", now)
            endpoint = None

        if endpoint is not None:
            # Known endpoint, same seat. Idempotent: refresh and re-ACK.
            endpoint.last_seen = now
            self._send_ack(addr, STATUS_OK, code_raw, len(data))
            return

        session = self.sessions.get(code)
        if session is not None and session.version != PROTO_VERSION:
            # A version 1 game quoting a version 2 session's code. Version 1
            # has no status 3, so it gets the refusal it knows: FULL, which an
            # old game logs ("session is FULL") and then plays solo. The
            # session is untouched.
            self.n_vmix += 1
            log("session %s refused %s: a v1 HELLO for a v2 session"
                % (code, short_addr(addr)))
            self._send_ack(addr, STATUS_FULL, code_raw, len(data))
            return
        if session is None:
            if len(self.sessions) >= MAX_SESSIONS:
                self.n_full += 1
                log("session %s refused: relay at %d sessions"
                    % (code, MAX_SESSIONS))
                self._send_ack(addr, STATUS_FULL, code_raw, len(data))
                return
            session = Session(code, now)
            self.sessions[code] = session
            log("session %s create by %s" % (code, short_addr(addr)))

        if role == ROLE_PARENT:
            if session.parent is not None and session.parent != addr:
                self.n_full += 1
                log("session %s refused parent %s: parent seat taken"
                    % (code, short_addr(addr)))
                self._send_ack(addr, STATUS_FULL, code_raw, len(data))
                self._drop_if_empty(session)
                return
            session.parent = addr
        else:
            if (len(session.children) >= MAX_CHILDREN
                    or session.size() >= MAX_ENDPOINTS_PER_SESSION):
                self.n_full += 1
                log("session %s refused child %s: %d children already"
                    % (code, short_addr(addr), len(session.children)))
                self._send_ack(addr, STATUS_FULL, code_raw, len(data))
                self._drop_if_empty(session)
                return
            session.children.append(addr)

        self.endpoints[addr] = Endpoint(addr, code, role, now)
        log("session %s join %s as %s (%d/%d)"
            % (code, short_addr(addr),
               "parent" if role == ROLE_PARENT else "child",
               session.size(), MAX_ENDPOINTS_PER_SESSION))
        self._send_ack(addr, STATUS_OK, code_raw, len(data))

    # ---- forwarding

    def _forward(self, data, addr, now, endpoint):
        if len(data) > MAX_PAYLOAD:
            self.n_drop_size += 1
            return
        endpoint.last_seen = now
        session = self.sessions.get(endpoint.code)
        if session is None:
            self.n_drop_unpaired += 1
            return
        if endpoint.role == ROLE_PARENT:
            targets = session.children
        else:
            targets = (session.parent,) if session.parent is not None else ()
        for target in targets:
            if target == addr:
                continue
            try:
                self.transport.sendto(data, target)
            except Exception:
                continue
            self.n_fwd += 1

    # ---- version 2: HELLO

    def _refuse_v2(self, addr, status, code, code_raw, request_len, why):
        """A well-formed version 2 HELLO the relay cannot seat. Answered every
        time (the reply is never longer than the HELLO) and never ignored:
        the game is not malformed, the room is just not open to it."""
        if status == STATUS_FULL:
            self.n_full += 1
        elif status == STATUS_SEAT_TAKEN:
            self.n_seat_taken += 1
        elif status == STATUS_VERSION:
            self.n_vmix += 1
        elif status == STATUS_WRONG_KIND:
            self.n_wrong_kind += 1
        log("session %s refused %s: %s (v2 status %d)"
            % (code, short_addr(addr), why, status))
        self._send_ack(addr, status, code_raw, request_len, version=PROTO_V2)

    def _on_hello_v2(self, data, addr, now):
        role = data[5]
        want = data[6]
        # data[7] is flags; bit0 "this endpoint will send voice". Every
        # version 2 endpoint already gets the voice budget (RATE_PPS_V2), so
        # the flag changes nothing today and unknown bits are not policed,
        # the same rule as version 1's reserved field.
        code_raw = data[8:16]
        n = len(data)
        code = parse_code(code_raw)
        if code is None:
            self._reject(data, addr, now, code_raw=b"\x00" * 8,
                         version=PROTO_V2)
            return
        # want_seat grammar, per role. Anything else is a malformed HELLO.
        if role == ROLE_PARENT:
            ok = want == 0
        elif role == ROLE_CHILD:
            ok = (1 <= want <= 15) or want == WANT_ANY
        elif role == ROLE_MEMBER:
            ok = (0 <= want < HUB_SEATS) or want == WANT_ANY
        elif role == ROLE_ARENA and referee is not None:
            ok = (0 <= want < HUB_SEATS) or want == WANT_ANY
        else:
            ok = False
        if not ok:
            self._reject(data, addr, now, version=PROTO_V2)
            return
        if role == ROLE_ARENA:
            kind = KIND_ARENA
        else:
            kind = KIND_HUB if role == ROLE_MEMBER else KIND_STAR

        endpoint = self.endpoints.get(addr)
        if endpoint is not None:
            if (endpoint.version == PROTO_V2 and endpoint.code == code
                    and endpoint.role == role
                    and (want == WANT_ANY or want == endpoint.seat)):
                # Known endpoint, same seat: the keepalive. Idempotent.
                endpoint.last_seen = now
                self._send_ack(addr, STATUS_OK, code_raw, n,
                               version=PROTO_V2, seat=endpoint.seat)
                return
            self._detach(endpoint, "rejoin", now)

        session = self.sessions.get(code)
        if session is not None and session.version != PROTO_V2:
            self._refuse_v2(addr, STATUS_VERSION, code, code_raw, n,
                            "a v2 HELLO for a v1 session")
            return
        if session is not None and session.kind != kind:
            self._refuse_v2(addr, STATUS_WRONG_KIND, code, code_raw, n,
                            "a %s role into a %s session"
                            % (kind, session.kind))
            return
        created = False
        if session is None:
            if len(self.sessions) >= MAX_SESSIONS:
                self._refuse_v2(addr, STATUS_FULL, code, code_raw, n,
                                "relay at %d sessions" % MAX_SESSIONS)
                return
            session = SessionV2(code, kind, now)
            if kind == KIND_ARENA:
                session.referee = self._new_referee(session, now)
                self.arenas[code] = session
            self.sessions[code] = session
            created = True
            log("session %s create by %s (v2 %s)"
                % (code, short_addr(addr), kind))

        # Seat the endpoint.
        status, why, seat = STATUS_OK, "", None
        if kind == KIND_STAR and role == ROLE_PARENT:
            if 0 in session.seats:
                status, why = STATUS_SEAT_TAKEN, "parent seat taken"
            else:
                seat = 0
        elif kind == KIND_STAR:
            if want == WANT_ANY:
                seat = session.free_seat(1, MAX_CHILDREN)
                if seat is None:
                    status, why = STATUS_FULL, "no free child seat"
            elif want > MAX_CHILDREN:
                status, why = STATUS_FULL, ("seat %d is past this relay's %d "
                                            "children" % (want, MAX_CHILDREN))
            elif want in session.seats:
                status, why = STATUS_SEAT_TAKEN, "seat %d taken" % want
            else:
                seat = want
        else:
            # An arena session seats only as many consoles as the match has
            # bodies (the referee learns that from the first READY's
            # capacity; before it, the room is open to every hub seat). A
            # seat with no body would be a console the game cannot seat.
            top = HUB_SEATS
            if kind == KIND_ARENA and session.referee is not None:
                bodies = session.referee.body_count()
                if bodies is not None:
                    top = min(top, bodies)
            if want == WANT_ANY:
                seat = session.free_seat(0, top - 1)
                if seat is None:
                    status, why = STATUS_FULL, (
                        "hub full" if top == HUB_SEATS else
                        "all %d bodies of this arena are seated" % top)
            elif want >= top:
                status, why = STATUS_FULL, (
                    "seat %d has no body (this arena has %d)" % (want, top))
            elif want in session.seats:
                holder = self.endpoints.get(session.seats[want])
                if (kind == KIND_ARENA and holder is not None
                        and (now - holder.last_seen) >= SEAT_RECLAIM_S):
                    # The seat's console has gone silent (a crash and a quick
                    # restart is a new source port). The returning console
                    # takes its seat back now instead of waiting out the
                    # idle expiry. The old holder leaves FIRST, so the
                    # referee's leave / spill rules run before the join. A
                    # live holder (heard within SEAT_RECLAIM_S) is never
                    # displaced: that is the only thing standing between a
                    # seat and anybody who knows the room code.
                    log("session %s seat %d reclaimed from %s: silent %.1f s"
                        % (code, want, short_addr(holder.addr),
                           now - holder.last_seen))
                    self._detach(holder, "seat reclaimed by a returning "
                                 "console", now)
                    if self.sessions.get(code) is not session:
                        # the holder was the room's only member: the detach
                        # expired the session, and the returning console is
                        # its next member
                        self.sessions[code] = session
                        self.arenas[code] = session
                    seat = want
                else:
                    status, why = STATUS_SEAT_TAKEN, "seat %d taken" % want
            else:
                seat = want
        if status != STATUS_OK:
            self._refuse_v2(addr, status, code, code_raw, n, why)
            if created:
                self._drop_if_empty(session)
            return

        session.seats[seat] = addr
        self.endpoints[addr] = Endpoint(
            addr, code, role, now, version=PROTO_V2, seat=seat,
            caps=self.kind_caps.setdefault(addr, {}))
        log("session %s join %s as %s seat %d (%d/%d) v2"
            % (code, short_addr(addr),
               ("parent", "child", "member", "arena")[role], seat,
               session.size(), session.capacity()))
        self._send_ack(addr, STATUS_OK, code_raw, n, version=PROTO_V2,
                       seat=seat)
        if session.referee is not None:
            session.referee.on_join(seat, now)

    # ---- version 2: forwarding by the kind table

    def _kind_cap_ok(self, endpoint, kind, now, rate=None):
        """Per (sender, kind) token bucket: kind.cap a second (or `rate`),
        one second deep. Inside the per-source budget, so no kind can starve
        another."""
        rate = kind.cap if rate is None else rate
        cap = endpoint.caps.get(kind.tag)
        if cap is None:
            cap = [float(rate), now]
            endpoint.caps[kind.tag] = cap
        else:
            elapsed = now - cap[1]
            if elapsed > 0:
                cap[0] = min(float(rate), cap[0] + elapsed * rate)
                cap[1] = now
        if cap[0] < 1.0:
            return False
        cap[0] -= 1.0
        return True

    def _drop_v2(self, reason, kstat=None):
        self.v2_drop[reason] += 1
        if kstat is not None:
            kstat[2] += 1

    def _forward_v2(self, data, addr, now, endpoint):
        n = len(data)
        if endpoint.role == ROLE_ARENA:
            self._forward_arena(data, addr, now, endpoint)
            return
        kind = KINDS.get(bytes(data[:4])) if n >= 4 else None
        if kind is None:
            self._drop_v2("unknown")
            return
        kstat = self.kstats[kind.tag]
        kstat[0] += 1
        if not kind.length_ok(n):
            self._drop_v2("length", kstat)
            return
        session = self.sessions.get(endpoint.code)
        if session is None:
            self.n_drop_unpaired += 1
            kstat[2] += 1
            return
        route = kind.hub if session.kind == KIND_HUB else kind.star
        if route is None:
            self._drop_v2("route", kstat)
            return
        seat = endpoint.seat
        if route == R_STAR:
            if seat == 0:
                targets = [a for s, a in session.seats.items() if s != 0]
            else:
                parent = session.seats.get(0)
                targets = [parent] if parent is not None else []
        elif route == R_ALL:
            targets = [a for s, a in session.seats.items() if s != seat]
        elif route == R_UP:
            if seat == 0:
                self._drop_v2("route", kstat)
                return
            parent = session.seats.get(0)
            targets = [parent] if parent is not None else []
        else:  # R_SEAT6
            to = session.seats.get(data[6])
            targets = [to] if (to is not None and data[6] != seat) else []
        # The cap is spent only on a datagram that is otherwise forwardable,
        # so garbage cannot eat a seat's own budget for a kind.
        rate = None
        if route == R_STAR and seat == 0 and kind.tag == b"MP2L":
            rate = HOST_MP2L_PPS        # the star host's lockstep cap
        if not self._kind_cap_ok(endpoint, kind, now, rate):
            self._drop_v2("cap", kstat)
            return
        endpoint.last_seen = now
        if not targets:
            self._drop_v2("target", kstat)
            return
        if not session.budget_ok(len(targets), now):
            self._drop_v2("session_cap", kstat)
            return
        out = bytearray(data)
        out[kind.stamp] = seat
        out = bytes(out)
        for target in targets:
            try:
                self.transport.sendto(out, target)
            except Exception:
                continue
            self.n_fwd += 1
            kstat[1] += 1

    # ---- arena sessions: the referee's kind table

    def _new_referee(self, session, now):
        code = session.code

        def send(seat, data, session=session):
            addr = session.seats.get(seat)
            if addr is None:
                return
            try:
                self.transport.sendto(data, addr)
            except Exception:
                return
            self.n_ref_tx += 1

        def kick(seat, session=session):
            addr = session.seats.get(seat)
            endpoint = self.endpoints.get(addr) if addr is not None else None
            if endpoint is not None:
                self._detach(endpoint, "kicked by the referee")
                self.ignored[addr] = time.monotonic() + BAD_IGNORE_S

        return referee.Referee(code, now, send, log,
                               kick=kick if REFEREE_KICK else None,
                               seed=REFEREE_SEED)

    def _drop_a(self, reason, kstat=None):
        self.a_drop[reason] += 1
        if kstat is not None:
            kstat[2] += 1

    def _forward_arena(self, data, addr, now, endpoint):
        """An arena seat's datagram, by ARENA_KINDS. Same discipline as a
        hub (classify by tag + exact length, cap, budget, stamp byte 5), plus
        the referee: it reads state / object frames, takes claims and clock
        pings, and is the only source of 'SYNR' and 20-byte 'SYNQ'."""
        n = len(data)
        entry = ARENA_KINDS.get(bytes(data[:4])) if n >= 4 else None
        if entry is None:
            self._drop_a("unknown")
            return
        route, kind = entry
        kstat = self.akstats[kind.tag]
        kstat[0] += 1
        if not kind.length_ok(n):
            self._drop_a("length", kstat)
            return
        session = self.sessions.get(endpoint.code)
        if session is None or session.referee is None:
            self.n_drop_unpaired += 1
            kstat[2] += 1
            return
        ref = session.referee
        seat = endpoint.seat
        if route == A_NONE or (route == A_PONG and n != 12) or (
                route == A_PING and n != 12 and data[6] != referee.REF_MARK):
            self._drop_a("route", kstat)
            if route == A_NONE:
                ref.on_violation(seat, "sent a referee-only kind", now)
            return
        if route == A_ALLREF:
            ok = (ref.check_state(seat, data) if kind.tag == b"SYNS"
                  else ref.check_object(seat, data))
            if not ok:
                self._drop_a("field", kstat)
                ref.on_violation(seat, "%s field out of range"
                                 % kind.tag.decode("ascii"), now)
                return
        if not self._kind_cap_ok(endpoint, kind, now):
            self._drop_a("cap", kstat)
            return
        endpoint.last_seen = now
        out = bytearray(data)
        out[5] = seat
        out = bytes(out)
        if route == A_REF:
            ref.on_claim(seat, out, now)
            return
        if route == A_PING and n == 20:
            reply = ref.on_ping(seat, out, now)
            if reply is not None:
                try:
                    self.transport.sendto(reply, addr)
                    self.n_ref_tx += 1
                except Exception:
                    pass
            return
        if route == A_PONG:
            to = session.seats.get(data[6])
            targets = [to] if (to is not None and data[6] != seat) else []
        else:
            targets = [a for s, a in session.seats.items() if s != seat]
        if route == A_ALLREF:
            if kind.tag == b"SYNS":
                ref.on_state(seat, out, now)
            else:
                ref.on_object(seat, out, now)
        if not targets:
            self._drop_a("target", kstat)
            return
        if not session.budget_ok(len(targets), now):
            self._drop_a("session_cap", kstat)
            return
        for target in targets:
            try:
                self.transport.sendto(out, target)
            except Exception:
                continue
            self.n_fwd += 1
            kstat[1] += 1

    def tick(self, now):
        """The referees' clock (REFEREE_TICK_S): verdicts whose hold has
        passed, match timers, heartbeats. Nothing at all without arenas."""
        for session in list(self.arenas.values()):
            if session.referee is not None:
                session.referee.tick(now)

    # ---- bookkeeping

    def _detach(self, endpoint, why, now=None):
        self.endpoints.pop(endpoint.addr, None)
        session = self.sessions.get(endpoint.code)
        if session is None:
            return
        if session.version == PROTO_V2:
            if session.seats.get(endpoint.seat) == endpoint.addr:
                del session.seats[endpoint.seat]
                if session.referee is not None:
                    session.referee.on_leave(
                        endpoint.seat,
                        time.monotonic() if now is None else now)
            log("session %s leave %s seat %s (%s)"
                % (endpoint.code, short_addr(endpoint.addr), endpoint.seat,
                   why))
            self._drop_if_empty(session)
            return
        if session.parent == endpoint.addr:
            session.parent = None
        if endpoint.addr in session.children:
            session.children.remove(endpoint.addr)
        log("session %s leave %s (%s)"
            % (endpoint.code, short_addr(endpoint.addr), why))
        self._drop_if_empty(session)

    def _drop_if_empty(self, session):
        if session.size() == 0 and self.sessions.get(session.code) is session:
            del self.sessions[session.code]
            if self.arenas.get(session.code) is session:
                del self.arenas[session.code]
            log("session %s expire" % session.code)

    def sweep(self, now):
        for addr, endpoint in list(self.endpoints.items()):
            idle = self.idle_s
            if (endpoint.role in (ROLE_MEMBER, ROLE_ARENA)
                    and endpoint.version == PROTO_V2):
                idle = self.hub_idle_s
            if (now - endpoint.last_seen) > idle:
                self.n_expired += 1
                self._detach(endpoint, "idle", now)
        for addr, caps in list(self.kind_caps.items()):
            if addr in self.endpoints:
                continue
            last = max([c[1] for c in caps.values()] or [0.0])
            if (now - last) > KIND_CAPS_IDLE_S:
                del self.kind_caps[addr]
        for addr, bucket in list(self.buckets.items()):
            if (now - bucket.stamp) > BUCKET_IDLE_S and addr not in self.endpoints:
                del self.buckets[addr]
        for addr, until in list(self.ignored.items()):
            if until <= now:
                del self.ignored[addr]
        for addr, stamp in list(self.bad_ack_at.items()):
            if (now - stamp) > BAD_IGNORE_S * 2:
                del self.bad_ack_at[addr]

    def stats_line(self):
        rss = rss_kb()
        return ("stats sessions=%d endpoints=%d rx=%d fwd=%d ack=%d "
                "drop_rate=%d drop_size=%d drop_unpaired=%d drop_ignored=%d "
                "bad=%d full=%d expired=%d rss_kb=%s%s"
                % (len(self.sessions), len(self.endpoints), self.n_rx,
                   self.n_fwd, self.n_ack, self.n_drop_rate, self.n_drop_size,
                   self.n_drop_unpaired, self.n_drop_ignored, self.n_bad,
                   self.n_full, self.n_expired,
                   "?" if rss is None else str(rss), self.stats_v2()))

    def stats_v2(self):
        """The version 2 tail of the stats line: session and seat refusals,
        drops by reason, and per kind rx/fwd/drop (fwd counts copies sent)."""
        v2_sessions = sum(1 for s in self.sessions.values()
                          if s.version == PROTO_V2)
        hubs = sum(1 for s in self.sessions.values()
                   if s.version == PROTO_V2 and s.kind == KIND_HUB)
        head = (" reset=%d v2_sessions=%d hubs=%d seat_taken=%d vmix=%d "
                "wrong_kind=%d"
                % (self.n_reset, v2_sessions, hubs, self.n_seat_taken,
                   self.n_vmix, self.n_wrong_kind))
        drops = " ".join("drop_%s=%d" % (r, self.v2_drop[r])
                         for r in DROP_REASONS)
        kinds = " ".join("%s=%d/%d/%d" % ((tag.decode("ascii"),)
                                          + tuple(self.kstats[tag]))
                         for tag in KIND_ORDER)
        return "%s %s %s%s" % (head, drops, kinds, self.stats_arena())

    def stats_arena(self):
        """The arena tail: sessions, referee sends, drops by reason, and per
        arena kind rx/fwd/drop."""
        # One token each, spelled so no star / hub counter name ("drop_x=",
        # "SYNP=") appears inside them: log readers grep for those.
        drops = ",".join("%s:%d" % (r, self.a_drop[r]) for r in ARENA_DROPS)
        kinds = ",".join("%s:%d/%d/%d" % ((tag.decode("ascii"),)
                                          + tuple(self.akstats[tag]))
                         for tag in ARENA_ORDER)
        return " arenas=%d ref_tx=%d arena_drops=%s arena_kinds=%s" % (
            len(self.arenas), self.n_ref_tx, drops, kinds)


# ------------------------------------------------------------------- entry

SIO_UDP_CONNRESET = 0x9800000C     # _WSAIOW(IOC_VENDOR, 12), mstcpip.h


def udp_connreset_off(sock):
    """Windows only: stop the OS reporting an ICMP port-unreachable (a peer's
    port closed) as WSAECONNRESET on this UDP socket.

    Without this, a relay on Windows goes deaf the first time any member's
    game exits: the next forward to that member's closed port comes back as
    an error on the receive, and the asyncio proactor loop stops receiving
    (run mp1, HUB1 S1). Linux does not report ICMP errors on an unconnected
    UDP socket, so the deployed relay never needed this; local Windows test
    rungs do. Python's socket.ioctl refuses this control code and asyncio
    hands back a socket wrapper without ioctl, so it is a raw WSAIoctl.
    SM64DS_RELAY_KEEP_CONNRESET=1 skips it (tests only: it lets the suite
    prove the second line of defence in error_received on its own).
    Returns a word for the startup log."""
    if os.name != "nt":
        return "n/a"
    if os.environ.get("SM64DS_RELAY_KEEP_CONNRESET") == "1":
        return "kept (SM64DS_RELAY_KEEP_CONNRESET=1)"
    try:
        import ctypes
        from ctypes import wintypes
        ws2 = ctypes.WinDLL("ws2_32")
        ws2.WSAIoctl.argtypes = [
            ctypes.c_size_t, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD,
            ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
            ctypes.c_void_p, ctypes.c_void_p]
        ws2.WSAIoctl.restype = ctypes.c_int
        off = wintypes.BOOL(False)
        got = wintypes.DWORD(0)
        rc = ws2.WSAIoctl(sock.fileno(), SIO_UDP_CONNRESET, ctypes.byref(off),
                          ctypes.sizeof(off), None, 0, ctypes.byref(got),
                          None, None)
        if rc != 0:
            return "FAILED (WSAIoctl rc %d, error %d)" % (
                rc, ws2.WSAGetLastError())
        return "off"
    except Exception as exc:
        return "FAILED (%s)" % exc


async def run():
    port = int(os.environ.get("SM64DS_RELAY_PORT", DEFAULT_PORT))
    bind = os.environ.get("SM64DS_RELAY_BIND", DEFAULT_BIND)
    idle_s = float(os.environ.get("SM64DS_RELAY_IDLE_S", DEFAULT_IDLE_S))
    stats_s = float(os.environ.get("SM64DS_RELAY_STATS_S", DEFAULT_STATS_S))
    hub_idle_s = float(os.environ.get("SM64DS_RELAY_HUB_IDLE_S",
                                      DEFAULT_HUB_IDLE_S))

    family = socket.AF_INET6 if ":" in bind else socket.AF_INET
    loop = asyncio.get_running_loop()
    transport, protocol = await loop.create_datagram_endpoint(
        lambda: RelayProtocol(idle_s, hub_idle_s), local_addr=(bind, port),
        family=family)

    sock = transport.get_extra_info("socket")
    if sock is not None:
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1 << 20)
        except OSError:
            pass
    connreset = udp_connreset_off(sock) if sock is not None else "no socket"

    log("relay listening on %s:%d idle=%.0fs max_sessions=%d "
        "max_payload=%d rate_pps=%d rate_burst=%d seats=%d "
        "(1 parent + %d children)"
        % (bind, port, idle_s, MAX_SESSIONS, MAX_PAYLOAD, RATE_PPS,
           RATE_BURST, MAX_ENDPOINTS_PER_SESSION, MAX_CHILDREN))
    log("v2: rate_pps_v2=%d session_fwd_pps=%d host_mp2l_pps=%d "
        "hub_idle=%.0fs hub_seats=%d kinds=%s"
        % (RATE_PPS_V2, SESSION_FWD_PPS, HOST_MP2L_PPS, hub_idle_s, HUB_SEATS,
           ",".join(t.decode("ascii") for t in KIND_ORDER)))
    if os.name == "nt":
        log("windows: udp connreset %s" % connreset)
    if RATE_PPS < 240:
        log("note: rate_pps %d is the pre-voice budget for version 1 "
            "sessions. A v1 seat running proximity voice chat sends ~117 "
            "packets a second; set SM64DS_RELAY_RATE_PPS=240 before hosting "
            "v1 voice sessions. Version 2 sessions use rate_pps_v2."
            % RATE_PPS)
    log(protocol.stats_line())

    log("arena: referee %s tick=%.0fms kick=%s"
        % ("loaded" if referee is not None else "MISSING (arena off)",
           REFEREE_TICK_S * 1000,
           "on" if REFEREE_KICK else "off"))

    stop = asyncio.Event()

    async def referee_clock():
        # Only arena sessions do anything here; without one a tick is a
        # loop over an empty dict.
        while not stop.is_set():
            # 10 ms while an arena session exists; a quarter second otherwise,
            # so a relay with no arena pays nothing for the referee's clock
            await asyncio.sleep(REFEREE_TICK_S if protocol.arenas else 0.25)
            try:
                protocol.tick(time.monotonic())
            except Exception as exc:     # never let one session stop the clock
                log("referee tick error: %s: %s" % (type(exc).__name__, exc))

    clock_task = loop.create_task(referee_clock())
    try:
        import signal
        for signame in ("SIGTERM", "SIGINT"):
            signum = getattr(signal, signame, None)
            if signum is not None:
                loop.add_signal_handler(signum, stop.set)
    except (NotImplementedError, RuntimeError, ImportError):
        pass  # Windows: fall back to KeyboardInterrupt

    last_stats = time.monotonic()
    try:
        while not stop.is_set():
            try:
                await asyncio.wait_for(stop.wait(), timeout=SWEEP_INTERVAL_S)
            except asyncio.TimeoutError:
                pass
            now = time.monotonic()
            protocol.sweep(now)
            if (now - last_stats) >= stats_s:
                last_stats = now
                log(protocol.stats_line())
    finally:
        clock_task.cancel()
        log(protocol.stats_line())
        log("relay stopping")
        transport.close()


def main():
    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
