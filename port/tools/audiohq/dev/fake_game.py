#!/usr/bin/env python3
"""A stand-in for walk_window.exe, for testing the audiohq scripts themselves off
Windows and without the ROM (selftest.py wires it up). It reads the same
environment variables and settings.json the real game does for the audio checks,
drives the host harness (port/tests/audiohq) over a synthetic archive, and writes
the logs, the WAV dump, the selftest picture and the state-hash lines in the
shapes the real game does. Not part of the game; not used on a real run.

Environment: FAKE_HOST_BIN (the harness binary), FAKE_SDAT (the synthetic archive).
FAKE_KIND=base makes it behave like the exe built before this work (ds only).
"""
import hashlib
import json
import os
import re
import subprocess
import sys

env = os.environ
frames = int(env.get("SM64DS_WINDOW_SELFTEST", "0") or 0)
settings = {}
if os.path.exists("settings.json"):
    settings = json.load(open("settings.json"))
quality = settings.get("AudioQuality", "hq")
if env.get("FAKE_KIND") == "base":
    quality = "ds"
hq = "1" if quality != "ds" else "0"
if env.get("SM64DS_AUDIO_HQ") in ("0", "1"):
    hq = env["SM64DS_AUDIO_HQ"]

cmd = [env["FAKE_HOST_BIN"], "--sdat", env["FAKE_SDAT"], "--hq", hq]
mods = settings.get("EnabledMods") or []
if mods and env.get("FAKE_KIND") != "base":
    cmd += ["--mods", os.path.join(os.getcwd(), "mods", mods[0])]
wav = env.get("SM64DS_WAV_DUMP", "mix.wav")
cmd += ["--out", wav, "--seconds", "%.4f" % (max(frames, 60) / 60.0)]
events = []
if env.get("SM64DS_LEVEL") or env.get("SM64DS_SND_MUSIC"):
    events.append((0.1, "start"))
for item in (env.get("SM64DS_SND_SYSPROBE") or "").split(","):
    if ":" in item:
        f, i = item.split(":")
        events.append((int(f) / 60.0, "arc=%d" % int(i, 0)))
if env.get("FAKE_FADE"):
    events.append((frames / 120.0, "db10=-240"))
    events.append((frames / 120.0 + 3.0, "db10=0"))
paused = False
for item in (env.get("SM64DS_PROBE_INPUT") or "").split(","):
    m = re.match(r"(\d+)(?:-\d+)?:START", item)
    if m:
        events.append((int(m.group(1)) / 60.0, "resume" if paused else "pause"))
        paused = not paused
for t, a in events:
    cmd += ["--at", "%.4f:%s" % (t, a)]
sys.stderr.write("[fake] game start: %s\n" % " ".join(os.path.basename(c) for c in cmd[:1]))
child_env = dict(os.environ)
r = subprocess.run(cmd, env=child_env, stdout=sys.stdout, stderr=sys.stderr)

brk = env.get("FAKE_BREAK", "") if env.get("FAKE_KIND") != "base" else ""
if "dswav" in brk and hq == "0" and os.path.exists(wav):
    b = bytearray(open(wav, "rb").read())
    b[len(b) // 2] ^= 1                      # one flipped bit in the middle of the ds dump
    open(wav, "wb").write(bytes(b))
key = "%s|%s|%s" % (env.get("SM64DS_LEVEL"), env.get("SM64DS_SCENE"), frames)
if env.get("SM64DS_WINDOW_SELFTEST"):
    h = hashlib.sha256((key + ("x" if "pic" in brk and hq == "1" else "")).encode()).digest()
    open("walk_window_selftest.bmp", "wb").write(b"BM" + h * 8)
if env.get("SM64DS_RASTER_AB"):
    sys.stderr.write("[raster-ab] summary over %d frame(s), mismatches colour 0 depth 0 coverage 0 stencil 0 attrid 0 tlattr 0, frames with any 0 VERDICT=PASS\n" % frames)
if env.get("SM64DS_TWOD_VERIFY"):
    sys.stderr.write("[twod] SUMMARY tiles verify: 40 tiles compared, 0 differ\n")
if env.get("SM64DS_SCENE_RAMHASH"):
    for f in range(0, max(frames, 1), max(1, frames // 8)):
        d = hashlib.md5(("%s|%d%s" % (key, f, "!" if "state" in brk and hq == "1" and f > 300 else "")).encode()).hexdigest()
        sys.stderr.write("[scene-ramhash] f%d %s %s\n" % (f, d[:8], d[8:16]))
dump = env.get("SM64DS_SCENE_RAMDUMP")
if dump and ":" in dump:
    open(dump.split(":", 1)[1] + ".arena", "wb").write(hashlib.sha256(key.encode()).digest() * 64)
sys.exit(r.returncode)
