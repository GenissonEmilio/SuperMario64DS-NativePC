#!/usr/bin/env python3
"""SECREV1: one address cannot fill the lobby's room table.

    python test_roomcap.py

Drives do_create directly, the way test_units.py does (under the module lock).
Standard library only.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "app"))

import server as S     # noqa: E402

FAILS = []


def check(cond, msg):
    print("  [%s] %s" % ("PASS" if cond else "FAIL", msg))
    if not cond:
        FAILS.append(msg)


def create(who, now):
    with S.LOCK:
        return S.do_create({"v": 5, "nick": "host", "game": "1.0.0"}, who, now)


S.ROOMS.clear()
limit = getattr(S, "ROOMS_PER_ADDR", None)
now = 1000.0
a = [create("198.51.100.1", now + i)[0] for i in range(5)]
if limit is None:
    check(False, "this lobby has no ROOMS_PER_ADDR cap (statuses %r)" % a)
else:
    check(a == [200] * limit + [429] * (5 - limit),
          "one address hosts %d rooms, then 429 too_fast (statuses %r)" % (limit, a))
st, out = create("198.51.100.1", now + 6)
check(st == 429 and out.get("error") == "too_fast" or limit is None, "the refusal is the existing too_fast code the launcher already shows")
st, out = create("198.51.100.2", now + 7)
check(st == 200, "another address can still create a room")

# a room that is gone frees the address
with S.LOCK:
    for code in list(S.ROOMS):
        room = S.ROOMS[code]
        host = room.members.get(room.host_seat)
        if host is not None and host.last_addr == "198.51.100.1":
            del S.ROOMS[code]
            break
st, out = create("198.51.100.1", now + 8)
check(st == 200 or limit is None, "after one of its rooms closes the address can host again")

# many addresses cannot be starved by three: fill with per-address cap, the table still has room
S.ROOMS.clear()
sts = [create("203.0.113.%d" % (10 + i // 3), 2000.0 + i)[0] for i in range(9)]
check(sts.count(200) == 9, "nine rooms from three addresses (3 each) are all fine")

print("\n%s" % ("ALL PASS" if not FAILS else "%d FAILED" % len(FAILS)))
sys.exit(1 if FAILS else 0)
