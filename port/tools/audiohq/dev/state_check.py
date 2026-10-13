#!/usr/bin/env python3
"""Host test of audio_check.compare_arena: early load-time differences between two exe images
are reported but pass; a difference in the settled window fails."""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import audio_check as A

N, DUMP = 900, 898
base = [(f, "e0010824", "%08x" % (f * 7919)) for f in range(N)]

def tip(diff_frames):
    return [(f, "c0283b6f", ("%08x" % (f * 7919 + 1)) if f in diff_frames else "%08x" % (f * 7919)) for f in range(N)]

cases = [
    ("identical", set(), True),
    ("early difference f0..f500 (l6 case)", set(range(0, 501)), True),
    ("difference only at f897", {897}, False),
    ("difference at f800 (inside the 100-frame window)", {800}, False),
    ("difference after the dump frame", {899}, False),
    ("difference f0..f801 (reaches the window)", set(range(0, 802)), False),
]
ok = True
for name, d, want in cases:
    got, note = A.compare_arena(base, tip(d), DUMP)
    good = got == want
    ok &= good
    print("%-48s %s %s" % (name, "ok " if good else "WRONG", note))
got, note = A.compare_arena(base, tip(set())[:-1], DUMP)
ok &= not got
print("%-48s %s %s" % ("one frame fewer", "ok " if not got else "WRONG", note))
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
