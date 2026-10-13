#!/usr/bin/env python3
"""volume_check.py: the WAV dump is taken AFTER the host master volume.

Why the audiohq rows launch NO_AUDIO games at SM64DS_VOLUME=100 (audiohq_common.launch): the
mixer applies SM64DS_VOLUME inside sd_mix_render, before the dump taps the buffer, so volume 0
records digital silence (-120 dB). Runs the Linux host harness at 0 and at 100 and checks both.
usage: python dev/volume_check.py      (needs g++; builds into a temp folder)
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import wavlib as W  # noqa: E402

T = os.path.abspath(os.path.join(HERE, "..", "..", "..", "tests", "audiohq"))


def main():
    out = tempfile.mkdtemp(prefix="ahq_vol_")
    subprocess.run(["sh", os.path.join(T, "build_host_test.sh"), out], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    sdat = os.path.join(out, "t.sdat")
    subprocess.run([sys.executable, os.path.join(T, "make_test_sdat.py"), sdat], check=True, stdout=subprocess.DEVNULL)
    os.makedirs(os.path.join(out, "m"), exist_ok=True)
    lv = {}
    for v in ("0", "100"):
        wav = os.path.join(out, "v%s.wav" % v)
        env = dict(os.environ, SM64DS_VOLUME=v)
        subprocess.run([os.path.join(out, "host_audio_test"), "--sdat", sdat, "--mods", os.path.join(out, "m"),
                        "--out", wav, "--seconds", "2", "--at", "0:start"], check=True, env=env, stderr=subprocess.DEVNULL)
        lv[v] = W.rms_db(W.read_wav(wav).mono())
    print("level at SM64DS_VOLUME=0: %.1f dB, at 100: %.1f dB" % (lv["0"], lv["100"]))
    ok = lv["0"] <= -119.0 and lv["100"] > -60.0
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
