#!/usr/bin/env python3
"""Throws damaged .ogg and .wav files (flipped bytes, cut-offs, zeroed runs) at the
real audio code, built with the address and undefined-behaviour sanitizers, as a
music replacement and as a wave replacement. Any crash or sanitizer report is a
failure. Needs g++ and Linux. usage: python dev/fuzz_files.py [count] [seed] [--fixcrc]
--fixcrc repairs the Ogg page checksums after each mutation, so the damage reaches
the decoder; that models a crafted file, which the game's checksum pre-check cannot
stop. Random damage (the normal run) is stopped by the checksum.

(stb_vorbis itself leaks a little when it rejects a corrupt file; that is not
counted, ASAN_OPTIONS=detect_leaks=0:allocator_may_return_null=1 is set, as a failing malloc just returns NULL in the game.)"""
import os
import random
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
REPO = os.path.abspath(os.path.join(TOOLS, "..", "..", ".."))
bt = os.path.join(REPO, "port", "tests", "audiohq")
sdat_dir = os.path.join(REPO, "port", "hal", "sdat")

def fix_crcs(b):
    """Recompute every Ogg page CRC so a mutation reaches the decoder (what a
    deliberately crafted file would look like; random damage fails the checksum)."""
    tab = []
    for i in range(256):
        r = i << 24
        for _ in range(8):
            r = ((r << 1) ^ 0x04c11db7) & 0xffffffff if r & 0x80000000 else (r << 1) & 0xffffffff
        tab.append(r)
    p = 0
    while p + 27 <= len(b) and b[p:p + 4] == b"OggS":
        segs = b[p + 26]
        if p + 27 + segs > len(b):
            break
        ln = 27 + segs + sum(b[p + 27:p + 27 + segs])
        if p + ln > len(b):
            break
        b[p + 22:p + 26] = b"\0\0\0\0"
        crc = 0
        for x in b[p:p + ln]:
            crc = ((crc << 8) ^ tab[((crc >> 24) ^ x) & 0xff]) & 0xffffffff
        b[p + 22:p + 26] = crc.to_bytes(4, "little")
        p += ln


FIXCRC = "--fixcrc" in sys.argv
sys.argv = [a for a in sys.argv if a != "--fixcrc"]
count = int(sys.argv[1]) if len(sys.argv) > 1 else 120
random.seed(int(sys.argv[2]) if len(sys.argv) > 2 else 7)
work = tempfile.mkdtemp(prefix="audiohq_fuzz_")
exe = os.path.join(work, "host_asan")
subprocess.check_call(["g++", "-std=c++14", "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize=alignment,shift",
                       "-fno-sanitize-recover=undefined", "-w", "-I", os.path.join(bt, "shim"), "-I", sdat_dir,
                       os.path.join(bt, "host_audio_test.cpp")] +
                      [os.path.join(sdat_dir, f) for f in ("sdat.cpp", "mixer.cpp", "sseq.cpp", "sd_audio.cpp", "vorbis_impl.cpp")] +
                      ["-o", exe, "-lm"])
sdat = os.path.join(work, "test.sdat")
subprocess.check_call([sys.executable, os.path.join(bt, "make_test_sdat.py"), sdat], stdout=subprocess.DEVNULL)
sys.path.insert(0, TOOLS)
import wavlib as W  # noqa: E402
W.write_wav(os.path.join(work, "t.wav"), 22050, W.tone(330, 22050, 1.0) + W.tone(550, 22050, 1.0))
seeds = [open(os.path.join(TOOLS, "fixtures", f), "rb").read() for f in ("tune_loop.ogg", "chirp.ogg", "chirp_loop.ogg")]
seeds.append(open(os.path.join(work, "t.wav"), "rb").read())
bad = 0
env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:allocator_may_return_null=1")
for it in range(count):
    d = os.path.join(work, "mod")
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(os.path.join(d, "sound", "NCS_WAVE_TEST"))
    src = random.choice(seeds)
    b = bytearray(src)
    mode = random.choice(["flip", "trunc", "zero"])
    if mode == "flip":
        for _ in range(random.randint(1, 12)):
            b[random.randrange(len(b))] = random.randrange(256)
    elif mode == "trunc":
        b = b[:random.randrange(1, len(b))]
    else:
        i = random.randrange(len(b))
        n = random.randint(1, 300)
        b[i:i + n] = b"\0" * n
    ext = "ogg" if src[:4] == b"OggS" else "wav"
    if FIXCRC and ext == "ogg":
        fix_crcs(b)
    for rel in ("NCS_BGM_TEST." + ext, "NCS_WAVE_TEST/0." + ext):
        open(os.path.join(d, "sound", rel), "wb").write(b)
    r = subprocess.run([exe, "--sdat", sdat, "--mods", d, "--out", os.path.join(work, "o.wav"), "--seconds", "3",
                        "--at", "0.1:start", "--hq", "1"], capture_output=True, text=True, timeout=120, env=env)
    if r.returncode != 0 or "ERROR" in r.stderr or "runtime error" in r.stderr:
        bad += 1
        print("FAIL iteration %d (%s, %s) rc=%d\n%s" % (it, mode, ext, r.returncode, r.stderr[-1500:]))
        open(os.path.join(work, "fail_%d.%s" % (it, ext)), "wb").write(b)
        if bad > 3:
            break
print("fuzz: %d files, %d failures (work folder %s)" % (it + 1, bad, work))
sys.exit(1 if bad else 0)
