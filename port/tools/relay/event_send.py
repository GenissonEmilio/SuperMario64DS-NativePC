#!/usr/bin/env python3
"""event_send.py: put a host event on the relay from the relay's own machine.

The relay takes events on a private UDP socket bound to 127.0.0.1 (SM64DS_EVENT_CMD_PORT), signed with SM64DS_EVENT_KEY. The lobby's admin
endpoint is the normal way in (POST /port/lobby/admin/event); this script is the same thing without the lobby, for a relay box the lobby
cannot reach (the socket is loopback only). It builds exactly the line the lobby builds and sends it to the loopback socket, so run it ON the
relay box, with the same two variables the relay's unit has:

    SM64DS_EVENT_KEY=...  SM64DS_EVENT_CMD_PORT=...  python3 event_send.py castle --secs 600
    python3 event_send.py message --text "Welcome to the event" --secs 15
    python3 event_send.py music --sublevel 6 --secs 60
    python3 event_send.py speed --secs 120           (also: onehit, chaos)
    python3 event_send.py race --secs 900 --stars 50
    python3 event_send.py castle --secs 600 --world ABCD2345      one world only (the 8 character hub code from the relay log)
    python3 event_send.py clear                                    stop everything (add --world to stop one world's)

--key and --port override the environment. Nothing is printed for a secret. Exit 0 = sent (the relay answers nobody; its log says
"event: id N type T ..." for an accepted command and "event: refused a command (reason)" otherwise).
"""
import argparse
import hashlib
import hmac
import os
import socket
import sys
import time

NAMES = {"message": (1, 0), "music": (2, 0), "clear": (4, 0), "speed": (3, 1), "onehit": (3, 2), "chaos": (3, 3),
         "race": (3, 4), "castle": (3, 5)}


def build(key, name, secs=0, text="", sublevel=None, world="", stars=0, ts=None):
    etype, arg = NAMES[name]
    if name == "music":
        if sublevel is None or not 0 <= sublevel <= 255:
            raise SystemExit("music needs --sublevel 0..255")
        arg = sublevel
    if not 0 <= secs <= 3600:
        raise SystemExit("--secs is 0..3600")
    if len(text) > 70 or any(not 32 <= ord(c) < 127 for c in text):
        raise SystemExit("--text is up to 70 plain ASCII characters")
    if name == "message" and not text:
        raise SystemExit("message needs --text")
    if stars and name != "race":
        raise SystemExit("--stars is for race only")
    if not 0 <= stars <= 150:
        raise SystemExit("--stars is 0..150")
    if world and (len(world) > 16 or not all(c.isalnum() or c in "-_" for c in world)):
        raise SystemExit("--world is the hub code (letters and digits)")
    if name == "clear":
        secs = 0
    ts = int(time.time() * 1000) if ts is None else ts
    msg = text.encode("ascii").hex() if text else "-"
    line = "EVT1 %d %d %d %d %s %s %d" % (ts, etype, arg, secs, msg, world or "-", stars)
    sig = hmac.new(key.encode("ascii"), line.encode("ascii"), hashlib.sha256).hexdigest()[:32]
    return "%s|%s" % (line, sig)


def main():
    ap = argparse.ArgumentParser(description="send a signed host event to this machine's relay")
    ap.add_argument("event", choices=sorted(NAMES))
    ap.add_argument("--secs", type=int, default=0, help="how long (default: 8 s for a message, 60 s for the rest)")
    ap.add_argument("--text", default="")
    ap.add_argument("--sublevel", type=int, default=None)
    ap.add_argument("--world", default="")
    ap.add_argument("--stars", type=int, default=0, help="race target (default 50)")
    ap.add_argument("--key", default=os.environ.get("SM64DS_EVENT_KEY", ""))
    ap.add_argument("--port", type=int, default=int(os.environ.get("SM64DS_EVENT_CMD_PORT", "0") or 0))
    a = ap.parse_args()
    if len(a.key) < 16 or not a.port:
        raise SystemExit("need SM64DS_EVENT_KEY (16+ characters) and SM64DS_EVENT_CMD_PORT, or --key and --port")
    cmd = build(a.key, a.event, a.secs, a.text, a.sublevel, a.world, a.stars)
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.sendto(cmd.encode("ascii"), ("127.0.0.1", a.port))
    finally:
        s.close()
    print("sent %s%s" % (a.event, (" to world " + a.world) if a.world else " to every world"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
