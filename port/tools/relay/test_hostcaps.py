#!/usr/bin/env python3
"""SECREV1: one host cannot use up a shared relay table (seats, sessions, buckets).

  python test_hostcaps.py                 against ./relay.py
  RELAY_PY=/path/to/relay.py python test_hostcaps.py

In process, like test_relay_v2.py's unit part: RelayProtocol is driven through
on_datagram with a recording transport and an explicit clock. The "public" hosts
below are outside every private range on purpose (the per-host caps exempt loopback
and private addresses, which test_relay_v2.py's rig uses).

Python 3 standard library only.
"""

import importlib.util
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
RELAY_PY = os.environ.get("RELAY_PY") or os.path.join(HERE, "relay.py")
sys.path.insert(0, HERE)
import test_relay_v2 as T  # noqa: E402

T.RELAY_PY = RELAY_PY

PUBLIC_A = "93.184.216.34"
PUBLIC_B = "151.101.1.69"
FAILS = []


def check(cond, msg):
    print("  [%s] %s" % ("PASS" if cond else "FAIL", msg))
    if not cond:
        FAILS.append(msg)


class HostRig(T.Rig):
    def at(self, host, port):
        return (host, port)


def run():
    mod = T.load_relay()
    # 1. seats: one host, six source ports, one world code
    rig = HostRig(mod)
    codes = []
    for i in range(6):
        ack = rig.join(rig.at(PUBLIC_A, 40000 + i), "WORLDAAA", T.ROLE_MEMBER, T.ANY)
        codes.append(ack[1] if ack else None)
    check(codes[:4] == [T.ST_OK] * 4 and codes[4:] == [T.ST_FULL] * 2,
          "one host: 4 seats in a world, the 5th and 6th are FULL (statuses %r)" % codes)
    other = rig.join(rig.at(PUBLIC_B, 41000), "WORLDAAA", T.ROLE_MEMBER, T.ANY)
    check(other is not None and other[1] == T.ST_OK, "another host still gets a seat in that world")
    again = rig.join(rig.at(PUBLIC_A, 40000), "WORLDAAA", T.ROLE_MEMBER, T.ANY)
    check(again is not None and again[1] == T.ST_OK, "a seated source port repeating its HELLO is still acked (keepalive)")

    # 2. sessions: one host opens nine
    rig = HostRig(mod)
    st = []
    for i in range(9):
        ack = rig.join(rig.at(PUBLIC_A, 42000 + i), "FLOOD%03d" % i, T.ROLE_MEMBER, T.ANY)
        st.append(ack[1] if ack else None)
    check(st[:8] == [T.ST_OK] * 8 and st[8] == T.ST_FULL, "one host: 8 sessions, the 9th is FULL (%r)" % st)
    fresh = rig.join(rig.at(PUBLIC_B, 43000), "LEGIT001", T.ROLE_MEMBER, T.ANY)
    check(fresh is not None and fresh[1] == T.ST_OK, "another host can still open a session")

    # 3. buckets: one host sprays 300 source ports, another host still gets in
    rig = HostRig(mod)
    for i in range(300):
        rig.send(rig.at(PUBLIC_A, 20000 + i), b"x", dt=0.0001)
    held = sum(1 for a in rig.relay.buckets if a[0] == PUBLIC_A)
    lim = getattr(mod, "MAX_BUCKETS_PER_IP", 64)
    check(held <= lim, "one host holds at most %d buckets (holds %d)" % (lim, held))
    ack = rig.join(rig.at(PUBLIC_B, 44000), "LEGIT002", T.ROLE_MEMBER, T.ANY)
    check(ack is not None and ack[1] == T.ST_OK, "another host is served after the spray")
    # the table itself: 9000 sources from 9000/64 hosts would fill it, one host cannot
    rig2 = HostRig(mod)
    for i in range(9000):
        rig2.send(rig2.at(PUBLIC_A, 1024 + (i % 60000)), b"x", dt=0.00001)
    ack = rig2.join(rig2.at(PUBLIC_B, 45000), "LEGIT003", T.ROLE_MEMBER, T.ANY)
    check(ack is not None and ack[1] == T.ST_OK, "9000 junk datagrams from one host do not lock the relay out for everyone")

    # 4. loopback and private hosts are exempt (the local harnesses seat 32 from 127.0.0.1)
    rig = HostRig(mod)
    ok = 0
    for i in range(32):
        ack = rig.join(rig.at("127.0.0.1", 46000 + i), "LOCAL001", T.ROLE_MEMBER, T.ANY)
        ok += 1 if ack and ack[1] == T.ST_OK else 0
    check(ok == 32, "loopback fills a 32 seat world (%d)" % ok)
    ok = 0
    for i in range(12):
        ack = rig.join(rig.at("192.168.1.20", 47000 + i), "LAN00001", T.ROLE_MEMBER, T.ANY)
        ok += 1 if ack and ack[1] == T.ST_OK else 0
    check(ok == 12, "a private LAN address is exempt too (%d of 12)" % ok)

    # 5. a household can be raised with the env knob
    mod2 = T.load_relay({"SM64DS_RELAY_MAX_SEATS_PER_IP": "8"})
    rig = HostRig(mod2)
    ok = sum(1 for i in range(8) if (rig.join(rig.at(PUBLIC_A, 48000 + i), "HOUSE001", T.ROLE_MEMBER, T.ANY) or (0, 9))[1] == T.ST_OK)
    check(ok == 8 and getattr(mod2, "MAX_SEATS_PER_IP", 0) == 8, "SM64DS_RELAY_MAX_SEATS_PER_IP=8 seats eight from one host (%d)" % ok)


run()
print("\n%s" % ("ALL PASS" if not FAILS else "%d FAILED" % len(FAILS)))
sys.exit(1 if FAILS else 0)
