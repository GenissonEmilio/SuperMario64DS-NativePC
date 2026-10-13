#!/usr/bin/env python3
"""fade_check.py: a replaced music track (.ogg stream) follows the game's volume steps with the
volume law of a sounding note: 10^(db10/200), i.e. db10/10 dB (mixer.cpp db10_to_gain, used by
sd_mix_stream_set_db10 for a stream and by the channel mix for every sequence note).

Runs the Linux host harness (the real sdat / mixer / sseq code) with tune_loop.ogg in place of the
test sequence and the game's fade script (sd_seq_set_volume_db10 = -100, -300, -600 at 2.0, 3.5, 5.0 s).
Each stretch's level is read off the dump relative to the first stretch and must drop by
-10, -30, -60 dB within 0.7 dB. The dump holds nothing but the stream here; in the game's
dump (mods_check fade row) the rest of the game's sound is mixed in, which is what that row has to
get around.
usage: python dev/fade_check.py      (needs g++; builds into a temp folder)
"""
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import wavlib as W  # noqa: E402

T = os.path.abspath(os.path.join(HERE, "..", "..", "..", "tests", "audiohq"))
STEPS = [(2.0, -100), (3.5, -300), (5.0, -600)]
WINDOWS = [(1.2, 1.9), (2.6, 3.4), (4.1, 4.9), (5.6, 6.4)]      # level read here, after each step settles


def run(out, name, mods):
    wav = os.path.join(out, name + ".wav")
    cmd = [os.path.join(out, "host_audio_test"), "--sdat", os.path.join(out, "t.sdat"), "--mods", mods, "--out", wav,
           "--seconds", "7", "--hq", "1", "--at", "0:start"]
    for t, d in STEPS:
        cmd += ["--at", "%g:db10=%d" % (t, d)]
    subprocess.run(cmd, check=True, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL, env=dict(os.environ, SM64DS_VOLUME="100"))
    w = W.read_wav(wav)
    m = w.mono()
    lv = [W.rms_db(m[int(a * w.rate):int(b * w.rate)]) for a, b in WINDOWS]
    return [l - lv[0] for l in lv]


class _Res:
    def __init__(self):
        self.rows = []

    def add(self, name, status, detail):
        self.rows.append((name, status, detail))


class _Row:
    def __init__(self, path):
        self._p = path

    def path(self, n):
        return self._p


def synthetic(out, fade_gain_error_db=0.0):
    """mods_check.check_fade on a made-up dump: a 330/550 Hz stream through the game's volume steps,
    plus a constant 'other sound' (noise and a 437 Hz tone) that does not fade. Plain RMS reads this
    3 dB high on a deep step; the 550 Hz reading must pass, and must fail when the stream really
    is too loud (fade_gain_error_db)."""
    import math
    import random
    import mods_check as M
    rate, secs = 48000, 12
    steps = [(0, -24), (4, -24), (700, -104), (1300, -264), (1900, -104)]    # (tick, db10): -24 is the full level
    rng = random.Random(5)
    db = [(t, 0, v) for t, v in steps]
    x = []
    for i in range(rate * secs):
        tk = i * 192.0 / rate
        v = [vv for t, vv in steps if t <= tk][-1]
        g = 10 ** ((v + 24) / 200.0)
        if v != -24:
            g *= 10 ** (fade_gain_error_db / 20.0)
        pos = (tk - 4) / 192.0
        f = 330 if pos < 1.0 else 550
        x.append(3000 * g * math.sin(2 * math.pi * f * i / rate) + 1500 * math.sin(2 * math.pi * 437 * i / rate)
                 + 1500 * (rng.random() - 0.5))
    wav = os.path.join(out, "synth.wav")
    W.write_wav(wav, rate, x)
    res = _Res()
    M.check_fade(res, "synthetic", _Row(wav), db, 4 / 192.0)
    return res.rows[0]


def main():
    out = tempfile.mkdtemp(prefix="ahq_fade_")
    subprocess.run(["sh", os.path.join(T, "build_host_test.sh"), out], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, os.path.join(T, "make_test_sdat.py"), os.path.join(out, "t.sdat")], check=True, stdout=subprocess.DEVNULL)
    mod = os.path.join(out, "mod", "sound")
    os.makedirs(mod)
    shutil.copyfile(os.path.join(HERE, "..", "fixtures", "tune_loop.ogg"), os.path.join(mod, "NCS_BGM_TEST.ogg"))
    want = [0.0] + [d / 10.0 for _, d in STEPS]
    got = run(out, "stream", os.path.dirname(mod))
    bad = [i for i in range(len(want)) if abs(got[i] - want[i]) > 0.7]
    ok = not bad
    print("ogg stream relative level per stretch: %s dB (wanted %s)%s" % (
        ", ".join("%.2f" % g for g in got), ", ".join("%g" % w_ for w_ in want), "" if not bad else "  <-- off"))
    for err, want in ((0.0, "PASS"), (4.0, "FAIL")):
        row = synthetic(out, err)
        good = row[1] == want
        ok &= good
        print("synthetic dump, stream %+g dB off, other sound mixed in: %s %s%s" % (err, row[1], row[2], "" if good else "  <-- WRONG"))
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
