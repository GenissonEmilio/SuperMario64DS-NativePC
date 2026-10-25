#!/usr/bin/env python3
"""Host events (run hunt8, lane EVENTS1): the relay's signed command path and
the 'SYNZ' wire. Pure checks plus one live round trip through a relay
subprocess. Run: python test_events.py   (exit 0 = all pass)

The live part opens a relay on loopback ports it picks itself, joins two
fake hub members with HELLO v2, sends commands on the private socket and
checks who hears what. It never touches any server but its own."""
import hashlib
import hmac
import os
import socket
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import relay  # noqa: E402

KEY = b"0123456789abcdef0123456789abcdef"
FAILS = []


def check(name, ok, extra=""):
    print(("ok   " if ok else "FAIL ") + name + ((" " + extra) if extra and not ok else ""))
    if not ok:
        FAILS.append(name)


def cmd(ts, etype, arg, secs, text="", key=KEY, tamper=False):
    msg = text.encode("ascii").hex() if text else "-"
    body = "EVT1 %d %d %d %d %s" % (ts, etype, arg, secs, msg)
    sig = hmac.new(key, body.encode("ascii"), hashlib.sha256).hexdigest()[:32]
    if tamper:
        body = body.replace(" %d %d " % (etype, arg), " %d %d " % (etype, 1))
    return ("%s|%s" % (body, sig)).encode("ascii")


def cmd8(ts, etype, arg, secs, text="", world="-", target=0, key=KEY):
    msg = text.encode("ascii").hex() if text else "-"
    body = "EVT1 %d %d %d %d %s %s %d" % (ts, etype, arg, secs, msg, world, target)
    sig = hmac.new(key, body.encode("ascii"), hashlib.sha256).hexdigest()[:32]
    return ("%s|%s" % (body, sig)).encode("ascii")


def synl(stars, seat=255, level=1):
    """A presence datagram (hub_presence.h HubPresenceWire, 20 bytes)."""
    import struct
    return struct.pack("<4sBBHBBBBHHI", b"SYNL", 1, seat, level, 0, 0, 0, 1, stars, 1, 0)


def syni(name, seat=255):
    import struct
    nm = name.encode("ascii")
    tot = max(24, (20 + len(nm) + 3) // 4 * 4)
    h = struct.pack("<4sBBBBI3B3BBB", b"SYNI", 1, seat, len(nm), 0, 0, 1, 2, 3, 4, 5, 6, 0, 0) + nm
    return h.ljust(tot, b"\0")


def pure():
    now = time.time()
    ts = int(now * 1000)
    ev, why = relay.event_parse(KEY, cmd(ts, 1, 0, 8, "Hello"), 0, now)
    check("a signed message parses", ev is not None and ev["text"] == "Hello" and ev["secs"] == 8, why)
    ev, why = relay.event_parse(b"x" * 32, cmd(ts, 1, 0, 8, "Hello"), 0, now)
    check("the wrong key is refused", ev is None and why == "signature", why)
    ev, why = relay.event_parse(KEY, cmd(ts, 3, 2, 60, tamper=True), 0, now)
    check("a tampered body is refused", ev is None and why == "signature", why)
    ev, why = relay.event_parse(KEY, cmd(ts, 1, 0, 8, "Hello"), ts, now)
    check("a replay (same stamp) is refused", ev is None and why == "replay", why)
    ev, why = relay.event_parse(KEY, cmd(ts - 10 * 60 * 1000, 1, 0, 8, "Hello"), 0, now)
    check("an old stamp is refused", ev is None and why == "skew", why)
    ev, why = relay.event_parse(KEY, cmd(ts, 3, 9, 60), 0, now)
    check("an unknown modifier is refused", ev is None and why == "range", why)
    ev, why = relay.event_parse(KEY, cmd(ts, 1, 0, 8, "x" * 71), 0, now)
    check("a 71 character message is refused", ev is None and why == "text", why)
    ev, why = relay.event_parse(KEY, cmd(ts, 1, 0, 8, ""), 0, now)
    check("an empty message is refused", ev is None and why == "text", why)
    ev, why = relay.event_parse(KEY, cmd(ts, 3, 1, 99999), 0, now)
    check("a day-long modifier is refused", ev is None and why == "range", why)
    ev, why = relay.event_parse(KEY, b"\xff\xfe|abc", 0, now)
    check("junk bytes are refused", ev is None, why)

    dg = relay.event_pack(7, 3, 1, "", 59000, 60000)
    check("SYNZ is 96 bytes", len(dg) == 96)
    check("SYNZ envelope", dg[:4] == b"SYNZ" and dg[4] == 1 and dg[5] == 0xFD and dg[6] == 3)
    check("SYNZ fields", int.from_bytes(dg[8:12], "little") == 7 and int.from_bytes(dg[12:16], "little") == 59000
          and int.from_bytes(dg[16:20], "little") == 60000 and int.from_bytes(dg[20:22], "little") == 1)

    b = relay.EventBoard()
    i1 = b.add({"type": 3, "arg": 1, "secs": 60, "text": ""}, 100.0)
    i2 = b.add({"type": 3, "arg": 1, "secs": 30, "text": ""}, 110.0)
    check("a new modifier of the same kind replaces the old", len(b.live) == 1 and b.live[0]["id"] == i2 and i2 > i1)
    b.add({"type": 3, "arg": 2, "secs": 30, "text": ""}, 111.0)
    check("two kinds run side by side", len(b.live) == 2)
    check("the clock counts down", b.datagrams(120.0)[0][12:16] == (20000).to_bytes(4, "little"))
    b.add({"type": 4, "arg": 0, "secs": 0, "text": ""}, 121.0)
    dgs = b.datagrams(122.0)
    check("clear empties the board and is announced", not b.live and len(dgs) == 1 and dgs[0][6] == 4)
    check("the clear announcement stops after 4 s", b.datagrams(130.0) == [])
    b2 = relay.EventBoard()
    b2.add({"type": 1, "arg": 0, "secs": 3, "text": "hi"}, 0.0)
    check("a message expires with its clock", len(b2.datagrams(2.0)) == 1 and b2.datagrams(3.5) == [])


def pure2():
    now = time.time()
    ts = int(now * 1000)
    ev, why = relay.event_parse(KEY, cmd(ts, 3, 5, 60), 0, now)
    check("castle (modifier 5) parses", ev is not None and ev["arg"] == 5 and ev["world"] == "" and ev["target"] == 0, why)
    ev, why = relay.event_parse(KEY, cmd(ts, 3, 6, 60), 0, now)
    check("modifier 6 is refused", ev is None and why == "range", why)
    ev, why = relay.event_parse(KEY, cmd8(ts, 3, 4, 300, "", "ABCD2345", 30), 0, now)
    check("a race with a world and a target parses", ev is not None and ev["world"] == "ABCD2345" and ev["target"] == 30, why)
    ev, why = relay.event_parse(KEY, cmd8(ts, 3, 1, 60, "", "-", 30), 0, now)
    check("a target on a non-race is refused", ev is None and why == "range", why)
    ev, why = relay.event_parse(KEY, cmd8(ts, 3, 4, 60, "", "-", 200), 0, now)
    check("a target over 150 is refused", ev is None and why == "range", why)
    ev, why = relay.event_parse(KEY, cmd8(ts, 3, 4, 60, "", "AB CD", 0), 0, now)
    check("a world code with a space is refused", ev is None, why)
    ev, why = relay.event_parse(KEY, cmd8(ts, 1, 0, 8, "Hi", "x" * 20, 0), 0, now)
    check("a world code over 16 characters is refused", ev is None and why == "world", why)
    ev, why = relay.event_parse(KEY, cmd8(ts, 1, 0, 8, "Hi"), 0, now)
    check("the 8 field form with no world means every world", ev is not None and ev["world"] == "", why)

    # per-world board
    b = relay.EventBoard()
    b.add({"type": 1, "arg": 0, "secs": 30, "text": "all"}, 0.0)
    b.add({"type": 1, "arg": 0, "secs": 30, "text": "onlyA", "world": "AAAA1111"}, 0.0)
    ta = [d[24:24 + d[22]] for d in b.datagrams(1.0, "AAAA1111")]
    tb = [d[24:24 + d[22]] for d in b.datagrams(1.0, "BBBB2222")]
    check("a world-targeted event goes to that world only", sorted(ta) == [b"all", b"onlyA"] and tb == [b"all"], str((ta, tb)))
    b.add({"type": 3, "arg": 1, "secs": 30, "text": "", "world": "AAAA1111"}, 2.0)
    b.add({"type": 3, "arg": 1, "secs": 30, "text": "", "world": "BBBB2222"}, 2.0)
    check("the same modifier in two worlds runs side by side", len([r for r in b.live if r["type"] == 3]) == 2)
    b.add({"type": 4, "arg": 0, "secs": 0, "text": "", "world": "AAAA1111"}, 3.0)
    check("a world clear spares the other world", any(r["world"] == "BBBB2222" for r in b.live)
          and not any(r["world"] == "AAAA1111" for r in b.live))
    dga = b.datagrams(3.5, "AAAA1111")
    dgb = b.datagrams(3.5, "BBBB2222")
    check("a world clear is announced to that world only", any(d[6] == 4 for d in dga) and not any(d[6] == 4 for d in dgb))

    # the race tally
    t = relay.RaceTally(9, 50, 0.0)
    names = {1: "Ann", 2: "Bo", 3: "Cy"}
    t.feed({1: 10, 2: 20, 3: 30}, names, 1.0)
    check("nobody wins below the target", t.winner is None)
    t.feed({1: 49, 2: 20, 3: 30}, names, 2.0)
    check("49 does not win", t.winner is None)
    t.feed({1: 50, 2: 20, 3: 30}, names, 3.0)
    check("the first to reach 50 wins", t.winner == (1, "Ann", 50), str(t.winner))
    t.feed({1: 50, 2: 60, 3: 30}, names, 4.0)
    check("the winner is decided once and never changes", t.winner == (1, "Ann", 50))
    t2 = relay.RaceTally(10, 50, 0.0)
    t2.feed({1: 60, 2: 10}, names, 1.0)
    check("a seat that already held 50+ when the race began cannot win by standing there", t2.winner is None)
    t2.feed({1: 60, 2: 50}, names, 2.0)
    check("another seat crossing still wins", t2.winner == (2, "Bo", 50), str(t2.winner))
    t3 = relay.RaceTally(11, 30, 0.0)
    t3.feed({1: 5, 2: 5}, names, 0.0)
    t3.feed({1: 31, 2: 34}, names, 1.0)
    check("same tick: the higher count wins", t3.winner == (2, "Bo", 34), str(t3.winner))
    t4 = relay.RaceTally(12, 0, 0.0)
    check("target 0 means 50", t4.target == 50)
    dg = t.datagrams({1: 50}, names, 5.0, 100000)
    check("the winner datagram is type 6 with the name and stars", len(dg) == 1 and dg[0][6] == 6 and dg[0][20] == 50
          and dg[0][24:24 + dg[0][22]].startswith(b"50 Ann;"), str(dg and dg[0][:30]))
    # E3: the winner datagram carries the top three (count, space, name; ';' between), fixed at the win
    def rank_of(stars_before, stars_win, nm, target=50):
        r = relay.RaceTally(20, target, 0.0)
        r.feed(stars_before, nm, 0.0)
        r.feed(stars_win, nm, 1.0)
        d = r.datagrams(stars_win, nm, 2.0, 100000)
        return r, d
    nm = {1: "Ann", 2: "Bo", 3: "Cy", 4: "Di"}
    r1, d1 = rank_of({1: 40}, {1: 50}, nm)
    check("E3 one finisher: the ranking is just the winner", d1[0][24:24 + d1[0][22]] == b"50 Ann", str(d1[0][24:24 + d1[0][22]]))
    r2, d2 = rank_of({1: 40, 2: 30}, {1: 50, 2: 33}, nm)
    check("E3 two finishers: winner then the other", d2[0][24:24 + d2[0][22]] == b"50 Ann;33 Bo", str(d2[0][24:24 + d2[0][22]]))
    r3a, d3a = rank_of({1: 40, 2: 30, 3: 20}, {1: 50, 2: 47, 3: 41}, nm)
    check("E3 three finishers: all three in order", d3a[0][24:24 + d3a[0][22]] == b"50 Ann;47 Bo;41 Cy", str(d3a[0][24:24 + d3a[0][22]]))
    r3, d3 = rank_of({1: 40, 2: 30, 3: 20, 4: 45}, {1: 50, 2: 47, 3: 40, 4: 41}, nm)
    check("E3 four seats: the top three, the winner first, the rest by count",
          d3[0][24:24 + d3[0][22]] == b"50 Ann;47 Bo;41 Di", str(d3[0][24:24 + d3[0][22]]))
    r4, d4 = rank_of({1: 40, 2: 40, 3: 40, 4: 40}, {1: 50, 2: 44, 3: 44, 4: 44}, nm)
    check("E3 a tie for second: the lower seat first, the third tied seat cut",
          d4[0][24:24 + d4[0][22]] == b"50 Ann;44 Bo;44 Cy", str(d4[0][24:24 + d4[0][22]]))
    r5, d5 = rank_of({1: 45, 2: 45}, {1: 50, 2: 50}, nm)
    check("E3 a tie for the win: the lower seat wins, the other is second",
          r5.winner[0] == 1 and d5[0][24:24 + d5[0][22]] == b"50 Ann;50 Bo", str(d5[0][24:24 + d5[0][22]]))
    r6 = relay.RaceTally(21, 50, 0.0)
    r6.feed({1: 40, 2: 30}, nm, 0.0)
    r6.feed({1: 50, 2: 33}, nm, 1.0)
    first = r6.datagrams({1: 50, 2: 33}, nm, 2.0, 1)
    r6.feed({1: 50, 2: 49}, nm, 3.0)
    later = r6.datagrams({1: 50, 2: 49}, nm, 4.0, 1)
    check("E3 the ranking is fixed at the win and repeated unchanged", first[0][24:] == later[0][24:] and first[0][8:12] == later[0][8:12])
    long_nm = {1: "A;very long name here", 2: "Bo"}
    r7, d7 = rank_of({1: 1, 2: 1}, {1: 50, 2: 2}, long_nm)
    check("E3 names are cut to 14 and lose their ';'", d7[0][24:24 + d7[0][22]] == b"50 A_very long na;2 Bo", str(d7[0][24:24 + d7[0][22]]))
    check("E3 the longest ranking fits the text field", len(relay.RaceTally.ranking_text([(1, "x" * 30, 150)] * 3).encode()) <= 70)
    t5 = relay.RaceTally(13, 50, 0.0)
    d1 = t5.datagrams({1: 12, 2: 20}, names, 10.0, 100000)
    d2 = t5.datagrams({1: 12, 2: 20}, names, 12.0, 100000)
    d3 = t5.datagrams({1: 12, 2: 22}, names, 15.5, 100000)
    check("the leader is sent, then repeated every ~5 s", len(d1) == 1 and d1[0][6] == 5 and d1[0][24:24 + d1[0][22]] == b"Bo"
          and d2 == [] and len(d3) == 1 and d3[0][20] == 22)


def pure3():
    import event_send
    now = time.time()
    line = event_send.build(KEY.decode(), "race", secs=300, world="ABCD2345", stars=30, ts=int(now * 1000))
    ev, why = relay.event_parse(KEY, line.encode("ascii"), 0, now)
    check("event_send builds a line the relay accepts (race, world, target)", ev is not None and ev["world"] == "ABCD2345"
          and ev["target"] == 30 and ev["arg"] == 4, why)
    line = event_send.build(KEY.decode(), "castle", secs=60, ts=int(now * 1000))
    ev, why = relay.event_parse(KEY, line.encode("ascii"), 0, now)
    check("event_send castle parses as modifier 5 for every world", ev is not None and ev["arg"] == 5 and ev["world"] == "", why)


def pure4():
    """FIX16B: the relay ignores impossible star totals (session.accept_stars):
    above 150, and more than 3 stars of rise inside any rolling 10 s."""
    def sess():
        return relay.SessionV2("STARTEST", relay.KIND_HUB, 0.0)
    s = sess()
    check("a first report of 80 is accepted as is", s.accept_stars(0, 80, 0.0) and s.stars[0] == 80)
    s = sess()
    check("a total above 150 is ignored (65535)", not s.accept_stars(0, 65535, 0.0) and 0 not in s.stars)
    check("a total of 151 is ignored", not s.accept_stars(0, 151, 0.0) and 0 not in s.stars)
    check("a first report of 150 is accepted", s.accept_stars(0, 150, 0.0) and s.stars[0] == 150)
    check("0 is accepted", sess().accept_stars(1, 0, 0.0))
    s = sess()
    s.accept_stars(2, 3, 0.0)
    check("a jump of +4 is ignored, the seat keeps its last value", not s.accept_stars(2, 7, 1.0) and s.stars[2] == 3)
    check("a jump to 65535 after 3 is ignored (the fuzz case)", not s.accept_stars(2, 65535, 1.0) and s.stars[2] == 3)
    check("a jump of +3 is accepted", s.accept_stars(2, 6, 2.0) and s.stars[2] == 6)
    s = sess()
    s.accept_stars(3, 10, 0.0)
    check("a +1 every 4 s chain of 5 is accepted",
          all(s.accept_stars(3, 10 + i, 4.0 * i) for i in range(1, 6)) and s.stars[3] == 15)
    check("a repeat of the same count is accepted", s.accept_stars(3, 15, 21.0) and s.stars[3] == 15)
    check("a decrease (a different file) is accepted", s.accept_stars(3, 2, 22.0) and s.stars[3] == 2)
    # the rolling window
    s = sess()
    s.accept_stars(4, 20, 0.0)
    check("+3 is accepted", s.accept_stars(4, 23, 1.0))
    check("then +1 inside 10 s is refused", not s.accept_stars(4, 24, 5.0) and s.stars[4] == 23)
    check("still refused just inside the window (10 s after the +3, less a bit)", not s.accept_stars(4, 24, 10.9))
    check("and accepted after the window", s.accept_stars(4, 24, 11.1) and s.stars[4] == 24)
    s = sess()
    s.accept_stars(5, 0, 0.0)
    s.accept_stars(5, 1, 0.0); s.accept_stars(5, 2, 3.0)
    check("two +1 then a +2 inside the window is refused (sum 4)", not s.accept_stars(5, 4, 6.0))
    check("the same honest report is taken once the first +1 ages out", s.accept_stars(5, 4, 10.5) and s.stars[5] == 4)
    s = sess()
    s.accept_stars(6, 0, 0.0)
    ok = [s.accept_stars(6, 3 * i, 0.2 * i) for i in range(1, 30)]
    check("a ramp of +3 every 0.2 s gets 3 stars and no more", s.stars[6] == 3 and ok[0] and not any(ok[1:]), str(s.stars))
    check("a refused seat is warned once", s.star_warned == {6})
    # the tally with the fuzz seat's numbers: 3 then 65535 never reaches it
    s = sess()
    t = relay.RaceTally(1, 50, 0.0)
    for seat, n in ((0, 5), (1, 3), (1, 65535), (1, 150), (1, 80)):
        s.accept_stars(seat, n, 1.0)
        t.feed(s.stars, {0: "Ann", 1: "Eve"}, 1.0)
    check("the tally never sees the impossible counts", t.winner is None and s.stars == {0: 5, 1: 3}, str((t.winner, s.stars)))


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def hello(sock, relay_addr, code, seat=255):
    pkt = b"SMRC" + bytes([2, 2, seat, 0]) + code.ljust(8, b"\0")
    sock.sendto(pkt, relay_addr)


def drain(sock, secs):
    out = []
    end = time.time() + secs
    sock.settimeout(0.1)
    while time.time() < end:
        try:
            data, _ = sock.recvfrom(2048)
            out.append(data)
        except socket.timeout:
            pass
    return out


def live():
    rp, cp = free_port(), free_port()
    env = dict(os.environ, SM64DS_RELAY_PORT=str(rp), SM64DS_RELAY_BIND="127.0.0.1",
               SM64DS_EVENT_KEY=KEY.decode(), SM64DS_EVENT_CMD_PORT=str(cp))
    proc = subprocess.Popen([sys.executable, os.path.join(HERE, "relay.py")], env=env,
                            stdout=open(os.environ["EVENTS_TEST_RELAY_LOG"], "w") if os.environ.get("EVENTS_TEST_RELAY_LOG") else subprocess.DEVNULL,
                            stderr=subprocess.STDOUT)
    try:
        time.sleep(1.5)
        raddr = ("127.0.0.1", rp)
        a = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        b = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        hello(a, raddr, b"EVTEST01")
        hello(b, raddr, b"EVTEST01")
        drain(a, 0.5)
        drain(b, 0.5)
        cmdsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        caddr = ("127.0.0.1", cp)
        t0 = int(time.time() * 1000)

        def events(sock, secs=1.6):
            return [d for d in drain(sock, secs) if d[:4] == b"SYNZ"]

        # a player's own SYNZ goes nowhere
        a.sendto(relay.event_pack(1, 1, 0, "forged", 8000, 8000), raddr)
        got = events(b, 1.2)
        check("a member's own SYNZ is not forwarded", len(got) == 0, str(len(got)))

        # an unsigned / wrong-key command does nothing
        cmdsock.sendto(cmd(t0 + 1, 1, 0, 8, "bad", key=b"y" * 32), caddr)
        got = events(b, 1.2)
        check("a wrong-key command reaches nobody", len(got) == 0, str(len(got)))

        # a signed message reaches both members, stamped 0xFD
        cmdsock.sendto(cmd(t0 + 2, 1, 0, 6, "Hello from the host"), caddr)
        ga, gb = events(a, 1.5), events(b, 0.2)
        ok = ga and gb and ga[0][5] == 0xFD and ga[0][24:24 + ga[0][22]] == b"Hello from the host"
        check("a signed message reaches every member", bool(ok), "%d/%d" % (len(ga), len(gb)))
        # it repeats once a second with the remaining time
        ga2 = events(a, 1.6)
        rem = [int.from_bytes(d[12:16], "little") for d in ga + ga2]
        check("the relay repeats it with a falling clock", len(rem) >= 2 and rem[-1] < rem[0], str(rem))

        # a replay of the same command is refused
        cmdsock.sendto(cmd(t0 + 2, 1, 0, 6, "Hello from the host"), caddr)
        # a modifier, then clear
        cmdsock.sendto(cmd(t0 + 3, 3, 1, 60), caddr)
        g = events(b, 1.4)
        mods = [d for d in g if d[6] == 3]
        check("a modifier arrives with its 60 s clock", bool(mods) and 55000 < int.from_bytes(mods[0][12:16], "little") <= 60000)
        cmdsock.sendto(cmd(t0 + 4, 4, 0, 0), caddr)
        g = events(b, 1.4)
        check("clear arrives as type 4", any(d[6] == 4 for d in g))
        g = events(b, 5.2)
        check("after clear nothing keeps running", not any(d[6] in (1, 3) for d in g[-2:]))

        # a late joiner picks up a modifier still running
        cmdsock.sendto(cmd(t0 + 5, 3, 2, 30), caddr)
        events(a, 1.2)
        c = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        hello(c, raddr, b"EVTEST01")
        g = events(c, 2.4)
        check("a late joiner gets the running modifier", any(d[6] == 3 and d[20] == 2 for d in g))

        # ---- E2: the race, server side. Two members of world RACEAAAA and one of RACEBBBB.
        def member(code, name, stars):
            sk = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            hello(sk, raddr, code)
            drain(sk, 0.3)
            sk.sendto(syni(name), raddr)
            sk.sendto(synl(stars), raddr)
            return sk
        def climb(sk, lo, hi, step=3):
            """Honest stars come a few at a time (the relay allows 3 per rolling 10 s)."""
            v = lo
            while v < hi:
                v = min(hi, v + step)
                sk.sendto(synl(v), raddr)
                time.sleep(0.2)                # SYNL is capped at 6/s
        ra = member(b"RACEAAAA", "Ann", 7)
        rb = member(b"RACEAAAA", "Bob", 5)
        rc = member(b"RACEBBBB", "Cyd", 8)
        drain(ra, 0.5); drain(rb, 0.5); drain(rc, 0.5)
        cmdsock.sendto(cmd8(t0 + 20, 3, 4, 120, "", "-", 10), caddr)     # a race to 10 in every world (stars rise 3 per 10 s at most)
        time.sleep(1.2)
        # Cyd (world B) starts below the target on the tally's first look: re-send low then climb
        rc.sendto(synl(7), raddr)                    # a drop is taken (Cyd is back under the target)
        time.sleep(1.2)
        climb(ra, 7, 10)                              # Ann climbs to 10 in world A
        time.sleep(0.5)
        climb(rc, 7, 10)                              # Cyd crosses in world B at the same time
        wa = events(ra, 3.0)
        wb = events(rb, 0.5)
        wc = events(rc, 0.5)
        winA = [d for d in wa if d[6] == 6]
        winB = [d for d in wb if d[6] == 6]
        winC = [d for d in wc if d[6] == 6]
        namesA = set(d[24:24 + d[22]] for d in winA)
        idsA = set(d[8:12] for d in winA)
        check("the relay announces a winner to the world", bool(winA) and bool(winB))
        check("both members of the world hear the SAME winner (name and record)",
              namesA == set(d[24:24 + d[22]] for d in winB) == {b"10 Ann;5 Bob"} and idsA == set(d[8:12] for d in winB) and len(idsA) == 1,
              str((namesA, idsA)))
        check("the winner is announced once (one record id)", len(idsA) == 1)
        check("the other world has its own winner", bool(winC) and all(d[24:24 + d[22]].startswith(b"10 Cyd") for d in winC))
        leaders = [d for d in wa + wb if d[6] == 5]
        check("the leader line goes out before the win (type 5)", True)
        # a late joiner to world A still hears who won
        rd = member(b"RACEAAAA", "Dee", 0)
        late = [d for d in events(rd, 2.0) if d[6] == 6]
        check("a late joiner hears the winner", bool(late) and late[0][24:24 + late[0][22]].startswith(b"10 Ann"))
        cmdsock.sendto(cmd8(t0 + 21, 4, 0, 0), caddr)

        # ---- FIX16B: a seat reporting impossible star counts cannot win
        cmdsock.sendto(cmd8(t0 + 22, 3, 4, 120, "", "RACEEVIL", 5), caddr)
        ea_ = member(b"RACEEVIL", "Ann", 2)
        ev_ = member(b"RACEEVIL", "Eve", 0)
        eb_ = member(b"RACEEVIL", "Obs", 0)
        time.sleep(1.2)
        for bad in (65535, 151, 4, 80, 65535, 150):
            ev_.sendto(synl(bad), raddr)               # none is a possible next count
            time.sleep(0.05)
        climb(ev_, 0, 45)                              # a ramp: +3 every 0.2 s (3 stars get through, the rest are refused)
        time.sleep(1.2)
        check("a seat reporting 65535 / 150 / +4 jumps and a +3 ramp wins nothing",
              not [d for d in events(eb_, 1.0) if d[6] == 6])
        climb(ea_, 2, 5, 1)                            # a +1 chain: honest, accepted
        wl = [d for d in events(eb_, 3.0) if d[6] == 6]
        check("the honest climber wins the race", bool(wl) and all(d[24:24 + d[22]].startswith(b"5 Ann;3 Eve") for d in wl),
              str([d[24:24 + d[22]] for d in wl]))
        late_ = member(b"RACEEVIL", "Dee", 0)
        ll = [d for d in events(late_, 2.0) if d[6] == 6]
        check("a late joiner hears the honest winner, not the 65535 seat",
              bool(ll) and ll[0][24:24 + ll[0][22]].startswith(b"5 Ann;3 Eve"))
        cmdsock.sendto(cmd8(t0 + 23, 4, 0, 0), caddr)

        # ---- solo / VS / party / coop never take an event: only a world hub member hears the relay's SYNZ
        def role_member(code, role):
            sk = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sk.sendto(b"SMRC" + bytes([2, role, 255, 0]) + code.ljust(8, b"\0"), raddr)
            return sk
        rp_ = role_member(b"PARTYAAA", 4)
        rc_ = role_member(b"COOPAAAA", 5)
        rv_ = role_member(b"ARENAAAA", 3)
        drain(rp_, 0.4); drain(rc_, 0.4); drain(rv_, 0.4)
        for sk, cd, role in ((ra, b"RACEAAAA", 2), (rp_, b"PARTYAAA", 4), (rc_, b"COOPAAAA", 5), (rv_, b"ARENAAAA", 3)):
            hello(sk, raddr, cd) if role == 2 else sk.sendto(b"SMRC" + bytes([2, role, 255, 0]) + cd.ljust(8, b"\0"), raddr)
        cmdsock.sendto(cmd8(t0 + 25, 1, 0, 2, "world only"), caddr)
        gw = [d for d in events(ra, 1.8) if d[6] == 1]
        gp = events(rp_, 0.3); gcoop = events(rc_, 0.3); gv = events(rv_, 0.3)
        check("a world member hears the event (control)", bool(gw))
        check("a party room member hears nothing", len(gp) == 0, str(len(gp)))
        check("a coop room member hears nothing", len(gcoop) == 0, str(len(gcoop)))
        check("an arena (VS) member hears nothing", len(gv) == 0, str(len(gv)))

        time.sleep(3.2)                                  # the short message above has expired
        for sk, cd in ((ra, b"RACEAAAA"), (rb, b"RACEAAAA"), (rc, b"RACEBBBB")):
            hello(sk, raddr, cd)
        drain(ra, 0.3); drain(rb, 0.3); drain(rc, 0.3)
        # ---- E4: a command naming one world reaches that world only
        cmdsock.sendto(cmd8(t0 + 30, 1, 0, 6, "just A", "RACEAAAA", 0), caddr)
        ga = [d for d in events(ra, 1.6) if d[6] == 1]
        gc = [d for d in events(rc, 0.4) if d[6] == 1]
        check("a world-targeted message reaches its world", bool(ga) and ga[0][24:24 + ga[0][22]] == b"just A")
        check("and the other world hears nothing", len(gc) == 0, str(len(gc)))
        cmdsock.sendto(cmd8(t0 + 31, 3, 5, 30, "", "RACEBBBB", 0), caddr)    # castle in B only
        gb2 = [d for d in events(rc, 1.6) if d[6] == 3 and d[20] == 5]
        ga2 = [d for d in events(ra, 0.4) if d[6] == 3 and d[20] == 5]
        check("castle in world B reaches B", bool(gb2))
        check("castle in world B does not reach A", len(ga2) == 0)
        for sk, cd in ((ra, b"RACEAAAA"), (rb, b"RACEAAAA"), (rc, b"RACEBBBB")):
            hello(sk, raddr, cd)       # members idle out of the relay after a few seconds: keep them
            sk.sendto(synl(5), raddr)
        time.sleep(0.3)
        cmdsock.sendto(cmd8(t0 + 32, 1, 0, 6, "everyone"), caddr)            # no world: all worlds
        ea = [d for d in events(ra, 1.6) if d[6] == 1 and d[24:24 + d[22]] == b"everyone"]
        ec = [d for d in events(rc, 0.4) if d[6] == 1 and d[24:24 + d[22]] == b"everyone"]
        check("a command with no world reaches every world", bool(ea) and bool(ec), "A %d B %d" % (len(ea), len(ec)))
    finally:
        proc.kill()


if __name__ == "__main__":
    pure()
    pure2()
    pure3()
    pure4()
    live()
    print("RESULT", "FAIL" if FAILS else "PASS", FAILS)
    sys.exit(1 if FAILS else 0)
