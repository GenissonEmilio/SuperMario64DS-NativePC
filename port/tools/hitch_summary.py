#!/usr/bin/env python3
"""Summarise the "[hitch]" lines of a play log (tests/perf_trace.inc, mode 3, on by default).

A hitch line is written for a game frame whose work ran past SM64DS_HITCH_MS (default 28 ms; 0 turns the log off):

  [hitch] t=620.4s frame=18412 lvl=6 work=38.3 wall=38.3 | rom_frame=11.2 raster=22.0 | gpuwait=1x/19.1ms tex=3x/4.2ms

t is process time, work is every stage but the pacer's wait, the stages after the first bar are the ones over 1.5 ms,
the names after the second bar are the costs timed by name (hal/hitch_probe.h: tex, cardfile, tongue, chat, nethook, hubpump,
ghostfollow, gpubuild, gpudraw, gpuwait, gpucopy, flush, swap). A LOAD tag marks a level load / star select frame.

usage: hitch_summary.py play_<id>.log [perf-<id>.jsonl]
With the perf file it also lists the 5 s windows whose worst frame passed 28 ms, so the two can be read side by side.
"""
import collections
import json
import re
import sys


def main():
    log = sys.argv[1]
    lines = [l for l in open(log, encoding="utf-8", errors="replace") if l.startswith("[hitch] t=")]
    rows = []
    for l in lines:
        m = re.match(r"\[hitch\] t=([\d.]+)s frame=(\d+) lvl=(-?\d+) work=([\d.]+) wall=([\d.]+)( LOAD)? \|(.*?)\|(.*)", l)
        if not m:
            continue
        t, fr, lvl, work, wall, load, st, named = m.groups()
        stages = {a: float(b) for a, b in re.findall(r"(\w+)=([\d.]+)", st)}
        costs = {a: (int(n), float(ms)) for a, n, ms in re.findall(r"(\w+)=(\d+)x/([\d.]+)ms", named)}
        rows.append(dict(t=float(t), frame=int(fr), lvl=int(lvl), work=float(work), load=bool(load), stages=stages, costs=costs))
    play = [r for r in rows if not r["load"]]
    print("%d hitch lines, %d of them level loads" % (len(rows), len(rows) - len(play)))
    by_stage = collections.Counter(max(r["stages"], key=r["stages"].get) for r in play if r["stages"])
    print("dominant stage:", dict(by_stage))
    by_cost = collections.Counter(max(r["costs"], key=lambda k: r["costs"][k][1]) for r in play if r["costs"])
    print("dominant named cost:", dict(by_cost))
    print("worst 15:")
    for r in sorted(play, key=lambda r: -r["work"])[:15]:
        print("  t=%7.1fs lvl %2d work %5.1f  %s | %s" % (r["t"], r["lvl"], r["work"],
              " ".join("%s=%.1f" % kv for kv in sorted(r["stages"].items(), key=lambda kv: -kv[1])[:3]),
              " ".join("%s=%dx/%.1f" % (k, v[0], v[1]) for k, v in sorted(r["costs"].items(), key=lambda kv: -kv[1][1])[:4])))
    if len(sys.argv) > 2:
        print("perf windows with a frame over 28 ms (level != load):")
        for l in open(sys.argv[2]):
            j = json.loads(l)
            if j.get("type") == "sample" and j["frameMsMax"] > 28 and j["level"] >= 0:
                print("  t=%7.1f lvl %2d fps %5.1f max %5.1f p95 %5.1f gpuMs/tick %.1f gpuRead %.1f" %
                      (j["t"], j["level"], j["fps"], j["frameMsMax"], j["frameMsP95"], j["gpuMsPerTick"], j["gpuReadMs"]))


main()
