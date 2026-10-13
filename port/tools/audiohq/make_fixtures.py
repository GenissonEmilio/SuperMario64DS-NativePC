#!/usr/bin/env python3
"""Regenerates fixtures/*.ogg (small, committed). Needs ffmpeg with libvorbis on
the PATH; nobody needs to run this to use the checks, the results are in the repo.

  tune_loop.ogg   44100 Hz mono, 2 s: 330 Hz for 1 s then 550 Hz for 1 s, with
                  LOOPSTART=44100 LOOPLENGTH=44100 (loops the 550 Hz half)
  chirp.ogg       22050 Hz mono, 12344 samples: a sweep rising from 600 Hz, no tags
  chirp_loop.ogg  22050 Hz mono, 12344 samples, same sweep with LOOPSTART=4000
                  LOOPLENGTH=8344 (a loop in the second part of the clip)
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import wavlib as W  # noqa: E402


def encode(wavpath, out, tags):
    cmd = ["ffmpeg", "-loglevel", "error", "-y", "-i", wavpath, "-c:a", "libvorbis", "-q:a", "3"]
    for k, v in tags.items():
        cmd += ["-metadata", "%s=%s" % (k, v)]
    subprocess.check_call(cmd + [out])


def main():
    out = os.path.join(HERE, "fixtures")
    os.makedirs(out, exist_ok=True)
    tmp = tempfile.mkdtemp()
    r = 44100
    W.write_wav(os.path.join(tmp, "tune.wav"), r, W.tone(330, r, 1.0) + W.tone(550, r, 1.0))
    encode(os.path.join(tmp, "tune.wav"), os.path.join(out, "tune_loop.ogg"),
           {"LOOPSTART": r, "LOOPLENGTH": r})
    chirp = W.tone(600, 22050, 0.6, chirp_to=1200)[:12344]
    W.write_wav(os.path.join(tmp, "chirp.wav"), 22050, chirp)
    encode(os.path.join(tmp, "chirp.wav"), os.path.join(out, "chirp.ogg"), {})
    encode(os.path.join(tmp, "chirp.wav"), os.path.join(out, "chirp_loop.ogg"),
           {"LOOPSTART": 4000, "LOOPLENGTH": 8344})


if __name__ == "__main__":
    main()
