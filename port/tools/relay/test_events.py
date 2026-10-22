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
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
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
    finally:
        proc.kill()


if __name__ == "__main__":
    pure()
    live()
    print("RESULT", "FAIL" if FAILS else "PASS", FAILS)
    sys.exit(1 if FAILS else 0)
