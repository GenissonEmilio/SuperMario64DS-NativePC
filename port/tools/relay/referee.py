#!/usr/bin/env python3
"""THE ARENA REFEREE (run mp1, lane REFEREE1; VSSYNC1 plan section 4, phase A).

In an arena-mode VS session every console runs its own arena world and its own
Yoshi with no input delay; everything two players can disagree about is decided
HERE, inside the relay that already carries their packets: which star wakes
next, who broke a marker, who got a contested star, whether a hit landed, the
score, the clock and the winner. One authority, so there can never be two
winners.

The wire is arena_contract.py's (arena_contract.json / arena_wire.h); this file
carries its own copy of the numbers so the relay box needs nothing else, and
test_referee.py checks the copy against the contract.

What the relay hands the referee (relay.py, an arena session):
  check_state / on_state   every 'SYNS' (48 B): own-body state, 30 Hz
  check_object / on_object every 'SYNO' (32 B): a star in flight
  on_claim                 every 'SYNE' (24 B): a claim, never forwarded
  on_ping                  'SYNP' (20 B, byte 6 = 0xFE): the clock ping
  on_join / on_leave       the relay seated / dropped a member
  on_violation             a seat sent what only the referee may send
  tick                     ~100 Hz: verdicts, the clock, heartbeats
and the referee answers through `send(seat, datagram)` with 'SYNR' (272 B)
event frames and 'SYNQ' (20 B) clock pongs, nothing else.

HOW A CLAIM IS DECIDED. Claims carry the console's referee-clock stamp of the
touch. A stamp is clamped into [arrival - cap, arrival], cap = min(150, the
seat's min RTT / 2 + 25) ms (no claims from the future, no more back-dating
than the seat's own one-way time plus a margin; M3: a cheater who back-dates
every stamp wins 62.5% of ties instead of 50%). Claims are then decided in
STAMP order once the referee clock passes stamp + H, H = the largest cap of the
other seats in the match: by then no other seat can still deliver a claim with
an earlier stamp. Earliest stamp wins; exact ties go to fewer stars, then the
match seed's seat order.

NO AMPLIFIER. The referee sends nothing to a seat until that seat has PROVEN
it receives at its address: its clock ping echoes the exact referee time of a
pong the referee sent to it, and every seat sees the referee clock from its
OWN random base (pongs, claim stamps and every time in a SYNR it receives),
so neither a spoofed source address nor another seat of the same session
can ever echo one. Until
then the seat gets only clock pongs, each the size of the ping that asked.
And whatever a seat has proven, the referee never sends it more than
AMP_FACTOR times the bytes it has sent the referee (a send credit per seat,
the QUIC rule, capped at AMP_CREDIT_CAP): a seat that goes quiet stops
hearing the referee, so even a seat verified by a trick (REVREFEREE1 H1: one
real seat reading the session clock for forged ones) cannot be turned into a
reflector. A live console streams SYNS at 10-30 Hz, three times which is
1.4-4.3 KB/s of credit against a 1.1 KB/s heartbeat.

SECURITY. Fixed-size kinds only (the relay checks tag + exact length before
anything here runs); every field is range-checked; every table is bounded
(history 64 samples a seat, 64 decided claims a seat, 64 retained events, 255
star instances, a pending queue capped per seat); nothing a client says that
the referee can check is trusted (the ledger is the referee's, a SYNS carries
no score, a grab / hit is checked against the claimant's and victim's OWN
reported positions). Phase A logs speed strikes; kicking is phase B.

Python 3 standard library only.
"""

import math
import os
import random
import struct
from collections import deque

# ---------------------------------------------------------------- numbers
# (arena_contract.py NUMBERS; test_referee.py checks this copy)

FRAME_MS = 1000.0 / 30.0
HEARTBEAT_HZ = 4
CLAIM_RESEND_MS = 60
VERDICT_TIMEOUT_MS = 500
HOLD_MAX_MS = 150
CAP_MARGIN_MS = 25
CAP_UNKNOWN_MS = 100
FUTURE_SLACK_MS = 5
COMP_MAX_MS = 200
HIT_IMMUNE_MS = 1000
# an ESCAPE earlier than this after the EATEN is refused: the ROM's victim
# needs 23 or more frames of mashing after its own console has the event, so a
# real escape is never this fast (a cheating victim could otherwise make its
# eater lose a star one round trip after being eaten)
ESCAPE_MIN_MS = int(15 * FRAME_MS)
TRADE_MS = 34
REACH_H = 250
REACH_V = 250
GRAB_R = 200
MARKER_R = 300
ATTACK_LOOKBACK_FRAMES = 10
PRESENCE_MS = 3000
START_WAIT_MS = 20000
COUNTDOWN_MS = 3000
OVER_LINGER_MS = 3000
SNAPSHOT_LAG_MS = 2000
HISTORY_FRAMES = 64
INVULN_CHEAT_MS = 300
STRIKE_WINDOW_MS = 10000
STRIKES_TO_KICK = 3
POS_LIMIT_UNITS = 32768

FIX = 4096                      # Fix12
POS_LIMIT = POS_LIMIT_UNITS * FIX
SEATS = 16
REF_MARK = 0xFE
SAMPLE_TOLERANCE_FRAMES = 15    # a claim's frame must have a sample this near
RETAIN_EVENTS = 64
DONE_PER_SEAT = 64
PENDING_PER_SEAT = 16
CATCHUP_EVERY_MS = 250
RTT_KEEP = 8
AMP_FACTOR = 3                  # referee bytes out <= 3 x the seat's bytes in
AMP_CREDIT_CAP = 16384          # the most credit a seat can bank
RESET_BACK_FRAMES = 300         # a frame this far back = the console restarted

# ---------------------------------------------------------------- wire

TAG_SYNS, TAG_SYNO, TAG_SYNE = b"SYNS", b"SYNO", b"SYNE"
TAG_SYNR, TAG_SYNP, TAG_SYNQ = b"SYNR", b"SYNP", b"SYNQ"
SYNS_LEN, SYNO_LEN, SYNE_LEN, PING_LEN = 48, 32, 24, 20
SYNR_EVENTS = 8
SYNR_LEN = 48 + 28 * SYNR_EVENTS

S_SYNS = struct.Struct("<4sBBBBI3i3hhHHBBBBBBH2BH")
S_SYNO = struct.Struct("<4sBBBBI3i3hBB")
S_SYNE = struct.Struct("<4sBBBBHHIII")
S_PING = struct.Struct("<4sBBBBIIHH")
S_HDR = struct.Struct("<4sBBBBIHHII16bBBBBBBBB")
S_EVT = struct.Struct("<HBBBBHHHIiii")
_U32 = struct.Struct("<I")
assert (S_SYNS.size, S_SYNO.size, S_SYNE.size, S_PING.size, S_HDR.size,
        S_EVT.size) == (48, 32, 24, 20, 48, 28)

# SYNS flags (plan 2.1 bit order) and power bits
F_LIVE, F_GROUNDED, F_TELEPORT, F_PAUSED = 0x01, 0x02, 0x04, 0x08
F_TALKING, F_IN_HURT, F_NO_CONTROL, F_STAR_LOCK = 0x10, 0x20, 0x40, 0x80
P_FIRE, P_WINGS, P_METAL, P_VANISH, P_MEGA = 0x01, 0x02, 0x04, 0x08, 0x10

# claims
C_READY, C_MARKER_BREAK, C_GRAB, C_HIT, C_HURT_SELF, C_PIT, C_QUIT = range(1, 8)
C_EAT, C_SPIT, C_SWALLOW, C_ESCAPE = 8, 9, 10, 11
CLAIM_NAMES = {1: "READY", 2: "MARKER_BREAK", 3: "GRAB", 4: "HIT",
               5: "HURT_SELF", 6: "PIT", 7: "QUIT", 8: "EAT", 9: "SPIT",
               10: "SWALLOW", 11: "ESCAPE"}
# events
(EV_START, EV_STAR_WAKE, EV_MARKER_BROKEN, EV_STAR_TAKEN, EV_DENY, EV_HIT,
 EV_CLASH, EV_SPILL, EV_MATCH_OVER, EV_LEAVE, EV_JOIN, EV_STAR_STATE,
 EV_ACCEPTED, EV_EATEN, EV_RELEASED) = range(1, 16)
# deny reasons
(D_STALE, D_GONE, D_LOST, D_RANGE, D_STATE, D_INVULN, D_MISMATCH,
 D_UNSUPPORTED, D_BAD, D_TOO_OLD, D_SELF) = range(1, 12)
# match-over reasons
O_TIME_UP, O_FIRST_TO_N, O_LAST_STANDING, O_EVERYONE_LEFT = 1, 2, 3, 4
# match states
MS_WAITING, MS_COUNTDOWN, MS_RUNNING, MS_OVER = 0, 1, 2, 3
# SYNR header flags
RF_SNAPSHOT, RF_DIRECTED, RF_RESEND = 0x01, 0x02, 0x04
# star instance states
ST_WAKING, ST_FREE = 1, 2
# spill causes
SP_HIT, SP_LEAVE, SP_HAZARD, SP_PIT = 1, 2, 3, 4
# leave reasons
L_QUIT, L_SILENT, L_DETACH = 1, 2, 3

STATE_NAMES = [
    "St_LedgeHang", "St_LedgeGrab", "St_CeilingGrate", "St_YoshiPower",
    "St_Swallow", "St_InYoshiMouth", "St_SmallLaunchUp", "St_Hurt",
    "St_HurtWater", "St_Electrocute", "St_BurnFire", "St_BurnLava",
    "St_DeadHit", "St_DeadPit", "St_Walk", "St_Wait", "St_Grabbed",
    "St_TurnAround", "St_Jump", "St_Fall", "St_Thrown", "St_SideFlip",
    "St_SlideKickRecover", "St_Fly", "St_NoControl", "St_Owl", "St_DebugFly",
    "St_WindCarry", "St_Balloon", "St_Teleport", "St_TornadoSpin", "St_Cannon",
    "St_Squish", "St_Shell", "St_StomachSlide", "St_ButtSlide",
    "St_DizzyStars", "St_HoldLight", "St_Bonk", "St_HoldHeavy", "St_WallSlide",
    "St_BowserEarthquake", "St_WallJump", "St_SlopeJump", "St_StuckInGround",
    "St_Land", "St_OnWall", "St_Spin", "St_Talk", "St_CrazedCrate",
    "St_WaitQuicksand", "St_LevelEnter", "St_JumpQuicksand", "St_Crouch",
    "St_Respawn", "St_Crawl", "St_BackFlip", "St_OpeningWakeUp", "St_LongJump",
    "St_PunchKick", "St_EndingFly", "St_GroundPound", "St_Dive", "St_Throw",
    "St_GrabBowserTail", "St_SwingPlayer", "St_SweepKick", "St_SlideKick",
    "St_CameraZoom", "St_Null", "St_Swim", "St_WaterJump",
    "St_MetalWaterGround", "St_MetalWaterWater", "St_Climb", "St_Headstand",
    "St_WallJump", "St_HeadstandJump",
]
N_STATES = len(STATE_NAMES)
ATTACK_STATES = ["St_YoshiPower", "St_Spin", "St_PunchKick", "St_GroundPound",
                 "St_Dive", "St_SweepKick", "St_SlideKick",
                 "St_SlideKickRecover", "St_Land", "St_SwingPlayer"]
DISABLED_STATES = ["St_Hurt", "St_HurtWater", "St_Electrocute", "St_BurnFire",
                   "St_BurnLava", "St_DeadHit", "St_DeadPit",
                   "St_InYoshiMouth", "St_NoControl", "St_Squish",
                   "St_DizzyStars", "St_Respawn", "St_Thrown", "St_Grabbed"]
CLASH_STATES = ["St_PunchKick"]
# The one attack state in which the ROM itself raises mIsNoControl: a tongue
# that has caught something (the St_Swallow / YoshiPower helpers call
# Player_DisableInteraction; no other state in ATTACK_STATES has a caller, see
# the relocation table of ov002 for 0x020c9e40). An attacker standing in it
# with that flag up is honest, so _usable() lets it claim (HITSPILL1).
NC_ATTACK_STATES = ["St_YoshiPower"]
ATTACK_NC_MAX_FRAMES = 45       # the exemption lasts at most 1.5 s of one run
SPEED_WALK = ["St_Walk", "St_Wait", "St_TurnAround", "St_Crouch", "St_Crawl",
              "St_PunchKick", "St_YoshiPower", "St_HoldLight", "St_HoldHeavy",
              "St_Throw", "St_Land", "St_Talk", "St_Spin"]
SPEED_SLIDE = ["St_StomachSlide", "St_ButtSlide", "St_SlideKick",
               "St_SlideKickRecover", "St_Dive"]
SPEED_EXEMPT = ["St_InYoshiMouth", "St_Respawn", "St_Thrown", "St_Teleport",
                "St_Cannon", "St_DeadPit", "St_SmallLaunchUp", "St_Swallow",
                "St_LevelEnter"]
SPEED_LIMITS = {"walk": 50, "slide": 125, "air": 90}
SPEED_AVG_LIMIT = 60
SPEED_AVG_FRAMES = 10

_IDX = dict((n, i) for i, n in reversed(list(enumerate(STATE_NAMES))))
_ALL_IDX = {}
for _i, _n in enumerate(STATE_NAMES):
    _ALL_IDX.setdefault(_n, set()).add(_i)


def _idx_set(names):
    out = set()
    for n in names:
        out |= _ALL_IDX[n]
    return frozenset(out)


ATTACK_IDX = _idx_set(ATTACK_STATES)
DISABLED_IDX = _idx_set(DISABLED_STATES)
CLASH_IDX = _idx_set(CLASH_STATES)
SPEED_CLASS = []
for _i, _n in enumerate(STATE_NAMES):
    if _n in SPEED_EXEMPT:
        SPEED_CLASS.append(None)
    elif _n in SPEED_SLIDE:
        SPEED_CLASS.append("slide")
    elif _n in SPEED_WALK:
        SPEED_CLASS.append("walk")
    else:
        SPEED_CLASS.append("air")
HURT_IDX = _idx_set(["St_Hurt", "St_HurtWater"])
YOSHI_POWER_IDX = _idx_set(["St_YoshiPower"])
NC_ATTACK_IDX = _idx_set(NC_ATTACK_STATES)

STAR_ORDERS = [
    [0, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0],
    [0, 1, 3, 2, 4, 0, 0, 0, 0, 0, 0, 0],
    [0, 2, 1, 3, 4, 0, 0, 0, 0, 0, 0, 0],
    [0, 2, 3, 1, 4, 0, 0, 0, 0, 0, 0, 0],
    [0, 3, 2, 1, 4, 0, 0, 0, 0, 0, 0, 0],
    [0, 3, 1, 2, 4, 0, 0, 0, 0, 0, 0, 0],
]


def env_flag(name):
    return os.environ.get(name, "") == "1"


def seq_next(seq):
    seq = (seq + 1) & 0xFFFF
    return 1 if seq == 0 else seq


def seq_after(a, b):
    """How many stream steps b is past a (0 when equal), in 1..65535."""
    return (b - a) % 0xFFFF if a else (b if b else 0)


# ---------------------------------------------------------------- tables


class Sample(object):
    __slots__ = ("frame", "rt", "x", "y", "z", "yaw", "state", "step",
                 "invuln", "power", "flags", "held", "eaten_by")


class Seat(object):
    """What the referee knows about one relay seat."""

    def __init__(self, seat, rt):
        self.seat = seat
        self.joined = rt
        self.verified = False       # echoed one of our pongs (no amplifier)
        self.base = 0               # this seat's own clock base (on_join)
        self.credit = 0             # bytes the referee may still send it
        self.pongs = deque(maxlen=RTT_KEEP)   # t_ref values we sent it
        self.hist = deque(maxlen=HISTORY_FRAMES)
        self.last_heard = rt
        self.ready = None           # the READY config tuple, once sent
        self.ack = 0
        self.ack_lag_since = None
        self.last_catchup = -1e9
        self.last_snapshot = -1e9
        self.rtts = deque(maxlen=RTT_KEEP)
        self.rtt_min = None
        self.done = {}              # claim_id -> (seq, record) of its verdict
        self.done_order = deque()
        self.pending_ids = set()
        self.clamps = 0
        self.claims = 0
        self.strikes = deque()
        self.strikes_total = 0
        self.last_hit = None        # referee stamp of the last HIT on me
        self.last_hit_by = None
        self.hit_check = None       # (deadline rt, verdict rt)
        self.sanction_until = -1e9
        self.eaten_by = None        # the referee's view (provisional, A5)
        self.eaten_at = -1e9        # referee ms of the EATEN that set it
        self.speed_win = deque(maxlen=SPEED_AVG_FRAMES)
        self.speed_logged = -1e9

    def cap(self):
        if self.rtt_min is None:
            return CAP_UNKNOWN_MS
        return min(HOLD_MAX_MS, self.rtt_min // 2 + CAP_MARGIN_MS)

    def one_way(self):
        return 50 if self.rtt_min is None else self.rtt_min / 2.0

    def newest(self):
        return self.hist[-1] if self.hist else None

    def sample_at(self, frame, tol=SAMPLE_TOLERANCE_FRAMES):
        """The newest sample at or before `frame`, else the oldest after it,
        within `tol` frames; None if there is none that close."""
        best = None
        for s in reversed(self.hist):
            if s.frame <= frame:
                best = s
                break
        if best is None:
            for s in self.hist:
                if s.frame > frame:
                    best = s
                    break
        if best is None or abs(best.frame - frame) > tol:
            return None
        return best

    def samples_between(self, lo, hi):
        return [s for s in self.hist if lo <= s.frame <= hi]


class Star(object):
    __slots__ = ("inst", "state", "marker", "star_id", "x", "y", "z", "known",
                 "owner", "since")

    def __init__(self, inst, state, marker, star_id, rt):
        self.inst = inst
        self.state = state
        self.marker = marker
        self.star_id = star_id
        self.x = self.y = self.z = 0
        self.known = False
        self.owner = 0xFF
        self.since = rt


class Claim(object):
    __slots__ = ("seat", "kind", "target", "cid", "aux", "stamp", "frame",
                 "ref_frame", "arrival", "resolve_at", "order")


class Referee(object):
    """One arena session's referee. `send(seat, data)` delivers a datagram to
    a seat through the relay; `log(msg)` writes one line."""

    def __init__(self, code, now, send, log, seed=None, kick=None):
        self.code = code
        self.created = now
        # the wire clock's random base (see NO AMPLIFIER); below 2^31 so a
        # session never wraps the u32 inside 24 days
        if seed is not None:
            self.base = (seed * 2654435761) & 0x7FFFFFFF
        else:
            self.base = int.from_bytes(os.urandom(4), "little") & 0x7FFFFFFF
        self.send = send
        self.log = log
        self.kick = kick            # callable(seat) or None: phase A logs only
        self.kick_on = kick is not None     # the relay passes one only when
                                            # SM64DS_REFEREE_KICK=1
        self.seed_env = seed
        self._base_rng = random.Random((seed or 0) ^ 0xBA5E5EED)
        self.seats = {}             # seat -> Seat (every seated member)
        self.order_counter = 0
        self.reset_match(self.rt(now))
        # counters
        self.n_states = 0
        self.n_bad_states = 0
        self.n_claims = 0
        self.n_verdicts = 0
        self.n_pings = 0
        self.n_tx = 0
        self.n_starved = 0
        self.n_violations = 0
        self.last_beat = -1e9

    # ---- clocks

    def rt(self, now):
        """Referee ms since this session was created (float)."""
        return (now - self.created) * 1000.0

    def ref_ms(self, now, seat=None):
        """The referee clock now: the session's wire base, or the u32 one
        seat sees (its own base) when `seat` is given."""
        if seat is None:
            return self.wire(self.rt(now))
        return self.seat_wire(seat, self.rt(now))

    def seat_wire(self, seat, rt):
        s = self.seats.get(seat)
        return ((s.base if s else self.base) + int(rt)) & 0xFFFFFFFF

    def seat_unwire(self, seat, v):
        s = self.seats.get(seat)
        d = (v - (s.base if s else self.base)) & 0xFFFFFFFF
        return d - 0x100000000 if d >= 0x80000000 else d

    def wire(self, rt):
        """Internal referee ms -> the u32 the wire carries."""
        return (self.base + int(rt)) & 0xFFFFFFFF

    def unwire(self, v):
        """A wire time -> internal referee ms (negative = before the
        session began)."""
        d = (v - self.base) & 0xFFFFFFFF
        return d - 0x100000000 if d >= 0x80000000 else d

    # ---- match lifecycle

    def reset_match(self, rt):
        self.state = MS_WAITING
        self.cfg = None
        self.first_ready = None
        self.seed = 0
        self.order_row = 0
        self.rng = random.Random(0)
        self.seat_rank = list(range(SEATS))
        self.t0 = 0
        self.match_ms = 0
        self.roster = set()
        self.had = 0
        self.scores = [0] * SEATS
        self.progress = 0
        self.stars = {}
        self.taken = {}             # inst -> (winner seat, stamp), recent
        self.taken_order = deque()
        self.broken = {}            # inst -> breaker seat
        self.next_inst = 1
        self.pending = []
        # A rematch in the same session starts a clean window but keeps the
        # sequence running, so a console from the last match applies the new
        # START in order instead of waiting for a SNAPSHOT (REVREFEREE1).
        self.events = deque(maxlen=RETAIN_EVENTS)   # (seq, record)
        self.newest_seq = getattr(self, "newest_seq", 0)
        self.unsent = 0
        self.over_at = None
        self.over_event = None
        for s in self.seats.values():
            s.ready = None
            s.done.clear()
            s.done_order.clear()
            s.pending_ids.clear()
            s.last_hit = None
            s.eaten_by = None

    def _start(self, rt):
        cfg = self.cfg
        if self.seed_env is not None:
            seed = self.seed_env & 0xFFFFFFFF
        else:
            seed = int.from_bytes(os.urandom(4), "little")
        self.seed = seed
        self.order_row = seed % 6
        self.rng = random.Random(seed)
        rank = list(range(SEATS))
        random.Random(seed ^ 0x5EED).shuffle(rank)
        self.seat_rank = rank       # tie order: lower rank wins
        self.roster = set(s.seat for s in self.seats.values()
                          if s.ready == cfg)
        self.had = len(self.roster)
        self.t0 = int(rt) + COUNTDOWN_MS
        self.match_ms = cfg[1] * 1000
        self.state = MS_COUNTDOWN
        for s in self.roster:
            self.seats[s].last_heard = max(self.seats[s].last_heard,
                                           float(self.t0))
        self._emit(EV_START, 0xFF, self.order_row, cfg[2], cfg[1],
                   self._mask(self.roster), cfg[5], self.t0, seed, cfg[0],
                   self._rules_word(cfg))
        self.log("arena %s START seats=%s map=%d time=%ds first_to=%d row=%d"
                 % (self.code, sorted(self.roster), cfg[0], cfg[1], cfg[2],
                    self.order_row))
        self._wake_next(rt)

    @staticmethod
    def _rules_word(cfg):
        return cfg[2] | (cfg[3] << 8) | (cfg[4] << 16) | (cfg[5] << 24)

    @staticmethod
    def _mask(seats):
        m = 0
        for s in seats:
            m |= 1 << s
        return m

    def _over(self, rt, reason, t=None):
        if self.state == MS_OVER:
            return
        # anything still waiting is decided now or never
        for c in list(self.pending):
            self._deny(c, D_STALE, rt)
        self.pending = []
        self.state = MS_OVER
        self.over_at = rt
        top = max([self.scores[s] for s in self.roster] or [0])
        winners = [s for s in sorted(self.roster) if self.scores[s] == top]
        mask = self._mask(winners)
        end = int(rt) if t is None else int(t)
        self.over_event = self._emit(EV_MATCH_OVER, 0xFF, reason,
                                     len(winners), mask, 0, 0,
                                     end, 0, 0, 0)
        self.log("arena %s MATCH_OVER reason=%d winners=%s scores=%s"
                 % (self.code, reason, winners,
                    [self.scores[s] for s in sorted(self.roster)]))

    # ---- events

    def _emit(self, etype, seat, a=0, b=0, c=0, d=0, e=0, t=0, x=0, y=0, z=0):
        self.newest_seq = seq_next(self.newest_seq)
        rec = S_EVT.pack(self.newest_seq, etype, seat & 0xFF, a & 0xFF,
                         b & 0xFF, c & 0xFFFF, d & 0xFFFF, e & 0xFFFF,
                         self.wire(t), _s32(x), _s32(y), _s32(z))
        self.events.append((self.newest_seq, rec))
        self.unsent += 1
        return (self.newest_seq, rec)

    def _snap_record(self, etype, seat, a=0, b=0, c=0, d=0, e=0, t=0, x=0,
                     y=0, z=0):
        return S_EVT.pack(0, etype, seat & 0xFF, a & 0xFF, b & 0xFF,
                          c & 0xFFFF, d & 0xFFFF, e & 0xFFFF, self.wire(t),
                          _s32(x), _s32(y), _s32(z))

    def _frame(self, rt, records, flags=0, part=0, seat=None):
        """One SYNR for `seat`: the header and up to 8 event records, newest
        first, every time rendered in that seat's own clock base (records
        are kept in the session base and re-based here)."""
        recs = list(records)[:SYNR_EVENTS]
        if seat is not None:
            out = []
            for rec in recs:
                t = self.unwire(_U32.unpack_from(rec, 12)[0])
                out.append(rec[:12] + _U32.pack(self.seat_wire(seat, t))
                           + rec[16:])
            recs = out
            wire = lambda v: self.seat_wire(seat, v)
        else:
            wire = self.wire
        hdr = S_HDR.pack(TAG_SYNR, 1, REF_MARK, len(recs), self.state,
                         wire(rt), self.newest_seq,
                         self._mask(self.roster),
                         wire(self.t0) if self.t0 else 0,
                         self.match_ms & 0xFFFFFFFF,
                         *[max(-128, min(127, v)) for v in self.scores],
                         self.progress & 0xFF,
                         (self.cfg[2] if self.cfg else 0) & 0xFF,
                         flags, part, self.order_row,
                         (self.cfg[0] if self.cfg else 0) & 0xFF,
                         min(255, len(self.stars)), 0)
        body = b"".join(recs) + b"\x00" * (28 * (SYNR_EVENTS - len(recs)))
        return hdr + body

    def _tx(self, seat, data):
        s = self.seats.get(seat)
        if s is None or not s.verified:
            return      # not proven to receive at its address: nothing
        if s.credit < len(data):
            self.n_starved += 1     # quiet seat: it hears the referee again
            return                  # once it sends (heartbeats re-carry all)
        s.credit -= len(data)
        self.n_tx += 1
        self.send(seat, data)

    @staticmethod
    def _earn(s, n):
        s.credit = min(AMP_CREDIT_CAP, s.credit + AMP_FACTOR * n)

    def _broadcast(self, rt, records, flags=0):
        for s in sorted(self.seats):
            if self.seats[s].verified:
                self._tx(s, self._frame(rt, records, flags, seat=s))

    def _window(self):
        return [rec for seq, rec in reversed(self.events)][:SYNR_EVENTS]

    def _flush(self, rt):
        """Send what was made since the last send: every new event rides in at
        least one datagram, newest first, 8 a datagram."""
        n = self.unsent
        self.unsent = 0
        if n <= 0:
            return
        evs = list(self.events)
        n = min(n, len(evs))
        new = evs[len(evs) - n:]
        # oldest chunk first so a console sees them in order
        while new:
            chunk = new[:SYNR_EVENTS]
            new = new[SYNR_EVENTS:]
            if not new:
                # the last datagram carries the 8 newest (redundancy)
                self._broadcast(rt, self._window())
            else:
                self._broadcast(rt, [rec for seq, rec in reversed(chunk)])
        self.last_beat = rt

    # ---- the relay's hooks

    def body_count(self):
        """Bodies the match is set up for (the first READY's capacity), or None
        while no READY has fixed it."""
        return self.cfg[4] if self.cfg is not None else None

    def on_join(self, seat, now):
        # Nothing is sent yet: the seat hears the referee once its clock ping
        # proves its address (on_ping).
        s = Seat(seat, self.rt(now))
        if self.seed_env is not None:
            s.base = self._base_rng.getrandbits(31)
        else:
            s.base = int.from_bytes(os.urandom(4), "little") & 0x7FFFFFFF
        self.seats[seat] = s

    def on_leave(self, seat, now, why=L_DETACH):
        rt = self.rt(now)
        if seat in self.roster:
            self._drop_from_match(seat, rt, why)
            self._flush(rt)
        self.seats.pop(seat, None)
        self.pending = [c for c in self.pending if c.seat != seat]

    def on_violation(self, seat, why, now):
        self.n_violations += 1
        self._strike(seat, why, self.rt(now))

    def check_state(self, seat, data):
        """Range-check a SYNS before the relay forwards it. False = drop it."""
        try:
            f = S_SYNS.unpack(data)
        except struct.error:
            return False
        # f: 0 tag 1 ver 2 seat 3 flags 4 char 5 frame 6-8 pos 9-11 vel
        # 12 yaw 13 anim 14 cursor 15 state 16 step 17 invuln 18 power
        # 19 held 20 eaten_by 21 ack 22-23 sfx 24 reserved
        ok = (f[1] == 1 and abs(f[6]) <= POS_LIMIT and abs(f[7]) <= POS_LIMIT
              and abs(f[8]) <= POS_LIMIT and f[13] < 194
              and f[15] < N_STATES and f[17] <= 36
              and (f[19] <= SEATS or f[19] >= 0x20) and f[20] <= SEATS)
        if not ok:
            self.n_bad_states += 1
        return ok

    def on_state(self, seat, data, now):
        s = self.seats.get(seat)
        if s is None:
            return
        rt = self.rt(now)
        self.n_states += 1
        self._earn(s, len(data))
        f = S_SYNS.unpack(data)
        frame = f[5]
        s.last_heard = rt
        s.ack = f[21]
        last = s.newest()
        if last is not None:
            if frame <= last.frame:
                if last.frame - frame > RESET_BACK_FRAMES:
                    s.hist.clear()          # the console restarted
                    s.speed_win.clear()
                    last = None
                else:
                    return                  # late / duplicate: forward only
        smp = Sample()
        smp.frame = frame
        smp.rt = rt
        smp.x, smp.y, smp.z = f[6], f[7], f[8]
        smp.yaw = f[12]
        smp.state = f[15]
        smp.step = f[16]
        smp.invuln = f[17]
        smp.power = f[18]
        smp.flags = f[3]
        smp.held = f[19]
        smp.eaten_by = f[20]
        s.hist.append(smp)
        if last is not None and self.state == MS_RUNNING and seat in self.roster:
            self._speed_check(s, last, smp, rt)

    def check_object(self, seat, data):
        try:
            f = S_SYNO.unpack(data)
        except struct.error:
            return False
        # f: 0 tag 1 ver 2 seat 3 class 4 id 5 frame 6-8 pos 9-11 vel
        # 12 state 13 flags
        return (f[1] == 1 and f[3] == 1 and abs(f[6]) <= POS_LIMIT
                and abs(f[7]) <= POS_LIMIT and abs(f[8]) <= POS_LIMIT
                and f[12] in (1, 2, 3))

    def on_object(self, seat, data, now):
        f = S_SYNO.unpack(data)
        so = self.seats.get(seat)
        if so is not None:
            self._earn(so, len(data))
        star = self.stars.get(f[4])
        if star is None or star.state != ST_FREE:
            return
        # only the console that launched it speaks for its flight
        if star.owner != 0xFF and star.owner != seat:
            return
        star.x, star.y, star.z = f[6], f[7], f[8]
        star.known = True
        s = self.seats.get(seat)
        if s is not None:
            s.last_heard = self.rt(now)

    def on_ping(self, seat, data, now):
        """The referee clock pong for a 20-byte 'SYNP' to 0xFE, or None."""
        s = self.seats.get(seat)
        if s is None:
            return None
        f = S_PING.unpack(data)
        if f[1] != 2 or f[3] != REF_MARK:
            return None
        rt = self.rt(now)
        self.n_pings += 1
        s.last_heard = rt
        self._earn(s, len(data))
        echo, held = f[6], f[7]
        if echo and echo in s.pongs:
            if not s.verified:
                s.verified = True
                s.credit = min(AMP_CREDIT_CAP, s.credit + SYNR_LEN)
                # proven: it hears the current state at once
                self._tx(seat, self._frame(rt, self._window(), seat=seat))
            if held != 0xFFFF:
                rtt = int(rt) - self.seat_unwire(seat, echo) - held
                if 0 <= rtt <= 5000:
                    s.rtts.append(rtt)
                    s.rtt_min = min(s.rtts)
        t_ref = self.ref_ms(now, seat)
        s.pongs.append(t_ref)
        return S_PING.pack(TAG_SYNQ, 2, REF_MARK, seat, 0, f[5], t_ref,
                           0xFFFF if s.rtt_min is None else
                           min(0xFFFE, s.rtt_min), s.cap())

    def on_claim(self, seat, data, now):
        s = self.seats.get(seat)
        if s is None:
            return
        rt = self.rt(now)
        self._earn(s, len(data))
        f = S_SYNE.unpack(data)
        if f[1] != 1:
            return
        kind, target, cid, aux, stamp, frame, ref_frame = (
            f[3], f[4], f[5], f[6], f[7], f[8], f[9])
        if not s.verified:
            # Not yet proven to receive at its address (see NO AMPLIFIER):
            # nothing it claims counts. A real console verifies within one
            # ping interval and keeps resending its claim until named.
            return
        if kind not in CLAIM_NAMES or cid == 0:
            self._strike(seat, "bad claim", rt)
            return
        s.last_heard = rt
        self.n_claims += 1
        # a resend: answer with the verdict again, never decide twice
        done = s.done.get(cid)
        if done is not None:
            self._resend_verdict(seat, done, rt)
            return
        if cid in s.pending_ids:
            return
        if kind == C_READY:
            self._ready(s, cid, target, aux, stamp, frame, rt)
            self._flush(rt)
            return
        c = Claim()
        c.seat, c.kind, c.target, c.cid, c.aux = seat, kind, target, cid, aux
        c.frame, c.ref_frame, c.arrival = frame, ref_frame, rt
        self.order_counter += 1
        c.order = self.order_counter
        live = self.state == MS_RUNNING or (
            self.state == MS_COUNTDOWN and kind == C_QUIT)
        if not live or seat not in self.roster:
            self._deny(c, D_STALE, rt)
            self._flush(rt)
            return
        # clamp the stamp into [arrival - cap, arrival]
        cap = s.cap()
        st = self.seat_unwire(seat, stamp)
        if st > rt + FUTURE_SLACK_MS:
            st = int(rt)
            s.clamps += 1
        elif st < rt - cap:
            st = int(rt - cap)
            s.clamps += 1
        c.stamp = st
        s.claims += 1
        if kind in (C_HURT_SELF, C_PIT, C_QUIT, C_SPIT, C_SWALLOW, C_ESCAPE):
            # nobody contends these: decide now
            self._decide(c, rt)
            self._flush(rt)
            return
        if len(s.pending_ids) >= PENDING_PER_SEAT:
            self._deny(c, D_STALE, rt)
            self._flush(rt)
            return
        others = [self.seats[o].cap() for o in self.roster
                  if o != seat and o in self.seats]
        c.resolve_at = c.stamp + min(HOLD_MAX_MS, max(others or [0]))
        s.pending_ids.add(cid)
        self.pending.append(c)

    # ---- the tick

    def tick(self, now):
        rt = self.rt(now)
        if self.state == MS_WAITING:
            self._maybe_start(rt)
        elif self.state == MS_COUNTDOWN and rt >= self.t0:
            self.state = MS_RUNNING
        if self.pending:
            self._resolve(rt)
        if self.state == MS_RUNNING:
            self._presence(rt)
            self._hit_checks(rt)
            if self.match_ms and self.state == MS_RUNNING:
                end = self.t0 + self.match_ms
                if rt >= end + HOLD_MAX_MS:
                    self._resolve(rt)
                    self._over(rt, O_TIME_UP, end)
        self._flush(rt)
        self._catchup(rt)
        if rt - self.last_beat >= 1000.0 / HEARTBEAT_HZ:
            self.last_beat = rt
            if self.seats:
                self._broadcast(rt, self._window())

    def _maybe_start(self, rt):
        if self.cfg is None:
            return
        ready = [s for s in self.seats.values() if s.ready == self.cfg]
        cap = self.cfg[4]
        if len(ready) >= cap or (len(ready) >= 2 and self.first_ready
                                 is not None
                                 and rt - self.first_ready >= START_WAIT_MS):
            self._start(rt)
            self._flush(rt)

    def _resolve(self, rt):
        self.pending.sort(key=lambda c: (c.stamp, self.scores[c.seat],
                                         self.seat_rank[c.seat], c.order))
        while self.pending and self.pending[0].resolve_at <= rt:
            c = self.pending.pop(0)
            s = self.seats.get(c.seat)
            if s is not None:
                s.pending_ids.discard(c.cid)
            if self.state != MS_RUNNING:
                self._deny(c, D_STALE, rt)
                continue
            if c.seat not in self.roster:
                continue
            self._decide(c, rt)

    def _presence(self, rt):
        for seat in sorted(self.roster):
            s = self.seats.get(seat)
            if s is None:
                continue
            if rt - s.last_heard > PRESENCE_MS:
                self._drop_from_match(seat, rt, L_SILENT)
                if self.state != MS_RUNNING:
                    return

    def _hit_checks(self, rt):
        for s in list(self.seats.values()):     # a kick may drop a seat
            if s.hit_check is None or rt < s.hit_check[0]:
                continue
            verdict_rt = s.hit_check[1]
            s.hit_check = None
            took = any((x.flags & F_IN_HURT) or x.state in HURT_IDX
                       or x.invuln > 0 for x in s.hist if x.rt > verdict_rt)
            if not took and any(x.rt > verdict_rt for x in s.hist):
                self._strike(s.seat, "ignored a HIT verdict", rt)

    def _catchup(self, rt):
        """A seat whose ack trails what the 8-event window still carries gets
        the missed events directed to it (retained ones), or a SNAPSHOT."""
        if not self.newest_seq:
            return
        oldest_in_window = None
        win = list(self.events)[-SYNR_EVENTS:]
        if win:
            oldest_in_window = win[0][0]
        for seat, s in self.seats.items():
            if seat not in self.roster:
                continue
            behind = seq_after(s.ack, self.newest_seq)
            if behind <= SYNR_EVENTS or oldest_in_window is None:
                s.ack_lag_since = None
                continue
            if s.ack_lag_since is None:
                s.ack_lag_since = rt
            if rt - s.last_catchup < CATCHUP_EVERY_MS:
                continue
            s.last_catchup = rt
            retained = [(q, r) for q, r in self.events
                        if s.ack and 0 < seq_after(s.ack, q) <= behind]
            if s.ack and len(retained) == behind:
                for i in range(0, len(retained), SYNR_EVENTS):
                    chunk = retained[i:i + SYNR_EVENTS]
                    self._tx(seat, self._frame(
                        rt, [r for q, r in reversed(chunk)],
                        RF_DIRECTED | RF_RESEND, seat=seat))
            elif rt - s.ack_lag_since >= SNAPSHOT_LAG_MS or not s.ack:
                if rt - s.last_snapshot >= SNAPSHOT_LAG_MS:
                    self._snapshot(seat, rt)

    def _snapshot(self, seat, rt):
        s = self.seats.get(seat)
        if s is None:
            return
        s.last_snapshot = rt
        recs = []
        if self.state != MS_WAITING and self.cfg is not None:
            cfg = self.cfg
            recs.append(self._snap_record(
                EV_START, 0xFF, self.order_row, cfg[2], cfg[1],
                self._mask(self.roster), cfg[5], self.t0, self.seed, cfg[0],
                self._rules_word(cfg)))
        for inst in sorted(self.stars):
            st = self.stars[inst]
            recs.append(self._snap_record(
                EV_STAR_STATE, st.owner, inst, st.state, st.marker,
                st.star_id, 0, int(st.since), st.x, st.y, st.z))
        if self.over_event is not None:
            q, rec = self.over_event
            recs.append(rec[:0] + struct.pack("<H", 0) + rec[2:])
        parts = [recs[i:i + SYNR_EVENTS]
                 for i in range(0, len(recs), SYNR_EVENTS)] or [[]]
        n = min(15, len(parts))
        for i, chunk in enumerate(parts[:n]):
            self._tx(seat, self._frame(rt, chunk, RF_SNAPSHOT | RF_DIRECTED,
                                       ((i & 0xF) << 4) | n, seat=seat))

    # ---- READY

    def _ready(self, s, cid, target, aux, rules, digest, rt):
        first_to_n = rules & 0xFF
        mode = (rules >> 8) & 0xFF
        capacity = (rules >> 16) & 0xFF
        markers = (rules >> 24) & 0xFF
        c = Claim()
        c.seat, c.kind, c.target, c.cid, c.aux = s.seat, C_READY, target, cid, aux
        c.stamp = c.frame = c.ref_frame = 0
        c.arrival = rt
        if (target > 15 or aux > 3600 or first_to_n > 99 or not
                1 <= capacity <= SEATS or not 1 <= markers <= 8):
            self._deny(c, D_BAD, rt)
            return
        if mode != 0:
            self._deny(c, D_UNSUPPORTED, rt)
            return
        cfg = (target, aux, first_to_n, mode, capacity, markers, digest)
        if self.state == MS_OVER and cfg != self.cfg:
            if self.over_at is not None and rt - self.over_at < OVER_LINGER_MS:
                # the last match's MATCH_OVER keeps riding the window for
                # OVER_LINGER_MS (a console that lost it still gets it); this
                # READY gets no verdict yet, so its console keeps resending
                return
            self.reset_match(rt)        # a new match in the same session
        if self.cfg is None:
            self.cfg = cfg
            self.first_ready = rt
        if cfg != self.cfg:
            self._deny(c, D_MISMATCH, rt)
            return
        was = s.ready
        s.ready = cfg
        s.last_heard = rt
        self._accept(c, rt)
        if self.state == MS_WAITING:
            if was != cfg:
                self._emit(EV_JOIN, s.seat, 0, 0, 0, 0, 0, int(rt))
        elif self.state in (MS_COUNTDOWN, MS_RUNNING) and \
                s.seat not in self.roster:
            self.roster.add(s.seat)
            self.had = max(self.had, len(self.roster))
            self.scores[s.seat] = 0
            self._emit(EV_JOIN, s.seat, 1, 0, 0, 0, 0, int(rt))
            self._snapshot(s.seat, rt)

    # ---- verdicts

    def _record(self, c, ev):
        s = self.seats.get(c.seat)
        if s is None:
            return
        s.done[c.cid] = ev
        s.done_order.append(c.cid)
        while len(s.done_order) > DONE_PER_SEAT:
            s.done.pop(s.done_order.popleft(), None)
        self.n_verdicts += 1

    # DENY and ACCEPTED concern the claimant alone, so they are DIRECTED
    # records (seq 0) sent to it only, never part of the stream every seat
    # hears: a seat spamming bad claims cannot crowd the other seats' event
    # window or bandwidth. A resend of the claim gets the same record again.

    def _direct(self, c, rec, rt):
        self._record(c, (0, rec))
        self._tx(c.seat, self._frame(rt, [rec], RF_DIRECTED, seat=c.seat))

    def _deny(self, c, reason, rt, winner=0xFF):
        rec = self._snap_record(EV_DENY, c.seat, c.kind, reason, c.cid,
                                winner, c.target, int(rt))
        self._direct(c, rec, rt)
        s = self.seats.get(c.seat)
        if s is not None:
            s.pending_ids.discard(c.cid)

    def _accept(self, c, rt, spilled=0):
        rec = self._snap_record(EV_ACCEPTED, c.seat, c.kind, 0, c.cid,
                                spilled, 0, int(rt))
        self._direct(c, rec, rt)

    def _resend_verdict(self, seat, done, rt):
        seq, rec = done
        if seq and any(q == seq for q, r in list(self.events)[-SYNR_EVENTS:]):
            return      # still in the window every SYNR carries
        self._tx(seat, self._frame(rt, [rec], RF_DIRECTED | RF_RESEND,
                                   seat=seat))

    def _decide(self, c, rt):
        k = c.kind
        if (self.match_ms and k != C_QUIT
                and c.stamp >= self.t0 + self.match_ms):
            # touched after the clock ran out: the match ends at T0 +
            # match_ms even though claims are still decided during the hold
            self._deny(c, D_STALE, rt)
            return
        if k == C_GRAB:
            self._grab(c, rt)
        elif k == C_MARKER_BREAK:
            self._marker(c, rt)
        elif k == C_HIT:
            self._hit(c, rt)
        elif k == C_HURT_SELF:
            inst = self._spill(c.seat, SP_HAZARD, rt)
            self.seats[c.seat].sanction_until = rt + 2000
            self._accept(c, rt, inst)
        elif k == C_PIT:
            inst = self._spill(c.seat, SP_PIT, rt) if c.aux & 1 else 0
            self.seats[c.seat].sanction_until = rt + 3000
            self._accept(c, rt, inst)
        elif k == C_QUIT:
            self._accept(c, rt)
            self._drop_from_match(c.seat, rt, L_QUIT)
        elif k == C_EAT:
            self._eat(c, rt)
        elif k in (C_SPIT, C_SWALLOW, C_ESCAPE):
            self._release(c, rt)
        else:
            self._deny(c, D_UNSUPPORTED, rt)

    @staticmethod
    def _attack_nc_ok(s, smp):
        """May an ATTACKER's sample carry the no_control flag?  Only while the
        ROM's own tongue holds it up (HITSPILL1): the sample is in an
        NC_ATTACK state, and the whole trailing run of no_control samples
        ending at it is in such states, is not hurt, began after a sample of
        the same seat that had the flag DOWN (a flag raised by a talk / cutscene
        / frozen state and carried into an attack state never qualifies), and
        is at most ATTACK_NC_MAX_FRAMES long (a state parked in the attack
        state with the flag up stops qualifying).  No pre-run sample on record
        (the first thing a seat ever sent) means no exemption.  The flag is the
        client's own report and a liar can leave it down, so this only
        decides which honest-looking samples a verdict may accept: it grants
        a cheater nothing it could not already do."""
        if smp.state not in NC_ATTACK_IDX or smp.state in DISABLED_IDX:
            return False
        hist = s.hist
        j = len(hist) - 1
        while j >= 0 and hist[j] is not smp:
            j -= 1
        if j < 0:
            return False
        first = smp
        while j >= 0 and hist[j].flags & F_NO_CONTROL:
            x = hist[j]
            if x.state not in NC_ATTACK_IDX or x.flags & F_IN_HURT:
                return False
            first = x
            j -= 1
        if j < 0:
            return False
        return smp.frame - first.frame <= ATTACK_NC_MAX_FRAMES

    def _usable(self, s, frame, attack=False):
        """The claimant's own sample at `frame`, and whether it may act.
        attack=True: the sample is an attacker's, which may carry no_control
        for the length of a caught tongue (see _attack_nc_ok)."""
        smp = s.sample_at(frame)
        if smp is None:
            return None, D_STALE
        bad = F_IN_HURT | F_NO_CONTROL
        if attack and smp.flags & F_NO_CONTROL and self._attack_nc_ok(s, smp):
            bad = F_IN_HURT
        if (smp.state in DISABLED_IDX or smp.flags & bad
                or smp.eaten_by or s.eaten_by is not None):
            return smp, D_STATE
        return smp, 0

    def _grab(self, c, rt):
        star = self.stars.get(c.target)
        if star is None or star.state != ST_FREE:
            t = self.taken.get(c.target)
            if t is not None:
                self._deny(c, D_LOST, rt, t[0])
            else:
                self._deny(c, D_GONE, rt)
            return
        s = self.seats[c.seat]
        smp, why = self._usable(s, c.frame)
        if why:
            self._deny(c, why, rt)
            return
        if star.known and _dist3(smp, star) > GRAB_R * FIX:
            self._deny(c, D_RANGE, rt)
            return
        del self.stars[c.target]
        self._taken(c.target, c.seat, c.stamp)
        self.scores[c.seat] = min(127, self.scores[c.seat] + 1)
        self.progress = (self.progress + 1) & 0xFF
        ev = self._emit(EV_STAR_TAKEN, c.seat, c.target, self.scores[c.seat],
                        c.cid, 0, self.progress, c.stamp, smp.x, smp.y, smp.z)
        self._record(c, ev)
        self._wake_next(rt)
        n = self.cfg[2] if self.cfg else 0
        if n and self.scores[c.seat] >= n:
            self._over(rt, O_FIRST_TO_N, c.stamp)

    def _taken(self, inst, seat, stamp):
        self.taken[inst] = (seat, stamp)
        self.taken_order.append(inst)
        while len(self.taken_order) > 32:
            old = self.taken_order.popleft()
            if old not in self.taken_order:
                self.taken.pop(old, None)

    def _marker(self, c, rt):
        star = self.stars.get(c.target)
        if star is None:
            self._deny(c, D_GONE, rt)
            return
        if star.state != ST_WAKING:
            b = self.broken.get(c.target)
            if b is not None and b != c.seat:
                self._deny(c, D_LOST, rt, b)
            else:
                self._deny(c, D_GONE, rt)
            return
        s = self.seats[c.seat]
        smp, why = self._usable(s, c.frame)
        if why:
            self._deny(c, why, rt)
            return
        if star.known and _dist3(smp, star) > MARKER_R * FIX:
            self._deny(c, D_RANGE, rt)
            return
        star.state = ST_FREE
        star.owner = c.seat
        star.x, star.y, star.z = smp.x, smp.y, smp.z
        star.known = False      # it pops and flies: SYNO frames place it
        star.since = rt
        self.broken[c.target] = c.seat
        ev = self._emit(EV_MARKER_BROKEN, c.seat, c.target, star.star_id,
                        c.cid, c.aux, 0, c.stamp, smp.x, smp.y, smp.z)
        self._record(c, ev)

    def _hit(self, c, rt, eat=False):
        victim = c.target
        if victim == c.seat or victim not in self.roster or \
                victim not in self.seats:
            self._deny(c, D_SELF, rt)
            return
        a = self.seats[c.seat]
        v = self.seats[victim]
        att = a.sample_at(c.frame)
        if att is None:
            self._deny(c, D_STALE, rt)
            return
        trade = False
        if a.last_hit is not None:
            dt = c.stamp - a.last_hit
            if abs(dt) <= TRADE_MS and a.last_hit_by == victim:
                trade = True
            elif 0 <= dt < HIT_IMMUNE_MS:
                self._deny(c, D_STATE, rt)
                return
        if not trade:
            smp, why = self._usable(a, c.frame, attack=True)
            if why:
                self._deny(c, why, rt)
                return
        want = YOSHI_POWER_IDX if eat else ATTACK_IDX
        win = a.samples_between(c.frame - ATTACK_LOOKBACK_FRAMES, c.frame)
        if not any(x.state in want for x in win):
            self._deny(c, D_STATE, rt)
            return
        vs = v.sample_at(c.ref_frame)
        if vs is None:
            self._deny(c, D_STALE, rt)
            return
        # how long before the touch the referee itself received the frame
        # the attacker drew: the victim's self-reported RTT plays no part
        # (REVREFEREE1 F1: a victim inflating it voided every hit on it)
        age = c.stamp - vs.rt
        if age > COMP_MAX_MS + FRAME_MS:
            self._deny(c, D_TOO_OLD, rt)
            return
        dh = math.hypot(att.x - vs.x, att.z - vs.z)
        if dh > REACH_H * FIX or abs(att.y - vs.y) > REACH_V * FIX:
            self._deny(c, D_RANGE, rt)
            return
        if (vs.state in DISABLED_IDX or vs.flags & (F_IN_HURT | F_NO_CONTROL)
                or vs.invuln > 0 or vs.power & (P_MEGA | P_VANISH | P_METAL)
                or vs.eaten_by or v.eaten_by is not None):
            self._deny(c, D_INVULN, rt)
            return
        if v.last_hit is not None and c.stamp - v.last_hit < HIT_IMMUNE_MS:
            self._deny(c, D_INVULN, rt)
            return
        if eat:
            v.eaten_by = c.seat
            v.eaten_at = rt
            v.sanction_until = rt + 60000
            ev = self._emit(EV_EATEN, victim, c.seat, 0, c.cid, 0, 0, c.stamp,
                            att.x, att.y, att.z)
            self._record(c, ev)
            return
        if vs.state in CLASH_IDX and _facing(vs, att):
            ev = self._emit(EV_CLASH, victim, c.seat, 0, c.cid, 0, 0, c.stamp,
                            att.x, att.y, att.z)
            self._record(c, ev)
            return
        v.last_hit = c.stamp
        v.last_hit_by = c.seat
        v.sanction_until = rt + 2000
        v.hit_check = (rt + v.one_way() * 2 + INVULN_CHEAT_MS, rt)
        inst = self._spill(victim, SP_HIT, rt, emit=False)
        ev = self._emit(EV_HIT, victim, c.seat, 1 if trade else 0, c.cid,
                        c.aux, inst, c.stamp, att.x, att.y, att.z)
        self._record(c, ev)
        if inst:
            self._emit_spill(inst, victim, SP_HIT, rt)

    def _eat(self, c, rt):
        if self.seats[c.seat].eaten_by is not None:
            self._deny(c, D_STATE, rt)
            return
        if any(s.eaten_by == c.seat for s in self.seats.values()):
            self._deny(c, D_STATE, rt)      # one mouthful at a time
            return
        self._hit(c, rt, eat=True)

    def _release(self, c, rt):
        if c.kind == C_ESCAPE:
            victim, eater = c.seat, c.target
        else:
            victim, eater = c.target, c.seat
        v = self.seats.get(victim)
        if v is None or v.eaten_by != eater:
            self._deny(c, D_GONE, rt)
            return
        if c.kind == C_ESCAPE and rt - v.eaten_at < ESCAPE_MIN_MS:
            self._deny(c, D_STATE, rt)      # too soon after the EATEN to be a real escape
            return
        v.eaten_by = None
        v.sanction_until = rt + 2000
        how = {C_SPIT: 1, C_SWALLOW: 2, C_ESCAPE: 3}[c.kind]
        ev = self._emit(EV_RELEASED, victim, eater, how, c.cid, 0, 0, int(rt))
        self._record(c, ev)

    # ---- stars

    def _alloc_inst(self):
        for _ in range(255):
            inst = self.next_inst
            self.next_inst = self.next_inst % 255 + 1
            if inst not in self.stars:
                self.taken.pop(inst, None)
                self.broken.pop(inst, None)
                return inst
        return 0

    def _wake_next(self, rt):
        if self.cfg is None:
            return
        p = self.progress
        star_id = STAR_ORDERS[self.order_row][p] if p < 12 else 0
        if star_id >= self.cfg[5]:
            return          # no marker with that id on this map (ROM: none)
        if any(st.star_id == star_id and st.state == ST_WAKING
               for st in self.stars.values()):
            return
        inst = self._alloc_inst()
        if not inst:
            return
        self.stars[inst] = Star(inst, ST_WAKING, star_id, star_id, rt)
        self._emit(EV_STAR_WAKE, 0xFF, inst, star_id, p, 0, 0, int(rt))

    def _free_marker(self):
        busy = set(st.marker for st in self.stars.values())
        n = self.cfg[5] if self.cfg else 1
        free = [m for m in range(n) if m not in busy]
        return self.rng.choice(free) if free else self.rng.randrange(n)

    def _spill(self, seat, cause, rt, emit=True):
        """One star leaves `seat` if it has any: a new FREE instance flying
        from the seat's position to a free marker. Returns the instance."""
        if self.scores[seat] <= 0:
            return 0
        inst = self._alloc_inst()
        if not inst:
            return 0
        self.scores[seat] -= 1
        st = Star(inst, ST_FREE, self._free_marker(), 0xFF, rt)
        s = self.seats.get(seat)
        last = s.newest() if s is not None else None
        if last is not None:
            st.x, st.y, st.z = last.x, last.y, last.z
        st.owner = seat if cause != SP_LEAVE else 0xFF
        st.known = False        # in flight to its marker: no range check
        self.stars[inst] = st
        if emit:
            self._emit_spill(inst, seat, cause, rt)
        return inst

    def _emit_spill(self, inst, seat, cause, rt):
        st = self.stars[inst]
        self._emit(EV_SPILL, seat, inst, st.marker, cause, self.scores[seat],
                   0, int(rt), st.x, st.y, st.z)

    # ---- leaving

    def _drop_from_match(self, seat, rt, why):
        if seat not in self.roster:
            return
        n = 0
        if self.state in (MS_COUNTDOWN, MS_RUNNING):
            while self.scores[seat] > 0:
                if not self._spill(seat, SP_LEAVE, rt):
                    self.scores[seat] = 0
                    break
                n += 1
        self.roster.discard(seat)
        self._emit(EV_LEAVE, seat, why, 0, 0, n, 0, int(rt))
        for s in self.seats.values():
            if s.eaten_by == seat:
                s.eaten_by = None
                self._emit(EV_RELEASED, s.seat, seat, 4, 0, 0, 0, int(rt))
        me = self.seats.get(seat)
        if me is not None:
            me.ready = None
            if me.eaten_by is not None:
                eater = me.eaten_by
                me.eaten_by = None
                self._emit(EV_RELEASED, seat, eater, 4, 0, 0, 0, int(rt))
        self.pending = [c for c in self.pending if c.seat != seat]
        self.log("arena %s seat %d left the match (%d), %d stars spilled"
                 % (self.code, seat, why, n))
        if self.state == MS_RUNNING or self.state == MS_COUNTDOWN:
            if not self.roster:
                self._over(rt, O_EVERYONE_LEFT)
            elif self.had >= 2 and len(self.roster) == 1:
                self._over(rt, O_LAST_STANDING)

    # ---- strikes (phase A: logged; kick is phase B)

    def _strike(self, seat, why, rt):
        s = self.seats.get(seat)
        if s is None:
            return
        s.strikes.append(rt)
        s.strikes_total += 1
        while s.strikes and rt - s.strikes[0] > STRIKE_WINDOW_MS:
            s.strikes.popleft()
        if rt - s.speed_logged >= 1000:
            s.speed_logged = rt
            self.log("arena %s strike seat %d: %s (%d in 10 s, %d total)"
                     % (self.code, seat, why, len(s.strikes),
                        s.strikes_total))
        if self.kick_on and len(s.strikes) >= STRIKES_TO_KICK:
            self.log("arena %s KICK seat %d" % (self.code, seat))
            self.kick(seat)

    def _speed_check(self, s, a, b, rt):
        df = b.frame - a.frame
        if df <= 0 or df > 30:
            s.speed_win.clear()
            return
        if rt < s.sanction_until or b.eaten_by or a.eaten_by:
            s.speed_win.clear()
            return
        step = math.hypot(b.x - a.x, b.z - a.z) / FIX / df
        if b.flags & F_TELEPORT:
            self._strike(s.seat, "unsanctioned teleport", rt)
            s.speed_win.clear()
            return
        cls = SPEED_CLASS[b.state]
        if cls is None:
            s.speed_win.clear()
            return
        if step > SPEED_LIMITS[cls]:
            self._strike(s.seat, "speed %.0f/frame in %s"
                         % (step, STATE_NAMES[b.state]), rt)
        s.speed_win.append((step, cls))
        if len(s.speed_win) == SPEED_AVG_FRAMES and \
                all(k != "slide" for st, k in s.speed_win):
            avg = sum(st for st, k in s.speed_win) / SPEED_AVG_FRAMES
            if avg > SPEED_AVG_LIMIT:
                self._strike(s.seat, "average speed %.0f/frame" % avg, rt)
                s.speed_win.clear()

    # ---- introspection (tests, stats)

    def invariants(self):
        """Bounded tables and a sane ledger; [] when every check holds."""
        bad = []
        if any(not -128 <= v <= 127 for v in self.scores):
            bad.append("score range")
        if any(v < 0 for v in self.scores):
            bad.append("negative score")
        if not self.roster <= set(range(SEATS)):
            bad.append("roster")
        if len(self.stars) > 255:
            bad.append("stars")
        if len(self.events) > RETAIN_EVENTS:
            bad.append("events")
        for s in self.seats.values():
            if len(s.hist) > HISTORY_FRAMES or len(s.done) > DONE_PER_SEAT:
                bad.append("seat tables")
            if len(s.pending_ids) > PENDING_PER_SEAT:
                bad.append("pending")
        if len(self.pending) > PENDING_PER_SEAT * SEATS:
            bad.append("pending queue")
        return bad

    def stats(self):
        return {"state": self.state, "seats": len(self.seats),
                "roster": len(self.roster), "states": self.n_states,
                "bad_states": self.n_bad_states, "claims": self.n_claims,
                "verdicts": self.n_verdicts, "pings": self.n_pings,
                "tx": self.n_tx, "starved": self.n_starved,
                "violations": self.n_violations}


def _s32(v):
    v = int(v)
    if v > 0x7FFFFFFF or v < -0x80000000:
        v = ((v + 0x80000000) & 0xFFFFFFFF) - 0x80000000
    return v


def _dist3(a, b):
    return math.sqrt((a.x - b.x) ** 2 + (a.y - b.y) ** 2 + (a.z - b.z) ** 2)


def _facing(victim, attacker):
    """AngleDiff(HorzAngle(victim -> attacker), victim yaw) < 0x4000."""
    ang = int(round(math.atan2(attacker.x - victim.x, attacker.z - victim.z)
                    * 32768.0 / math.pi)) & 0xFFFF
    diff = (ang - (victim.yaw & 0xFFFF)) & 0xFFFF
    if diff >= 0x8000:
        diff = 0x10000 - diff
    return diff < 0x4000
