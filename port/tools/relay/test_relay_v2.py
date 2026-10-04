#!/usr/bin/env python3
"""Relay version 2 acceptance suite (run mp1, lane RELAY2).

Version 2 is NETARCH1 design section 3: HELLO/ACK v2, star and hub sessions,
seats stamped by the relay, and a kind table that is the whole of what a
version 2 session forwards. test_client.py stays the version 1 suite; this
file proves version 2 and that version 1 did not move.

  python test_relay_v2.py            both parts below
  python test_relay_v2.py unit       in-process only (a second or two)
  python test_relay_v2.py socket     the relay as a subprocess over real UDP

UNIT: relay.py is imported and driven through RelayProtocol.on_datagram with
a recording transport and an explicit clock, so every drop reason, every
refusal status and every route is checked exactly, with no timing luck. A
fuzz pass then throws random datagrams at a star and a hub and checks that
nothing ever leaves the relay that the kind table does not allow.

SOCKET: the real relay.py on 127.0.0.1 (a port from this process id, never
51765), real sockets: a 16-seat hub fan-out with stamping, a spoofed seat, an
oversize datagram, a flood against the caps, a version 1 session beside a
version 2 one, the stats line's per-kind counters, and a member process that
dies mid-session while the others keep hearing each other.

Python 3 standard library only.
"""

import importlib.util
import os
import random
import select
import socket
import struct
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
RELAY_PY = os.path.join(HERE, "relay.py")

MAGIC_HELLO = b"SMRC"
MAGIC_ACK = b"SMRA"
ROLE_PARENT, ROLE_CHILD, ROLE_MEMBER = 0, 1, 2
ANY = 0xFF
ST_OK, ST_FULL, ST_BAD, ST_VERSION, ST_TAKEN, ST_KIND = 0, 1, 2, 3, 4, 5

# The kind table as the design writes it (NETARCH1 design.txt section 3). The
# suite checks relay.py against THIS copy, so a drift in either is a failure.
#   tag: (lengths, star route, hub route, cap, stamp offset)
DESIGN_KINDS = {
    b"MP2L": ((0x90, 0x214), "star", None, 75, 6),
    b"SYNG": ((44,), None, "all", 35, 5),
    b"SYNI": (tuple(range(24, 65)), "all", "all", 3, 5),
    b"SYNT": (tuple(range(12, 133)), "all", "all", 8, 5),
    b"SYNV": ((176, 340), "all", "all", 30, 5),
    b"SYNP": ((12,), "all", "all", 6, 5),
    b"SYNQ": ((12,), "seat6", "seat6", 6, 5),
    b"SYNK": ((24,), "up", None, 4, 5),
    b"SYN1": ((56,), "star", None, 35, 5),
}


def hello(code, role, want=0, flags=0, version=2, magic=MAGIC_HELLO):
    if isinstance(code, str):
        code = code.encode("ascii")
    return (magic + bytes((version & 0xFF, role & 0xFF, want & 0xFF,
                           flags & 0xFF)) + code.ljust(8, b"\x00")[:8])


def parse_ack(data):
    """(version, status, seat, reserved, code) or None."""
    if len(data) != 16 or data[:4] != MAGIC_ACK:
        return None
    return (data[4], data[5], data[6], data[7],
            data[8:16].rstrip(b"\x00").decode("ascii", "replace"))


def dgram(tag, length, fill=0x5A, seat_claim=0x0E, byte6=None):
    """A datagram of one kind: tag, a claimed (spoofable) seat at byte 5 and 6,
    then filler. byte6 sets the pong target / MP2L slot byte."""
    body = bytearray(tag + bytes([1, seat_claim, seat_claim, 0]))
    body = body[:length] if length < len(body) else body
    body += bytes([fill]) * (length - len(body))
    if byte6 is not None and length > 6:
        body[6] = byte6
    return bytes(body)


# ------------------------------------------------------------- result sink


class Result(object):
    def __init__(self, name):
        self.name = name
        self.ok = True
        self.lines = []
        self.n = 0

    def check(self, cond, msg):
        self.n += 1
        if not cond:
            self.ok = False
            self.lines.append("    [FAIL] %s" % msg)
        return bool(cond)

    def note(self, msg):
        self.lines.append("    ---- %s" % msg)

    def report(self):
        print("  %s: %s (%d checks)" % (self.name, "PASS" if self.ok
                                        else "FAIL", self.n))
        for line in self.lines:
            print(line)
        sys.stdout.flush()


# ================================================================ UNIT PART


def load_relay(env=None):
    """A fresh relay module with its knobs read from `env` (defaults + 15
    children, the live box's setting, unless told otherwise)."""
    keys = ("SM64DS_RELAY_MAX_CHILDREN", "SM64DS_RELAY_RATE_PPS",
            "SM64DS_RELAY_RATE_BURST", "SM64DS_RELAY_RATE_PPS_V2")
    saved = dict((k, os.environ.get(k)) for k in keys)
    try:
        for k in keys:
            os.environ.pop(k, None)
        os.environ["SM64DS_RELAY_MAX_CHILDREN"] = "15"
        for k, v in (env or {}).items():
            os.environ[k] = v
        spec = importlib.util.spec_from_file_location(
            "relay_under_test_%d" % random.randrange(1 << 30), RELAY_PY)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
    finally:
        for k, v in saved.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
    mod.LOGGED = []
    mod.log = mod.LOGGED.append
    return mod


class Wire(object):
    """Records everything the relay sends."""

    def __init__(self):
        self.sent = []

    def sendto(self, data, addr):
        self.sent.append((bytes(data), addr))

    def take(self):
        out, self.sent = self.sent, []
        return out


class Rig(object):
    """One RelayProtocol, a recording transport and a clock."""

    def __init__(self, mod, idle_s=90.0, hub_idle_s=10.0):
        self.mod = mod
        self.relay = mod.RelayProtocol(idle_s, hub_idle_s)
        self.wire = Wire()
        self.relay.connection_made(self.wire)
        self.now = 1000.0
        self._port = 30000

    def addr(self):
        self._port += 1
        return ("198.51.100.7", self._port)

    def send(self, addr, data, dt=0.0):
        self.now += dt
        self.relay.on_datagram(data, addr, self.now)
        return self.wire.take()

    def join(self, addr, code, role, want, **kw):
        out = self.send(addr, hello(code, role, want, **kw), dt=0.001)
        acks = [parse_ack(d) for d, a in out if a == addr]
        acks = [a for a in acks if a is not None]
        return acks[-1] if acks else None

    def settle(self, dt=2.0):
        """Let every bucket refill."""
        self.now += dt


def u_hello_and_ack(mod):
    res = Result("HELLO/ACK v2: seats, ACK bytes, keepalive, rejoin")
    rig = Rig(mod)
    p, c1, c2 = rig.addr(), rig.addr(), rig.addr()
    raw = rig.send(p, hello("STAR0001", ROLE_PARENT, 0))
    res.check(len(raw) == 1 and raw[0][1] == p, "parent gets exactly one ACK")
    res.check(raw[0][0] == b"SMRA\x02\x00\x00\x00STAR0001",
              "parent ACK is byte for byte SMRA|2|ok|seat 0|0|code (%r)"
              % raw[0][0])
    res.check(rig.join(c1, "STAR0001", ROLE_CHILD, 3)[:3] == (2, ST_OK, 3),
              "child wanting seat 3 gets seat 3")
    res.check(rig.join(c2, "STAR0001", ROLE_CHILD, ANY)[:3] == (2, ST_OK, 1),
              "child wanting any seat gets the lowest free child seat, 1")
    # keepalive: idempotent, same seat, no new join line
    joins = sum(1 for l in mod.LOGGED if "join" in l)
    for want in (3, 3, ANY):
        ack = rig.join(c1, "STAR0001", ROLE_CHILD, want)
        res.check(ack[:3] == (2, ST_OK, 3), "re-HELLO want %d keeps seat 3"
                  % want)
    res.check(sum(1 for l in mod.LOGGED if "join" in l) == joins,
              "re-HELLOs log no new join")
    # rejoin with another seat: the old seat is freed
    ack = rig.join(c1, "STAR0001", ROLE_CHILD, 5)
    res.check(ack[:3] == (2, ST_OK, 5), "same endpoint asking for seat 5 moves")
    s = rig.relay.sessions["STAR0001"]
    res.check(3 not in s.seats and s.seats.get(5) == c1,
              "seat 3 freed, seat 5 held by the mover")
    # hub
    hub = [rig.addr() for _ in range(17)]
    seats = [rig.join(a, "HUB00001", ROLE_MEMBER, ANY) for a in hub[:16]]
    res.check([s[2] for s in seats] == list(range(16))
              and all(s[1] == ST_OK for s in seats),
              "16 hub members get seats 0..15 in order")
    ack = rig.join(hub[16], "HUB00001", ROLE_MEMBER, ANY)
    res.check(ack[:2] == (2, ST_FULL), "the 17th hub member gets FULL")
    # a hub member may name a seat
    rig2 = Rig(mod)
    a, b = rig2.addr(), rig2.addr()
    res.check(rig2.join(a, "HUBSEAT1", ROLE_MEMBER, 7)[:3] == (2, ST_OK, 7),
              "hub member naming seat 7 gets 7")
    res.check(rig2.join(b, "HUBSEAT1", ROLE_MEMBER, 7)[:3]
              == (2, ST_TAKEN, 0), "a second member naming 7 gets SEAT TAKEN")
    res.check(rig2.join(b, "HUBSEAT1", ROLE_MEMBER, ANY)[:3] == (2, ST_OK, 0),
              "and then any seat gets 0")
    return res


def u_refusals(mod):
    res = Result("HELLO refusals: every status, never amplifying")
    rig = Rig(mod)
    p = rig.addr()
    rig.join(p, "REFUSE01", ROLE_PARENT, 0)
    c = rig.addr()
    rig.join(c, "REFUSE01", ROLE_CHILD, 2)

    def expect(name, data, status, version=2):
        a = rig.addr()
        out = rig.send(a, data, dt=0.001)
        acks = [parse_ack(d) for d, to in out if to == a]
        res.check(len(out) == 1 and acks and acks[0] is not None
                  and acks[0][0] == version and acks[0][1] == status,
                  "%s -> v%d status %d (%r)" % (name, version, status, out))
        res.check(all(len(d) <= len(data) for d, _ in out),
                  "%s: reply no longer than the HELLO" % name)
        return a

    expect("seat taken (child 2 twice)", hello("REFUSE01", ROLE_CHILD, 2),
           ST_TAKEN)
    expect("second parent", hello("REFUSE01", ROLE_PARENT, 0), ST_TAKEN)
    expect("member into a star", hello("REFUSE01", ROLE_MEMBER, ANY), ST_KIND)
    h = rig.addr()
    rig.join(h, "REFUSE02", ROLE_MEMBER, ANY)
    expect("parent into a hub", hello("REFUSE02", ROLE_PARENT, 0), ST_KIND)
    expect("child into a hub", hello("REFUSE02", ROLE_CHILD, 1), ST_KIND)
    # version mix both ways
    v1 = rig.addr()
    out = rig.send(v1, hello("REFUSE03", ROLE_PARENT, version=1))
    res.check(parse_ack(out[0][0])[:2] == (1, ST_OK), "v1 parent opens a v1 "
              "session")
    expect("v2 into a v1 session", hello("REFUSE03", ROLE_CHILD, 1), ST_VERSION)
    expect("v1 child into a v2 session (v1 knows no status 3: FULL)",
           hello("REFUSE01", ROLE_CHILD, 0, version=1), ST_FULL, version=1)
    res.check(isinstance(rig.relay.sessions["REFUSE01"], mod.SessionV2)
              and len(rig.relay.sessions["REFUSE01"].seats) == 2,
              "the v2 session is untouched by the v1 attempt")
    # seat past this relay's children
    rig15 = Rig(load_relay({"SM64DS_RELAY_MAX_CHILDREN": "3"}))
    a = rig15.addr()
    rig15.join(a, "SMALL001", ROLE_PARENT, 0)
    res.check(rig15.join(rig15.addr(), "SMALL001", ROLE_CHILD, 4)[:2]
              == (2, ST_FULL), "child seat 4 on a 3-child relay -> FULL")
    for want in (1, 2, 3):
        rig15.join(rig15.addr(), "SMALL001", ROLE_CHILD, want)
    res.check(rig15.join(rig15.addr(), "SMALL001", ROLE_CHILD, ANY)[:2]
              == (2, ST_FULL), "any seat on a full star -> FULL")
    # malformed: status 2, then the address is ignored for 10 s
    for name, data in (
            ("parent wanting seat 1", hello("BADREQ01", ROLE_PARENT, 1)),
            ("child wanting seat 0", hello("BADREQ02", ROLE_CHILD, 0)),
            ("child wanting seat 16", hello("BADREQ03", ROLE_CHILD, 16)),
            ("member wanting seat 16", hello("BADREQ04", ROLE_MEMBER, 16)),
            ("member wanting seat 200", hello("BADREQ05", ROLE_MEMBER, 200)),
            ("role 4 (unknown; 3 is an arena member)",
             hello("BADREQ06", 4, 0)),
            ("arena wanting seat 16", hello("BADREQ07", 3, 16)),
            ("empty code", hello("", ROLE_MEMBER, ANY)),
            ("code with a space", hello("AB CD", ROLE_MEMBER, ANY))):
        a = expect(name, data, ST_BAD)
        out = rig.send(a, hello("GOODCODE", ROLE_MEMBER, ANY), dt=0.5)
        res.check(out == [], "%s: the address is then ignored" % name)
    # session cap
    rigc = Rig(mod)
    made = 0
    for i in range(mod.MAX_SESSIONS):
        if rigc.join(rigc.addr(), "CAP%05d" % i, ROLE_MEMBER, ANY)[1] == ST_OK:
            made += 1
    res.check(made == 256 == mod.MAX_SESSIONS, "256 sessions open (%d)" % made)
    res.check(rigc.join(rigc.addr(), "CAPOVER1", ROLE_MEMBER, ANY)[:2]
              == (2, ST_FULL), "the 257th session -> FULL")
    res.check(rigc.join(rigc.addr(), "CAPOVER2", ROLE_PARENT, 0,
                        version=1)[:2] == (1, ST_FULL),
              "and a v1 session past the cap -> v1 FULL")
    return res


def build_star(rig, code, n):
    addrs = [rig.addr() for _ in range(n)]
    rig.join(addrs[0], code, ROLE_PARENT, 0)
    for k in range(1, n):
        rig.join(addrs[k], code, ROLE_CHILD, k)
    rig.settle()
    return addrs


def build_hub(rig, code, n):
    addrs = [rig.addr() for _ in range(n)]
    for a in addrs:
        rig.join(a, code, ROLE_MEMBER, ANY)
    rig.settle()
    return addrs


def u_kind_table(mod):
    res = Result("kind table matches the design, byte for byte")
    res.check(set(mod.KINDS) == set(DESIGN_KINDS),
              "same tags (%s)" % sorted(mod.KINDS))
    for tag, (lengths, star, hub, cap, stamp) in DESIGN_KINDS.items():
        k = mod.KINDS.get(tag)
        if k is None:
            continue
        ok_lens = [n for n in range(0, 800) if k.length_ok(n)]
        res.check(ok_lens == sorted(lengths), "%s lengths %s..%s"
                  % (tag, ok_lens[:1], ok_lens[-1:]))
        res.check((k.star, k.hub, k.cap, k.stamp) == (star, hub, cap, stamp),
                  "%s routes/cap/stamp %r" % (tag, (k.star, k.hub, k.cap,
                                                    k.stamp)))
    res.check(mod.RATE_PPS_V2 == 240 and mod.RATE_PPS == 120,
              "per-source budget: v2 240, v1 120 (defaults)")
    res.check(mod.MAX_SESSIONS == 256 and mod.HUB_SEATS == 16,
              "MAX_SESSIONS 256, hub seats 16")
    return res


def u_routes_and_stamping(mod):
    res = Result("routes and seat stamping, every kind, star and hub")
    rig = Rig(mod)
    star = build_star(rig, "ROUTES01", 4)
    hub = build_hub(rig, "ROUTES02", 4)

    def deliver(frm, data):
        rig.settle(1.0)
        return rig.send(frm, data)

    for tag, (lengths, star_r, hub_r, cap, stamp) in DESIGN_KINDS.items():
        for shape, addrs, route in (("star", star, star_r),
                                    ("hub", hub, hub_r)):
            for n in sorted(set((min(lengths), max(lengths)))):
                for sender in range(4):
                    target6 = (sender + 1) % 4
                    data = dgram(tag, n, fill=0x40 + sender, seat_claim=0x0D,
                                 byte6=target6 if tag == b"SYNQ" else None)
                    out = deliver(addrs[sender], data)
                    got = sorted(addrs.index(a) for _, a in out)
                    if route is None:
                        want = []
                    elif route == "all":
                        want = [k for k in range(4) if k != sender]
                    elif route == "star":
                        want = [1, 2, 3] if sender == 0 else [0]
                    elif route == "up":
                        want = [] if sender == 0 else [0]
                    else:
                        want = [target6]
                    res.check(got == want, "%s %s len %d from seat %d -> %s "
                              "(got %s)" % (shape, tag, n, sender, want, got))
                    for d, _ in out:
                        exp = bytearray(data)
                        exp[stamp] = sender
                        res.check(d == bytes(exp),
                                  "%s %s from %d: only byte %d rewritten, to "
                                  "the sender's seat" % (shape, tag, sender,
                                                         stamp))
    # pong to itself or to an empty seat goes nowhere
    out = deliver(hub[1], dgram(b"SYNQ", 12, byte6=1))
    res.check(out == [], "pong naming its own seat goes nowhere")
    out = deliver(hub[1], dgram(b"SYNQ", 12, byte6=9))
    res.check(out == [], "pong naming an empty seat goes nowhere")
    out = deliver(hub[1], dgram(b"SYNQ", 12, byte6=0xFF))
    res.check(out == [], "pong naming seat 255 goes nowhere")
    return res


def u_drop_reasons(mod):
    res = Result("every drop reason is a drop, counted, and nothing leaks")
    rig = Rig(mod)
    star = build_star(rig, "DROPS001", 3)
    hub = build_hub(rig, "DROPS002", 3)
    r = rig.relay

    def case(name, frm, data, reason, tag=None):
        before = dict(r.v2_drop)
        kb = list(r.kstats[tag]) if tag else None
        rig.settle(1.0)
        out = rig.send(frm, data)
        res.check(out == [], "%s: nothing forwarded (%d)" % (name, len(out)))
        res.check(r.v2_drop[reason] == before[reason] + 1,
                  "%s: counted as drop_%s" % (name, reason))
        if tag:
            res.check(r.kstats[tag][2] == kb[2] + 1 and
                      r.kstats[tag][0] == kb[0] + 1,
                      "%s: counted rx and drop under %s" % (name, tag))

    # unknown tags, including near misses of real ones and the v1 world
    for name, data in (("tag XXXX", b"XXXX" + bytes(40)),
                       ("tag SYNZ", b"SYNZ" + bytes(20)),
                       ("tag syng (case)", b"syng" + bytes(40)),
                       ("tag MP2l", b"MP2l" + bytes(0x8C)),
                       ("3 bytes", b"SYN"),
                       ("1 byte", b"\x00"),
                       ("empty", b""),
                       ("16 bytes not a HELLO", b"ZZZZABCDEFGHIJKL")):
        case("hub " + name, hub[0], data, "unknown")
        case("star " + name, star[1], data, "unknown")
    # wrong length for every kind, both shapes
    for tag, (lengths, star_r, hub_r, cap, stamp) in DESIGN_KINDS.items():
        bad = sorted(set(n for L in lengths for n in (L - 1, L + 1))
                     - set(lengths))
        bad += [0x300, 4, 5]
        for n in bad:
            if n < 4:
                continue
            case("hub %s len %d" % (tag, n), hub[1], dgram(tag, n),
                 "length", tag)
            case("star %s len %d" % (tag, n), star[1], dgram(tag, n),
                 "length", tag)
    # wrong kind for the session shape
    case("MP2L in a hub", hub[0], dgram(b"MP2L", 0x90), "route", b"MP2L")
    case("wide MP2L in a hub", hub[0], dgram(b"MP2L", 0x214), "route", b"MP2L")
    case("SYNG in a star", star[0], dgram(b"SYNG", 44), "route", b"SYNG")
    case("SYNK in a hub", hub[1], dgram(b"SYNK", 24), "route", b"SYNK")
    case("SYN1 in a hub", hub[1], dgram(b"SYN1", 56), "route", b"SYN1")
    case("SYNK from the parent", star[0], dgram(b"SYNK", 24), "route",
         b"SYNK")
    # no target
    case("pong to an empty seat", hub[0], dgram(b"SYNQ", 12, byte6=12),
         "target", b"SYNQ")
    lone = rig.addr()
    rig.join(lone, "ALONE001", ROLE_PARENT, 0)
    rig.settle()
    case("parent alone", lone, dgram(b"MP2L", 0x90), "target", b"MP2L")
    # over cap: a burst of one kind, one second deep
    for tag in DESIGN_KINDS:
        lengths, star_r, hub_r, cap, stamp = DESIGN_KINDS[tag]
        shape, addrs = (("hub", hub) if hub_r else ("star", star))
        frm = addrs[1]
        rig.settle(3.0)
        through = 0
        for i in range(cap + 10):
            out = rig.send(frm, dgram(tag, lengths[0],
                                      byte6=2 if tag == b"SYNQ" else None),
                           dt=0.0001)
            through += 1 if out else 0
        res.check(through == cap, "%s %s: a burst of %d passes exactly the cap"
                  " %d (%d)" % (shape, tag, cap + 10, cap, through))
        rig.settle(1.0)
        out = rig.send(frm, dgram(tag, lengths[0],
                                  byte6=2 if tag == b"SYNQ" else None))
        res.check(len(out) > 0, "%s %s: the cap releases after a second"
                  % (shape, tag))
    res.check(r.v2_drop["cap"] >= 10 * len(DESIGN_KINDS),
              "cap drops counted (%d)" % r.v2_drop["cap"])
    # a stranger's non-HELLO never reaches the table at all
    before = r.n_drop_unpaired
    out = rig.send(rig.addr(), dgram(b"SYNG", 44))
    res.check(out == [] and r.n_drop_unpaired == before + 1,
              "a stranger's ghost frame is dropped unpaired, no reply")
    line = r.stats_line()
    res.check(all(("drop_%s=" % x) in line for x in mod.DROP_REASONS)
              and "SYNG=" in line and "MP2L=" in line,
              "stats line carries per-reason and per-kind counters")
    res.note(line.split("rss_kb=")[1])
    return res


def u_rate_budget(mod):
    res = Result("per-source budget: v2 240/s, v1 unchanged at 120/s")
    rig = Rig(mod)
    hub = build_hub(rig, "BUDGET01", 2)
    rig.settle(3.0)
    # Mix kinds so no per-kind cap binds first: 35+3+8+30+6+6 = 88 < 240, so
    # use many kinds and spread the burst; check the per-source limit by
    # counting what the relay accepted (fwd or cap-drop) in a 1 ms burst.
    r = rig.relay
    kinds = [(b"SYNG", 44), (b"SYNV", 340), (b"SYNT", 40), (b"SYNP", 12),
             (b"SYNI", 24), (b"SYNQ", 12)]
    rx0 = r.n_drop_rate
    for i in range(400):
        tag, n = kinds[i % len(kinds)]
        rig.send(hub[0], dgram(tag, n, byte6=1 if tag == b"SYNQ" else None),
                 dt=0.000001)
    rate_drops = r.n_drop_rate - rx0
    res.check(160 - 2 <= rate_drops <= 160 + 2,
              "a 400-datagram burst from a v2 seat: ~240 pass the source "
              "bucket, %d rate drops (expect 160)" % rate_drops)
    # v1: 120
    p, c = rig.addr(), rig.addr()
    rig.send(p, hello("BUDGET02", ROLE_PARENT, version=1))
    rig.send(c, hello("BUDGET02", ROLE_CHILD, version=1))
    rig.settle(3.0)
    rx0 = r.n_drop_rate
    fwd = 0
    for i in range(400):
        fwd += len(rig.send(c, b"anything at all" + bytes(20), dt=0.000001))
    rate_drops = r.n_drop_rate - rx0
    res.check(280 - 2 <= rate_drops <= 280 + 2 and 118 <= fwd <= 122,
              "the same burst from a v1 seat: ~120 forwarded (%d), %d rate "
              "drops" % (fwd, rate_drops))
    return res


def u_idle(mod):
    res = Result("idle expiry: hub 10 s, star 90 s")
    rig = Rig(mod)
    hub = build_hub(rig, "IDLEHUB1", 3)
    star = build_star(rig, "IDLESTR1", 2)
    r = rig.relay
    # hub member 0 keeps alive by HELLO every second, 1 by ghost frames,
    # 2 goes quiet
    for sec in range(12):
        rig.now += 1.0
        rig.relay.on_datagram(hello("IDLEHUB1", ROLE_MEMBER, ANY), hub[0],
                              rig.now)
        rig.relay.on_datagram(dgram(b"SYNG", 44), hub[1], rig.now)
        r.sweep(rig.now)
    rig.wire.take()
    seats = r.sessions["IDLEHUB1"].seats
    res.check(sorted(seats) == [0, 1],
              "after 12 s: the HELLO keeper and the talker stay, the quiet "
              "member is gone (%s)" % sorted(seats))
    res.check("IDLESTR1" in r.sessions and len(r.sessions["IDLESTR1"].seats)
              == 2, "the star session is still whole at 12 s")
    rig.now += 80.0
    r.sweep(rig.now)
    res.check("IDLESTR1" not in r.sessions,
              "the star session expires past 90 s")
    rig.now += 11.0
    r.sweep(rig.now)
    res.check("IDLEHUB1" not in r.sessions, "and the hub once nobody talks")
    # a freed hub seat is reused
    back = rig.join(hub[2], "IDLEHUB1", ROLE_MEMBER, ANY)
    res.check(back[:3] == (2, ST_OK, 0), "a returning member gets a seat")
    return res


def u_fuzz(mod, rounds=40000):
    res = Result("fuzz: %d random datagrams, nothing outside the table leaves"
                 % rounds)
    rng = random.Random(0x5EED)
    rig = Rig(mod)
    star = build_star(rig, "FUZZSTAR", 5)
    hub = build_hub(rig, "FUZZHUB1", 5)
    v1p, v1c = rig.addr(), rig.addr()
    rig.send(v1p, hello("FUZZV1XX", ROLE_PARENT, version=1))
    rig.send(v1c, hello("FUZZV1XX", ROLE_CHILD, version=1))
    seat_of = {}
    for k, a in enumerate(star):
        seat_of[a] = ("star", k)
    for k, a in enumerate(hub):
        seat_of[a] = ("hub", k)
    tags = list(DESIGN_KINDS) + [b"SMRC", b"SMRA", b"XXXX", b"SYN\x00",
                                 b"MP2M"]
    out_n = 0
    bad = []
    for i in range(rounds):
        frm = rng.choice(star + hub)
        tag = rng.choice(tags)
        if rng.random() < 0.5 and tag in DESIGN_KINDS:
            n = rng.choice(DESIGN_KINDS[tag][0])
        else:
            n = rng.randrange(0, 760)
        body = (tag + bytes(rng.randrange(256) for _ in range(12)))[:n]
        body += bytes(max(0, n - len(body)))
        if n == 16 and body[:4] == b"SMRC":
            continue    # a HELLO is not a forwarded datagram
        out = rig.send(frm, body, dt=rng.random() * 0.004)
        shape, seat = seat_of[frm]
        for d, to in out:
            out_n += 1
            if to == frm:
                bad.append("echo to sender")
            k = DESIGN_KINDS.get(d[:4])
            if k is None:
                bad.append("tag %r left the relay" % d[:4])
                continue
            lengths, star_r, hub_r, cap, stamp = k
            if len(d) not in lengths:
                bad.append("%r len %d left" % (d[:4], len(d)))
            if (hub_r if shape == "hub" else star_r) is None:
                bad.append("%r routed in a %s" % (d[:4], shape))
            if d[stamp] != seat:
                bad.append("%r not stamped" % d[:4])
            if seat_of.get(to, (None,))[0] != shape:
                bad.append("crossed sessions")
            exp = bytearray(body)
            exp[stamp] = seat
            if d != bytes(exp):
                bad.append("bytes other than the stamp changed")
    res.check(not bad, "violations: %d %s" % (len(bad), sorted(set(bad))[:5]))
    res.check(out_n > 1000, "the fuzz forwarded plenty of legal traffic (%d)"
              % out_n)
    r = rig.relay
    res.note("forwarded %d; drops %s" % (out_n, r.v2_drop))
    return res


def u_v1_untouched(mod):
    res = Result("version 1 sessions: blind, unstamped, as before")
    rig = Rig(mod)
    p, c1, c2 = rig.addr(), rig.addr(), rig.addr()
    raw = rig.send(p, hello("V1STAR01", ROLE_PARENT, version=1))
    res.check(raw == [(b"SMRA\x01\x00\x00\x00V1STAR01", p)],
              "v1 ACK byte for byte as before")
    rig.send(c1, hello("V1STAR01", ROLE_CHILD, version=1))
    rig.send(c2, hello("V1STAR01", ROLE_CHILD, version=1))
    rig.settle()
    for data in (b"MP2L" + bytes([2, 3, 9, 1]) + bytes(0x88),
                 b"SYNG" + bytes(40), b"whatever the game sends" * 3,
                 b"\x00" * 700):
        out = rig.send(c1, data, dt=0.01)
        res.check(out == [(data, p)], "v1 child -> parent verbatim, len %d"
                  % len(data))
        out = rig.send(p, data, dt=0.01)
        res.check(sorted(out) == sorted([(data, c1), (data, c2)]),
                  "v1 parent -> children verbatim, len %d" % len(data))
    out = rig.send(c1, b"\x00" * 701)
    res.check(out == [], "v1 701 bytes still dropped")
    res.check(all(v == [0, 0, 0] for v in rig.relay.kstats.values()),
              "v1 traffic never touches the kind counters")
    return res


def schedule(addrs, per_kind, t0, secs, pong_to=None):
    """(t, addr, datagram) events: every addr sends every (tag, length, pps)
    in per_kind evenly over [t0, t0 + secs), each sender offset a little."""
    ev = []
    for i, a in enumerate(addrs):
        for tag, n, pps in per_kind:
            step = 1.0 / pps
            k = 0
            while True:
                t = t0 + (i * 0.00037) + k * step
                if t >= t0 + secs:
                    break
                b6 = None
                if tag == b"SYNQ":
                    b6 = (pong_to(i, k) if pong_to
                          else (i + 1) % len(addrs))
                ev.append((t, a, dgram(tag, n, byte6=b6)))
                k += 1
    return ev


def u_session_budget(mod):
    res = Result("per-session forward budget: a flooding hub is held, a "
                 "normal and a worst-case hub beside it are untouched")
    budget = mod.SESSION_FWD_PPS
    res.check(budget == 15000, "default budget 15000 forwards/s (%d)" % budget)
    rig = Rig(mod)
    flood = build_hub(rig, "FLOODHUB", 16)
    normal = build_hub(rig, "NORMHUB1", 16)
    worst = build_hub(rig, "WORSTHUB", 16)
    # 12 s, read over the last 4: the budget's one-second bucket lets a flood
    # run over it until that second of credit is spent (~6 s at this flood)
    secs = 12.0
    t0 = rig.now + 0.5
    # the flood: 16 ordinary sockets, each at every per-kind cap
    ev = schedule(flood, [(b"SYNG", 44, 35), (b"SYNI", 24, 3),
                          (b"SYNT", 40, 8), (b"SYNV", 340, 30),
                          (b"SYNP", 12, 6), (b"SYNQ", 12, 6)], t0, secs)
    # a normal room: 15 Hz ghosts, 2 Hz pings, 4 talkers
    ev += schedule(normal, [(b"SYNG", 44, 15), (b"SYNP", 12, 2)], t0, secs)
    ev += schedule(normal[:4], [(b"SYNV", 340, 25)], t0, secs)
    # the worst legitimate room (design sections 3 and 6): every seat talking,
    # chatting at the sender limit (3 sends per 1.5 s), identity every 2 s,
    # pings at 2 Hz, pongs at the table's cap of 6 a second
    ev += schedule(worst, [(b"SYNG", 44, 15), (b"SYNP", 12, 2),
                           (b"SYNI", 24, 0.5), (b"SYNT", 40, 2),
                           (b"SYNV", 340, 25), (b"SYNQ", 12, 6)], t0, secs,
                   pong_to=lambda i, k: (i + 1 + k % 15) % 16)
    ev.sort(key=lambda e: e[0])
    where = {}
    for name, addrs in (("flood", flood), ("normal", normal),
                        ("worst", worst)):
        for a in addrs:
            where[a] = name
    got = dict((n, 0) for n in ("flood", "normal", "worst"))
    late = dict((n, 0) for n in ("flood", "normal", "worst"))
    for t, a, d in ev:
        rig.now = t
        rig.relay.on_datagram(d, a, t)
        for _, to in rig.wire.take():
            got[where[to]] += 1
            if t >= t0 + 8.0:
                late[where[to]] += 1
    late_s = secs - 8.0
    rate = late["flood"] / late_s
    res.note("forwards/s over the last 4 s: flood %.0f, normal %.0f, "
             "worst-case %.0f; session_cap drops %d"
             % (rate, late["normal"] / late_s, late["worst"] / late_s,
                rig.relay.v2_drop["session_cap"]))
    res.check(budget * 0.95 <= rate <= budget * 1.01,
              "the flooding hub is held at the budget (%.0f/s)" % rate)
    exp = {"normal": 0, "worst": 0}
    for t, a, d in ev:
        if where[a] in exp:
            exp[where[a]] += 1 if d[:4] == b"SYNQ" else 15
    for name in ("normal", "worst"):
        res.check(got[name] == exp[name], "the %s hub got every copy (%d of "
                  "%d)" % (name, got[name], exp[name]))
    res.check(late["worst"] / late_s > 10000,
              "the worst-case room ran at %.0f/s, under the budget"
              % (late["worst"] / late_s))
    line = rig.relay.stats_line()
    res.check(rig.relay.v2_drop["session_cap"] > 0 and
              ("drop_session_cap=%d" % rig.relay.v2_drop["session_cap"])
              in line, "drops counted as drop_session_cap in the stats line")
    # the knob, and v1 is not budgeted at all
    small = load_relay({"SM64DS_RELAY_SESSION_FWD_PPS": "300"})
    rig2 = Rig(small)
    a = build_hub(rig2, "KNOBHUB1", 16)
    n = 0
    for k in range(3):
        for x in a:
            n += len(rig2.send(x, dgram(b"SYNG", 44), dt=0.0005))
    res.check(n == 300, "SM64DS_RELAY_SESSION_FWD_PPS=300: a burst passes "
              "exactly 300 copies (%d)" % n)
    p = rig2.addr()
    rig2.send(p, hello("KNOBV1XX", ROLE_PARENT, version=1))
    for kid in [rig2.addr() for _ in range(3)]:
        rig2.send(kid, hello("KNOBV1XX", ROLE_CHILD, version=1))
    rig2.settle()
    n = 0
    for k in range(110):
        n += len(rig2.send(p, b"MP2L" + bytes(0x8C), dt=0.0005))
    res.check(n == 330, "and a v1 session on that relay is not budgeted "
              "(%d of 110 x 3)" % n)
    return res


def u_rehello_caps(mod):
    res = Result("per-kind caps survive a re-HELLO into another seat, "
                 "session or version")
    rig = Rig(mod)
    listener = rig.addr()
    rig.join(listener, "HOPHUB01", ROLE_MEMBER, 0)
    hopper = rig.addr()
    rig.join(hopper, "HOPHUB01", ROLE_MEMBER, 1)
    other = rig.addr()
    rig.join(other, "HOPHUB02", ROLE_MEMBER, 0)
    v1p = rig.addr()
    rig.send(v1p, hello("HOPV1XXX", ROLE_PARENT, version=1))
    rig.settle(3.0)
    t0 = rig.now
    heard = [0, 0, 0]
    seat = 1
    for i in range(3 * 200):     # 200 voice datagrams a second for 3 s
        rig.now = t0 + i / 200.0
        if i % 5 == 4:
            # hop: another seat, now and then another session or a v1 seat
            step = (i // 5) % 6
            if step == 5:
                data = hello("HOPV1XXX", ROLE_CHILD, version=1)
            elif step == 4:
                data = hello("HOPHUB02", ROLE_MEMBER, ANY)
            else:
                seat = 1 + (seat % 14)
                data = hello("HOPHUB01", ROLE_MEMBER, seat)
            rig.relay.on_datagram(data, hopper, rig.now)
            rig.wire.take()
            continue
        ep = rig.relay.endpoints.get(hopper)
        if ep is None or ep.version != 2:
            continue            # a v1 seat forwards blind: not this test
        rig.relay.on_datagram(dgram(b"SYNV", 340), hopper, rig.now)
        for d, to in rig.wire.take():
            if to in (listener, other):
                heard[min(2, int(rig.now - t0))] += 1
    res.note("voice copies a second at the listeners while hopping: %s"
             % heard)
    res.check(heard[0] <= 60 and heard[1] <= 30 and heard[2] <= 30,
              "held to the 30/s voice cap (plus one second of burst in the "
              "first second): %s" % heard)
    res.check(heard[1] >= 25 and heard[2] >= 25,
              "and not starved below it: %s" % heard)
    rig.relay._detach(rig.relay.endpoints[hopper], "test")
    rig.now += mod.KIND_CAPS_IDLE_S + 1.0
    rig.relay.on_datagram(hello("HOPHUB01", ROLE_MEMBER, 0), listener,
                          rig.now)
    rig.relay.sweep(rig.now)
    res.check(hopper not in rig.relay.kind_caps,
              "an address that holds no seat is forgotten after %.0f s"
              % mod.KIND_CAPS_IDLE_S)
    res.check(listener in rig.relay.kind_caps, "a seated address is kept")
    return res


def u_host_mp2l_cap(mod):
    res = Result("the star host's lockstep cap: 150/s for seat 0, children "
                 "still 75/s")
    res.check(mod.HOST_MP2L_PPS == 150, "default host MP2L cap 150/s (%d)"
              % mod.HOST_MP2L_PPS)
    rig = Rig(mod)
    star = build_star(rig, "HOSTCAP1", 16)
    t0 = rig.now + 0.5
    secs = 4.0
    # the host at 140/s of wide aggregates, 14 children at 30/s, and one
    # child (seat 15) pushing 100/s
    ev = schedule(star[:1], [(b"MP2L", 0x214, 140)], t0, secs)
    ev += schedule(star[1:15], [(b"MP2L", 0x214, 30)], t0, secs)
    ev += schedule(star[15:], [(b"MP2L", 0x214, 100)], t0, secs)
    ev.sort(key=lambda e: e[0])
    host = star[0]
    sent = {"host": 0, "fast": 0}
    out = {"host": 0, "fast": 0}
    for t, a, d in ev:
        rig.now = t
        rig.relay.on_datagram(d, a, t)
        copies = rig.wire.take()
        who = "host" if a == host else ("fast" if a == star[15] else None)
        if who:
            sent[who] += 1
            if copies:
                out[who] += 1
    res.note("host sent %d, forwarded %d; seat 15 sent %d, forwarded %d"
             % (sent["host"], out["host"], sent["fast"], out["fast"]))
    res.check(out["host"] == sent["host"],
              "the host at 140/s loses nothing (%d of %d)"
              % (out["host"], sent["host"]))
    # a token bucket passes one second of burst plus the rate: 75 + 75 x 4
    res.check(abs(out["fast"] - (75 + 75 * secs)) <= 2,
              "a child at 100/s is still held at 75/s: %d of %d through "
              "(75 burst + 75 x %.0f s)" % (out["fast"], sent["fast"], secs))
    # 160/s from the host is held at 150/s: 150 burst + 150 x 20 s
    rig2 = Rig(mod)
    s2 = build_star(rig2, "HOSTCAP2", 3)
    t1 = rig2.now + 0.5
    ev = schedule(s2[:1], [(b"MP2L", 0x90, 160)], t1, 20.0)
    n = 0
    for t, a, d in ev:
        rig2.relay.on_datagram(d, a, t)
        n += 1 if rig2.wire.take() else 0
    res.check(abs(n - (150 + 150 * 20)) <= 2,
              "the host at 160/s is held at 150/s: %d of %d through "
              "(150 burst + 150 x 20 s)" % (n, len(ev)))
    # the knob
    k = load_relay({"SM64DS_RELAY_HOST_MP2L_PPS": "90"})
    res.check(k.HOST_MP2L_PPS == 90, "SM64DS_RELAY_HOST_MP2L_PPS=90 read")
    return res


def run_unit():
    mod = load_relay()
    results = []
    for t in (u_kind_table, u_hello_and_ack, u_refusals, u_routes_and_stamping,
              u_drop_reasons, u_rate_budget, u_idle, u_v1_untouched, u_fuzz,
              u_session_budget, u_rehello_caps, u_host_mp2l_cap):
        res = t(mod)
        res.report()
        results.append(res)
    return results


# ============================================================== SOCKET PART


def pid_port():
    base = 20000 + (os.getpid() * 7 % 20000)
    for off in range(400):
        port = base + off
        if port == 51765 or port > 65000:
            continue
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.bind(("127.0.0.1", port))
        except OSError:
            s.close()
            continue
        s.close()
        return port
    raise RuntimeError("no free port")


class RelayProc(object):
    def __init__(self, env=None, relay_py=None):
        self.relay_py = relay_py or RELAY_PY
        self.port = pid_port()
        self.addr = ("127.0.0.1", self.port)
        self.env = dict(os.environ)
        for k in list(self.env):
            if k.startswith("SM64DS_RELAY_"):
                del self.env[k]
        self.env.update(SM64DS_RELAY_PORT=str(self.port),
                        SM64DS_RELAY_BIND="127.0.0.1",
                        SM64DS_RELAY_MAX_CHILDREN="15",
                        SM64DS_RELAY_STATS_S="2", PYTHONUNBUFFERED="1")
        self.env.update(env or {})
        fd, self.log_path = tempfile.mkstemp(prefix="relayv2_", suffix=".log")
        os.close(fd)
        self.fh = open(self.log_path, "wb")

    def __enter__(self):
        self.proc = subprocess.Popen(
            [sys.executable, "-u", self.relay_py], stdout=self.fh,
            stderr=subprocess.STDOUT, env=self.env,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        t0 = time.time()
        while time.time() - t0 < 15:
            if "relay listening" in self.text():
                return self
            if self.proc.poll() is not None:
                break
            time.sleep(0.05)
        raise RuntimeError("relay did not start:\n" + self.text())

    def __exit__(self, *exc):
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
        self.fh.close()
        return False

    def text(self):
        self.fh.flush()
        with open(self.log_path, "rb") as f:
            return f.read().decode("utf-8", "replace")

    def last_stats(self, wait=2.5):
        time.sleep(wait)
        lines = [l for l in self.text().splitlines() if " stats " in l]
        return lines[-1] if lines else ""


class Sock(object):
    def __init__(self, relay):
        self.relay = relay.addr
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.bind(("127.0.0.1", 0))
        self.s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)

    def send(self, data):
        self.s.sendto(data, self.relay)

    def recv_all(self, window=0.3):
        out = []
        end = time.time() + window
        while True:
            left = end - time.time()
            r, _, _ = select.select([self.s], [], [], max(0, left))
            if not r:
                if left <= 0:
                    return out
                continue
            try:
                d, a = self.s.recvfrom(4096)
            except OSError:
                continue
            if a == self.relay:
                out.append(d)

    def join(self, code, role, want, version=2):
        for _ in range(5):
            self.send(hello(code, role, want, version=version))
            for d in self.recv_all(0.5):
                a = parse_ack(d)
                if a is not None:
                    return a
        return None

    def close(self):
        self.s.close()


def s_hub16(relay_env=None):
    res = Result("socket: 16-seat hub, every member hears every other, "
                 "stamped")
    with RelayProc(relay_env) as relay:
        members = [Sock(relay) for _ in range(16)]
        try:
            acks = [m.join("SOCKHUB1", ROLE_MEMBER, ANY) for m in members]
            res.check(all(a and a[1] == ST_OK for a in acks)
                      and sorted(a[2] for a in acks) == list(range(16)),
                      "16 members seated 0..15")
            seat = dict((id(m), a[2]) for m, a in zip(members, acks))
            late = Sock(relay)
            res.check(late.join("SOCKHUB1", ROLE_MEMBER, ANY)[1] == ST_FULL,
                      "17th member refused FULL")
            late.close()
            # every member sends one ghost frame claiming seat 13
            for m in members:
                m.send(dgram(b"SYNG", 44, seat_claim=13))
                time.sleep(0.002)
            time.sleep(0.3)
            heard_ok = 0
            for m in members:
                got = [d for d in m.recv_all(0.4) if d[:4] == b"SYNG"]
                senders = sorted(d[5] for d in got)
                want = sorted(s for s in range(16) if s != seat[id(m)])
                if senders == want:
                    heard_ok += 1
            res.check(heard_ok == 16, "every member heard all 15 others, "
                      "each under its own stamped seat (%d/16)" % heard_ok)
            line = relay.last_stats()
            res.check("SYNG=16/240/0" in line,
                      "stats: SYNG rx 16, fwd 240, drop 0 (%s)"
                      % line.split(" SYNG=")[1].split()[0] if " SYNG=" in line
                      else line)
        finally:
            for m in members:
                m.close()
    return res


def s_security():
    res = Result("socket: oversize, flood, spoofed seat, v1 beside v2")
    with RelayProc() as relay:
        p, c1, c2 = Sock(relay), Sock(relay), Sock(relay)
        v1p, v1c = Sock(relay), Sock(relay)
        try:
            res.check(p.join("SECSTAR1", ROLE_PARENT, 0)[:3] == (2, ST_OK, 0),
                      "v2 parent seated 0")
            res.check(c1.join("SECSTAR1", ROLE_CHILD, 1)[:3] == (2, ST_OK, 1),
                      "v2 child seated 1")
            res.check(c2.join("SECSTAR1", ROLE_CHILD, 2)[:3] == (2, ST_OK, 2),
                      "v2 child seated 2")
            res.check(v1p.join("SECV1XXX", ROLE_PARENT, 0, version=1)[:2]
                      == (1, ST_OK), "v1 parent beside it")
            res.check(v1c.join("SECV1XXX", ROLE_CHILD, 0, version=1)[:2]
                      == (1, ST_OK), "v1 child beside it")
            time.sleep(1.0)
            # spoofed seat: child 2 claims to be the parent (slot 0) on MP2L
            # and seat 0 on voice
            fake = dgram(b"MP2L", 0x90, seat_claim=0)
            c2.send(fake)
            got = [d for d in p.recv_all(0.4)]
            res.check(len(got) == 1 and got[0][6] == 2
                      and got[0][:6] == fake[:6] and got[0][7:] == fake[7:],
                      "child 2's MP2L claiming slot 0 arrives stamped slot 2")
            res.check(c1.recv_all(0.2) == [],
                      "and never reaches the other child")
            c2.send(dgram(b"SYNV", 340, seat_claim=0))
            gp, g1 = p.recv_all(0.4), c1.recv_all(0.4)
            res.check(len(gp) == 1 and len(g1) == 1 and gp[0][5] == 2
                      and g1[0][5] == 2,
                      "child 2's voice claiming seat 0 reaches the parent AND "
                      "child 1, stamped 2")
            # oversize and junk
            for data in (b"SYNV" + bytes(700), b"MP2L" + bytes(0x300),
                         b"SYNG" + bytes(1400), b"\xff" * 64):
                c1.send(data)
            res.check(p.recv_all(0.4) == [] and c2.recv_all(0.2) == [],
                      "oversize / wrong length / junk: nothing forwarded")
            # flood: 2000 voice datagrams as fast as possible
            t0 = time.time()
            for i in range(2000):
                c1.send(dgram(b"SYNV", 176))
            dt = time.time() - t0
            got = len(p.recv_all(1.0))
            res.check(got <= 30 + int(dt * 30) + 2,
                      "a 2000-datagram voice flood in %.2fs: %d reach the "
                      "parent (the 30/s cap)" % (dt, got))
            c2.recv_all(0.3)
            time.sleep(1.2)
            # the flooder's other kinds are unaffected once its source budget
            # refills
            c1.send(dgram(b"MP2L", 0x90))
            res.check(len(p.recv_all(0.4)) == 1,
                      "after the flood the flooder's lockstep still flows")
            # v1 beside v2: verbatim, unstamped
            data = b"MP2L" + bytes([2, 3, 0x0D, 1]) + bytes(0x88)
            v1c.send(data)
            got = v1p.recv_all(0.4)
            res.check(got == [data], "v1 session beside it: verbatim, the "
                      "claimed slot 0x0D untouched")
            line = relay.last_stats()
            res.check("drop_length=" in line and " SYNV=" in line,
                      "stats line: %s" % line.split("rss_kb=")[-1])
        finally:
            for s in (p, c1, c2, v1p, v1c):
                s.close()
    return res


# A hub member in its own process: join, send a few ghost frames, then exit
# without a word (os._exit: no close, no goodbye), so its UDP port simply
# vanishes the way a crashed or killed game's does.
MEMBER_THAT_DIES = r"""
import socket, sys, time
host, port, code = sys.argv[1], int(sys.argv[2]), sys.argv[3].encode()
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("127.0.0.1", 0))
s.settimeout(2.0)
s.sendto(b"SMRC" + bytes([2, 2, 0xFF, 0]) + code.ljust(8, b"\0"), (host, port))
ack = s.recvfrom(64)[0]
print("seat", ack[6], flush=True)
for i in range(5):
    s.sendto(b"SYNG" + bytes([1, 0, 1, 0]) + bytes(36), (host, port))
    time.sleep(0.05)
import os
os._exit(0)
"""


def s_member_exits(relay_env=None, relay_py=None, label=""):
    """A member process exits abruptly mid-session; the rest keep hearing each
    other. On Windows the relay's next forward to the dead port comes back as
    an ICMP port-unreachable; before this was handled it stopped the relay
    receiving for good (run mp1, HUB1 S1)."""
    res = Result("socket: a member process dies, the others keep hearing "
                 "each other%s" % label)
    with RelayProc(relay_env, relay_py) as relay:
        members = [Sock(relay) for _ in range(3)]
        try:
            code = "DIEHUB01"
            acks = [m.join(code, ROLE_MEMBER, ANY) for m in members]
            res.check(all(a and a[1] == ST_OK for a in acks),
                      "three members seated")
            seat = [a[2] for a in acks]
            dead = subprocess.run(
                [sys.executable, "-c", MEMBER_THAT_DIES, "127.0.0.1",
                 str(relay.port), code], capture_output=True, timeout=20,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            res.check(dead.stdout.startswith(b"seat 3"),
                      "the fourth member joined as seat 3 and exited (%r)"
                      % dead.stdout.strip())
            # 4 s of ghost traffic at 15 Hz from the three survivors: every
            # one of these is also forwarded to the dead member's closed port
            # until its hub seat expires at 10 s.
            heard = [dict() for _ in members]
            t_end = time.time() + 4.0
            late = [dict() for _ in members]
            while time.time() < t_end:
                for m in members:
                    m.send(dgram(b"SYNG", 44))
                for k, m in enumerate(members):
                    for d in m.recv_all(0.066):
                        if d[:4] != b"SYNG":
                            continue
                        heard[k][d[5]] = heard[k].get(d[5], 0) + 1
                        if t_end - time.time() < 1.0:
                            late[k][d[5]] = late[k].get(d[5], 0) + 1
            for k in range(3):
                others = [seat[j] for j in range(3) if j != k]
                res.check(all(late[k].get(o, 0) >= 5 for o in others),
                          "member seat %d still hears %s in the last second "
                          "(%s)" % (seat[k], others, late[k]))
            line = relay.last_stats()
            res.note("relay: %s" % " ".join(
                w for w in line.split() if w.startswith(("rx=", "fwd=",
                                                         "reset="))))
        finally:
            for m in members:
                m.close()
    return res


def s_session_budget():
    """Real sockets, the budget set low: 16 sockets flooding one hub are held
    to it while a normal hub beside them hears everything."""
    budget = 2000
    res = Result("socket: 16 flooding sockets held to the session budget "
                 "(%d/s), the hub beside them untouched" % budget)
    with RelayProc({"SM64DS_RELAY_SESSION_FWD_PPS": str(budget)}) as relay:
        flood = [Sock(relay) for _ in range(16)]
        normal = [Sock(relay) for _ in range(4)]
        try:
            fa = [m.join("SOCKFLD1", ROLE_MEMBER, ANY) for m in flood]
            na = [m.join("SOCKNRM1", ROLE_MEMBER, ANY) for m in normal]
            res.check(all(a and a[1] == ST_OK for a in fa + na),
                      "16 + 4 members seated")
            for m in flood + normal:
                m.s.setblocking(False)
            late_f = 0
            per_n = [dict() for _ in normal]
            secs, warm = 5.0, 1.5
            t0 = time.time()
            nxt_f = nxt_n = t0
            while True:
                now = time.time()
                if now - t0 >= secs:
                    break
                if now >= nxt_f:            # 30/s per flooding socket
                    for m in flood:
                        m.send(dgram(b"SYNG", 44))
                    nxt_f += 1.0 / 30
                if now >= nxt_n:            # 15/s per normal socket
                    for m in normal:
                        m.send(dgram(b"SYNG", 44))
                    nxt_n += 1.0 / 15
                for m in flood:
                    while True:
                        try:
                            m.s.recvfrom(4096)
                        except OSError:
                            break
                        if now - t0 >= warm:
                            late_f += 1
                for k, m in enumerate(normal):
                    while True:
                        try:
                            d, _ = m.s.recvfrom(4096)
                        except OSError:
                            break
                        if d[:4] == b"SYNG":
                            per_n[k][d[5]] = per_n[k].get(d[5], 0) + 1
                time.sleep(0.001)
            rate = late_f / (secs - warm)
            res.note("flood hub delivered %.0f/s after %.1f s (offered %d/s)"
                     % (rate, warm, 16 * 30 * 15))
            res.check(budget * 0.7 <= rate <= budget * 1.1,
                      "the flooding hub is held to the budget (%.0f/s)" % rate)
            exp = secs * 15
            res.check(all(len(p) == 3 and min(p.values()) >= exp * 0.85
                          for p in per_n),
                      "every normal member heard the other 3 at ~15/s (%s)"
                      % [sorted(p.values()) for p in per_n])
            line = relay.last_stats()
            res.check("drop_session_cap=" in line
                      and "drop_session_cap=0 " not in line,
                      "the stats line counts drop_session_cap")
        finally:
            for m in flood + normal:
                m.close()
    return res


def run_socket():
    results = []
    tests = [s_hub16, s_security, s_member_exits, s_session_budget]
    if os.name == "nt":
        # the second line of defence on its own: the OS report left on
        tests.append(lambda: s_member_exits(
            {"SM64DS_RELAY_KEEP_CONNRESET": "1"},
            label=" (connreset left on: error_received re-arms)"))
    for t in tests:
        res = t()
        res.report()
        results.append(res)
    return results


def main(argv):
    which = argv[1] if len(argv) > 1 else "all"
    print("relay v2 suite, python %s on %s, pid %d"
          % (sys.version.split()[0], sys.platform, os.getpid()))
    t0 = time.time()
    results = []
    if which in ("all", "unit"):
        results += run_unit()
    if which in ("all", "socket"):
        results += run_socket()
    failed = [r.name for r in results if not r.ok]
    print("")
    print("%d tests, %d checks, %d failed, %.1fs"
          % (len(results), sum(r.n for r in results), len(failed),
             time.time() - t0))
    if failed:
        print("FAILED: %s" % "; ".join(failed))
        return 1
    print("SUITE PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
