#!/usr/bin/env python3
"""Unit tests for the lobby's host-events admin endpoint (event_build, event_world, the token check). Standard library only, no sockets.

    EVENT_ADMIN_TOKEN=... EVENT_KEY=... EVENT_CMD_PORT=1 python3 test_events_admin.py   (the file sets its own values before importing)

The live round trip (lobby -> relay -> two real consoles) is run by the lane's harness; the relay's side is relay/test_events.py."""
import hashlib
import hmac
import os
import sys

os.environ["EVENT_ADMIN_TOKEN"] = "t" * 30
os.environ["EVENT_KEY"] = "k" * 20
os.environ["EVENT_CMD_PORT"] = "45999"
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "app"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "relay"))

import server as S   # noqa: E402
import relay         # noqa: E402

FAILS = []


def check(name, ok, extra=""):
    print(("ok   " if ok else "FAIL ") + name + ((" " + str(extra)) if extra and not ok else ""))
    if not ok:
        FAILS.append(name)


WORLDS = {"AAAA1111": 1, "BBBB2222": 2}


def parse(body, worlds=WORLDS):
    cmd, err = S.event_build(body, worlds)
    if err:
        return None, err
    ev, why = relay.event_parse(os.environ["EVENT_KEY"].encode(), cmd.encode("ascii"), 0, __import__("time").time())
    return ev, why


ev, why = parse({"event": "castle", "secs": 600})
check("castle builds a command the relay accepts", ev is not None and ev["arg"] == 5 and ev["world"] == "" and ev["secs"] == 600, why)
ev, why = parse({"event": "race", "secs": 900, "stars": 30, "world": 2})
check("a world number is turned into its hub code", ev is not None and ev["world"] == "BBBB2222" and ev["target"] == 30, why)
ev, why = parse({"event": "message", "text": "hi", "world": "AAAA1111"})
check("a known hub code is accepted", ev is not None and ev["world"] == "AAAA1111", why)
for body, err in (
        ({"event": "message", "text": "hi", "world": 9}, "unknown_world"),
        ({"event": "message", "text": "hi", "world": "ZZZZ9999"}, "unknown_world"),
        ({"event": "message", "text": "hi", "world": True}, "bad_world"),
        ({"event": "castle", "stars": 10}, "bad_stars"),
        ({"event": "race", "stars": 151}, "bad_stars"),
        ({"event": "race", "stars": True}, "bad_stars"),
        ({"event": "nope"}, "bad_event"),
        ({"event": "message"}, "bad_text"),
        ({"event": "music", "sublevel": 999}, "bad_sublevel"),
        ({"event": "speed", "secs": 99999}, "bad_secs"),
        ("not an object", "bad_shape")):
    got = S.event_build(body, WORLDS)[1]
    check("refused: %s -> %s" % (body, err), got == err, got)
cmd, err = S.event_build({"event": "castle"}, {"AAAA1111": 1})
check("no world means every world", err is None and cmd.split("|")[0].split(" ")[6] == "-")
# a number two builds share is ambiguous
check("a shared world number is ambiguous", S.event_world(1, {"AAAA1111": 1, "CCCC3333": 1})[1] == "ambiguous_world")
check("the token check", S.event_token_ok("t" * 30) and not S.event_token_ok("t" * 29) and not S.event_token_ok(""))
check("bearer header", S.event_bearer({"Authorization": "Bearer abc"}) == "abc" and S.event_bearer({}) == "")
check("the feature is on with token, key and port set", S.EVENT_ON)
print("RESULT", "FAIL" if FAILS else "PASS", FAILS)
sys.exit(1 if FAILS else 0)
