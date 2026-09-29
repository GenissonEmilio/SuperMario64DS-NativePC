#!/usr/bin/env python3
"""Security regressions for sm64ds-lobby, over raw sockets. Stdlib only.

    python3 test_security.py

Two holes an independent review found at service tip 7a5c8ffb7, each re-run here
as a guard so neither can come back:

  BLOCKER 1  a slow body could pin a worker thread forever, and the rate
             limiter could not even deliver its own 429 because the refusal
             drained the body it was refusing first.
  BLOCKER 2  X-Forwarded-For was read from its FIRST entry, the one entry a
             client controls, so every per-address limit was forgeable.

Plus the honest-cap regression: after the fix, the per-address limit still
engages for a real client.

RAW SOCKETS THROUGHOUT. The whole point of these cases is a request that a
well-behaved client library would never send -- a body announced and then not
sent, a forged proxy header -- so nothing here goes through a convenience
wrapper that would fix it on the way out.

XFF IS HOW ONE MACHINE PRETENDS TO BE MANY. The server trusts X-Forwarded-For
(TRUST_XFF=1, the production default behind Caddy), so a single entry in that
header is read as the client address. That lets this test stand in for several
distinct clients, and it lets it stand in for Caddy by appending a constant
"real peer" entry after a forged one -- which is exactly the shape the fix
turns on.
"""

import json
import socket
import sys
import time

from test_client import Spawned, Client, pick_port

PASSED = []
FAILED = []


def check(name, ok, detail=""):
    (PASSED if ok else FAILED).append(name)
    print("%-5s %s%s" % ("ok" if ok else "FAIL", name,
                         "" if ok else "   <- " + str(detail)))
    return ok


def raw_head(port, verb, announced, xff=None, ctype="application/json"):
    """Open a socket and send the request line and headers and the blank line,
    announcing `announced` body bytes but sending NONE of them yet. Returns the
    connected socket for the caller to dribble, or read, or abandon."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10)
    s.connect(("127.0.0.1", port))
    lines = ["POST /port/lobby/%s HTTP/1.1" % verb,
             "Host: 127.0.0.1",
             "Content-Type: %s" % ctype,
             "Content-Length: %d" % announced]
    if xff is not None:
        lines.append("X-Forwarded-For: %s" % xff)
    head = ("\r\n".join(lines) + "\r\n\r\n").encode("ascii")
    s.sendall(head)
    return s


def read_reply(s, timeout):
    """Read whatever the server sends, until EOF or `timeout`. Returns the text,
    or "" if nothing arrived. A reset after a partial read still returns what
    was read."""
    s.settimeout(timeout)
    got = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            got += chunk
            if b"\r\n\r\n" in got:
                # headers are in; one more short read for the small body, then stop
                s.settimeout(0.4)
                try:
                    got += s.recv(4096)
                except OSError:
                    pass
                break
    except OSError:
        pass
    return got.decode("latin-1")


def status_of(reply):
    if not reply.startswith("HTTP/"):
        return 0
    try:
        return int(reply.split(" ", 2)[1])
    except (IndexError, ValueError):
        return 0


def quick_post(c, verb, obj, xff):
    """One POST with a chosen X-Forwarded-For, returning (status, parsed).

    A refusal closes the connection at once (see _reject), and on Windows the
    client can see that close as WinError 10053 before it reads the answer.
    That is a refused request, so it comes back as status 0, never a crash."""
    try:
        status, text = c.raw("POST", "/port/lobby/" + verb,
                             json.dumps(obj).encode(),
                             extra=(("X-Forwarded-For", xff),))
    except ConnectionError:
        return 0, {}
    try:
        return status, json.loads(text)
    except ValueError:
        return status, {"_raw": text}


# ---------------------------------------------------------- BLOCKER 1


def blocker1():
    print("\n== BLOCKER 1: a slow body must not pin a thread, and a refusal "
          "must not block on the body it is refusing ==")
    # A short read timeout so the test is quick; tiny budgets and a tiny
    # bad-sender threshold so one address can be pushed to `ignored` in a few
    # requests. Everything else is the shipped default.
    port = pick_port()
    sp = Spawned(port, {
        "HANDLER_TIMEOUT_S": "3",
        "RATE_REQ_PER_S": "1", "RATE_BURST": "2",
        "BAD_LIMIT": "3", "BAD_WINDOW_S": "10", "BAD_IGNORE_S": "10",
    })
    c = sp.wait_ready()
    try:
        # (a) THE REFUSAL PATH. Push one address past its budget, confirm it is
        #     now getting `ignored`, then open ONE slow-body connection from it
        #     and time how long its 429 takes to arrive. Before the fix this was
        #     "nothing after 6 s", because the 429 drained the body first.
        burned = "10.9.9.1"
        last = None
        for _ in range(12):
            last = quick_post(c, "create",
                              {"v": 5, "nick": "x", "game": "t-1.0"}, burned)
        check("an address can be pushed to 429 ignored",
              last is not None and last[0] == 429
              and last[1].get("error") == "ignored", last)

        s = raw_head(port, "create", 4000, xff=burned)
        s.sendall(b'{"v":1,')          # a few body bytes, then nothing more
        t0 = time.time()
        reply = read_reply(s, timeout=6)
        dt = time.time() - t0
        s.close()
        check("the over-budget slow sender gets its 429 fast, WITHOUT the "
              "server draining the body first",
              status_of(reply) == 429 and dt < 2.0,
              "status=%s after %.2fs" % (status_of(reply), dt))

        # (b) THE THREAD. A valid, in-budget request that then dribbles its body
        #     must be DROPPED at the read timeout, not held forever.
        s = raw_head(port, "create", 4000, xff="10.9.9.2")
        s.sendall(b'{"v":1,"nick":"ab')   # partial, never completed
        t0 = time.time()
        reply = read_reply(s, timeout=8)   # server should close ~3 s in
        dt = time.time() - t0
        s.close()
        check("a valid request that dribbles its body is dropped at the read "
              "timeout, not held",
              dt < 7.0 and dt > 1.0,
              "connection closed after %.2fs (read_timeout=3s)" % dt)

        # (c) MANY of them at once, and the server still serves and then fully
        #     recovers. Locally there is no pid cap to hit -- that is the
        #     container's, and the reviewer hit it -- so what is proved here is
        #     the mechanism that stops it: held connections do not wedge the
        #     server, and they are all gone after the timeout.
        held = []
        for i in range(25):
            hs = raw_head(port, "create", 4000, xff="10.8.%d.%d" % (i, i))
            hs.sendall(b'{')
            held.append(hs)
        gt0 = time.time()
        hstatus, _ = c.get("health")
        gdt = time.time() - gt0
        check("health still answers promptly while 25 slow bodies are held",
              hstatus == 200 and gdt < 2.0, "status=%s in %.2fs" % (hstatus, gdt))
        # Let them time out and be reaped.
        time.sleep(4)
        for hs in held:
            try:
                hs.close()
            except OSError:
                pass
        st, body = c.post("create", {"v": 5, "nick": "after", "game": "t-1.0"})
        check("after the held connections time out the server serves normally",
              st == 200, (st, body))
    finally:
        sp.stop()


# ---------------------------------------------------------- BLOCKER 2


# A join to a room nobody holds: well formed at contract v5, so it reaches the
# join limiter and then the room lookup (404) when the limiter lets it through.
JOIN_BODY = {"v": 5, "room": "ZZZZZZ", "nick": "n", "game": "t-1.0"}


def blocker2():
    print("\n== BLOCKER 2: X-Forwarded-For must be read from the entry Caddy "
          "controls, not the entry the client controls ==")
    # A tiny join budget so the cap is quick to reach.
    port = pick_port()
    sp = Spawned(port, {"RATE_JOIN_PER_MIN": "5", "RATE_REQ_PER_S": "1000",
                        "RATE_BURST": "2000"})
    c = sp.wait_ready()
    try:
        REAL = "203.0.113.9"      # the peer Caddy would append, last in the trail

        # (a) THE FIX. 30 joins, each with a DIFFERENT forged first entry but the
        #     same real peer appended last, the way Caddy produces the header.
        #     The forged first entries must NOT buy 30 separate budgets.
        codes = {}
        for i in range(30):
            forged = "%d.%d.%d.%d" % (i + 1, i + 2, i + 3, i + 4)
            st, _ = quick_post(c, "join", JOIN_BODY,
                               "%s, %s" % (forged, REAL))
            codes[st] = codes.get(st, 0) + 1
        check("forged FIRST entries do not bypass the join cap when the real "
              "peer is the LAST entry (behind Caddy)",
              codes.get(429, 0) > 0,
              "status counts across 30 forged joins: %s" % codes)
        check("and only about the budget got through before the cap engaged",
              codes.get(404, 0) <= 8,
              "%d joins reached the room lookup (budget is 5/min)"
              % codes.get(404, 0))

        # (b) THE REGRESSION. A single honest address is still capped -- the fix
        #     did not turn legitimate rate limiting off.
        time.sleep(1)
        honest = {}
        for _ in range(30):
            st, _ = quick_post(c, "join", JOIN_BODY, "198.51.100.7")
            honest[st] = honest.get(st, 0) + 1
        check("an honest single address is still rate-limited",
              honest.get(429, 0) > 0, honest)

        # (c) THE CAVEAT, measured rather than asserted. With NO proxy appending
        #     a real entry -- i.e. direct exposure, which the compose warns
        #     against -- a single forged entry IS the last entry, so it is still
        #     forgeable. This is why the compose ships TRUST_XFF=1 only because
        #     it is behind Caddy, and the README says set TRUST_XFF=0 for any
        #     deployment reachable directly. Recorded, not gated.
        time.sleep(1)
        direct = {}
        for i in range(30):
            forged = "%d.%d.%d.%d" % (100 + i, i, i, i)
            st, _ = quick_post(c, "join", JOIN_BODY,
                               forged)   # single entry, no appended peer
            direct[st] = direct.get(st, 0) + 1
        print("info  direct-exposure control (no proxy appending a real peer): "
              "%s -- forgeable by design, which is why TRUST_XFF must be 0 off "
              "Caddy" % direct)
    finally:
        sp.stop()


# ------------------------------------------------------ v5 WORLD POOL


def compose_env():
    """The environment docker-compose.yml ships, read from the file itself so
    these rows run at the SHIPPED settings and cannot drift from them (review
    REVLOBBY5: the first version of this section narrowed the pool and the
    join budget, which hid the flood)."""
    import os
    import re
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "docker-compose.yml")
    env, inside = {}, False
    for line in open(path, encoding="utf-8"):
        if re.match(r"^\s+environment:\s*$", line):
            inside = True
            continue
        if inside:
            m = re.match(r'^\s+([A-Z_][A-Z0-9_]*):\s*"([^"]*)"\s*$', line)
            if m:
                env[m.group(1)] = m.group(2)
            elif re.match(r"^\s+#", line) or not line.strip():
                continue
            else:
                break
    env.pop("LISTEN_PORT", None)
    return env


REAL = "1.0.0-0123abcd"     # the build id the release step would list


def world_pool():
    """Review REVLOBBY5 S2, at the SHIPPED compose settings: the world pool
    cannot be filled or padded from one or two home connections, a household
    of four behind one connection plays, and unknown builds are refused.

    Each row gets a fresh server (the compose env plus the release step's
    GAME_IDS line), and what the attacker holds is read from /health
    (`worlds`, `world_members` = live members over the whole pool) rather than
    from the attacker's own pings, which the bad-sender rule is by then
    refusing."""
    print("\n== v5 WORLD POOL at the shipped docker-compose settings ==")
    shipped = compose_env()
    print("info  compose env: %s" % ", ".join(
        "%s=%s" % kv for kv in sorted(shipped.items())
        if kv[0] in ("GAME_IDS", "RELAY_V", "MAX_WORLDS", "WORLD_SEATS",
                     "WORLD_MEMBER_S", "WORLD_EMPTY_S", "WORLD_ADDR_TOKENS",
                     "RATE_JOIN_PER_MIN", "RATE_REQ_PER_S", "BAD_LIMIT",
                     "TRUST_XFF")))
    released = dict(shipped)
    released["GAME_IDS"] = REAL

    def fresh(env):
        sp = Spawned(pick_port(), env)
        return sp, sp.wait_ready()

    def world(c, who, game=REAL, skip=None, nick="x"):
        body = {"v": 5, "nick": nick, "game": game}
        if skip:
            body["skip"] = skip
        return quick_post(c, "world", body, who)

    # (0) The compose file AS SHIPPED (GAME_IDS empty until the release step
    #     writes it): the world pool is off, code rooms work.
    sp, c = fresh(shipped)
    try:
        st, h = c.get("health")
        st1, w = world(c, "198.51.100.1")
        st2, r = quick_post(c, "create", {"v": 5, "nick": "a", "game": REAL},
                            "198.51.100.1")
        check("compose as shipped (GAME_IDS empty): world pool off (503 "
              "world_off), code rooms work",
              h.get("world_open") is False and st1 == 503
              and w.get("error") == "world_off" and st2 == 200,
              (h.get("world_open"), st1, w, st2))
    finally:
        sp.stop()

    # (1) THE REVIEWER'S FLOOD: one address, 64 world calls with made-up
    #     build ids.
    sp, c = fresh(released)
    try:
        st, h = c.get("health")
        check("with GAME_IDS set the pool is open at the shipped knobs",
              h.get("world_open") is True and h.get("world_addr_tokens") == 4
              and h.get("worlds") == 0, h)
        codes = {}
        for i in range(64):
            st, _ = world(c, "203.0.113.66", game="fake-%d" % i)
            codes[st] = codes.get(st, 0) + 1
            time.sleep(0.25)
        st, h = c.get("health")
        st2, real = world(c, "198.51.100.200", nick="real")
        print("info  (1) status counts %s, worlds %s, real player %s world %s"
              % (codes, h.get("worlds"), st2, real.get("world_no")))
        check("one address, 64 made-up builds: not one world opens (409 "
              "unknown_game, then the bad-sender rule), a real player gets "
              "world 1",
              codes.get(200, 0) == 0 and h.get("worlds") == 0
              and st2 == 200 and real.get("world_no") == 1,
              (codes, h, st2, real))
        st, out = quick_post(c, "create", {"v": 5, "nick": "x",
                                           "game": "fake-99"}, "198.51.100.77")
        check("an unknown build on create is 409 unknown_game",
              st == 409 and out.get("error") == "unknown_game", (st, out))
    finally:
        sp.stop()

    # (2) THE REAL BUILD FROM ONE ADDRESS (review H9b): 64 calls, skip
    #     alternating to spread over worlds.
    sp, c = fresh(released)
    try:
        codes, last = {}, None
        for i in range(64):
            st, out = world(c, "203.0.113.77", skip=last)
            codes[st] = codes.get(st, 0) + 1
            if st == 200:
                last = out["code"]
            time.sleep(0.25)
        st, h = c.get("health")
        st2, real = world(c, "198.51.100.201", nick="real")
        print("info  (2) status counts %s, pool worlds %s live members %s, "
              "real player %s world %s members %s"
              % (codes, h.get("worlds"), h.get("world_members"), st2,
                 real.get("world_no"), real.get("members")))
        check("one address, real build, 64 calls with skip: the whole pool "
              "holds at most 4 live members and 2 worlds, a real player gets "
              "world 1 with room",
              h.get("world_members", 99) <= 4 and h.get("worlds", 99) <= 2
              and st2 == 200 and real.get("world_no") == 1
              and real.get("members", 99) <= 5, (codes, h, st2, real))
    finally:
        sp.stop()

    # (3) TWO ADDRESSES AT ONCE, one with the real build and skip, one with
    #     made-up builds.
    sp, c = fresh(released)
    try:
        codes, last = {}, None
        for i in range(64):
            st, out = world(c, "203.0.113.88", skip=last)
            codes[st] = codes.get(st, 0) + 1
            if st == 200:
                last = out["code"]
            st, _ = world(c, "203.0.113.99", game="fake-%d" % i)
            codes[st] = codes.get(st, 0) + 1
            time.sleep(0.25)
        st, h = c.get("health")
        st2, real = world(c, "198.51.100.202", nick="real")
        print("info  (3) status counts %s, pool worlds %s live members %s, "
              "real player %s world %s"
              % (codes, h.get("worlds"), h.get("world_members"), st2,
                 real.get("world_no")))
        check("two addresses flooding at once: at most 4 live members and 2 "
              "worlds in the pool, a real player gets world 1",
              h.get("world_members", 99) <= 4 and h.get("worlds", 99) <= 2
              and st2 == 200 and real.get("world_no") == 1,
              (codes, h, st2, real))
        # and a second pair of addresses, both on the real build (the row
        # above's members are still live, so this reads the growth)
        before = h.get("world_members", 0) + 1     # + the real player
        for i in range(64):
            for who in ("203.0.113.101", "203.0.113.102"):
                world(c, who)
            time.sleep(0.25)
        st, h = c.get("health")
        print("info  (3b) two real-build addresses: pool worlds %s live "
              "members %s (was %d)" % (h.get("worlds"),
                                       h.get("world_members"), before))
        check("two real-build addresses, 64 calls each: they add at most 8 "
              "live members to the pool (4 each)",
              h.get("world_members", 99) - before <= 8
              and h.get("worlds", 99) <= 2, (before, h))
    finally:
        sp.stop()

    # (4) FAKE-MEMBER PADDING: an observer (a real player) sits in world 1;
    #     one address asks 16 times with no skip to fill that world.
    sp, c = fresh(released)
    try:
        st, obs = world(c, "198.51.100.30", nick="observer")
        for i in range(16):
            world(c, "203.0.113.111")
            time.sleep(0.1)
        st, pong = quick_post(c, "world_ping", {"v": 5, "code": obs["code"],
                                                "token": obs["token"]},
                              "198.51.100.30")
        print("info  (4) world 1 after 16 padding asks: %s members"
              % pong.get("members"))
        check("padding world 1 from one address: it holds the observer + 4, "
              "not 17", st == 200 and pong.get("members") == 5, (st, pong))
    finally:
        sp.stop()

    # (5) A HOUSEHOLD OF 4 behind one connection, playing normally.
    sp, c = fresh(released)
    try:
        home = "198.51.100.150"
        fam = [world(c, home, nick="fam%d" % i) for i in range(4)]
        ok = all(st == 200 for st, _ in fam)
        lives = []
        for _round in range(3):
            time.sleep(1.0)
            n = 0
            for st, o in fam:
                if st == 200:
                    st2, _ = quick_post(c, "world_ping", {
                        "v": 5, "code": o["code"], "token": o["token"]}, home)
                    n += st2 == 200
            lives.append(n)
        st, h = c.get("health")
        print("info  (5) household: %s, pings live per round %s, pool live "
              "members %s" % ([st for st, _ in fam], lives,
                              h.get("world_members")))
        check("a household of 4 on one address: all 4 get world 1 and all 4 "
              "keep pinging (3 rounds)",
              ok and lives == [4, 4, 4] and h.get("world_members") == 4
              and all(o.get("world_no") == 1 for _, o in fam),
              (fam, lives, h))

        # (6) Hostile fields and guessed pings still make nothing.
        for body, want in (
                ({"v": 5, "nick": "n" * 17, "game": REAL}, "too_long"),
                ({"v": 5, "nick": "n", "game": "g" * 25}, "too_long"),
                ({"v": 5, "nick": "n", "game": "../../etc"}, "bad_game"),
                ({"v": 5, "nick": "n", "game": REAL, "skip": "A" * 400},
                 "too_long"),
                ({"v": 5, "nick": "n", "game": REAL, "admin": True},
                 "bad_field")):
            st, out = quick_post(c, "world", body, "198.51.102.1")
            check("world %s -> 400 %s" % (sorted(body), want),
                  st == 400 and out.get("error") == want, (st, out))
        st, h0 = c.get("health")
        st1, _ = quick_post(c, "world_ping", {"v": 5, "code": "ZZZZZZZZ",
                                              "token": "0" * 32},
                            "198.51.102.2")
        st2, _ = quick_post(c, "world_ping", {"v": 5,
                                              "code": fam[0][1].get("code"),
                                              "token": "0" * 32},
                            "198.51.102.2")
        st, h1 = c.get("health")
        check("guessed world_pings are 404 / 403 and open nothing",
              st1 == 404 and st2 == 403
              and h1.get("worlds") == h0.get("worlds"),
              (st1, st2, h0.get("worlds"), h1.get("worlds")))
    finally:
        sp.stop()


def main():
    blocker1()
    blocker2()
    world_pool()
    print("\n%d passed, %d failed" % (len(PASSED), len(FAILED)))
    for name in FAILED:
        print("  FAILED: %s" % name)
    return 1 if FAILED else 0


if __name__ == "__main__":
    sys.exit(main())
