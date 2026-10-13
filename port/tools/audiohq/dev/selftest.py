#!/usr/bin/env python3
"""selftest.py: tests the audiohq scripts themselves, on Linux, with no game and no ROM.

It builds the host harness (port/tests/audiohq) for this tree and for a baseline
git ref, wraps each in a stand-in "walk_window" (dev/fake_game.py) that speaks the
same environment variables, settings.json and log lines, and runs audio_check,
mods_check and gate against them using a synthetic sound archive. A PASS here
means the scripts' logic (launching, parsing, comparing, the table) works and the
checks catch what they should; it says nothing about the real game, which only
run_all.py on the real exes can say.

usage: python dev/selftest.py [--base-ref origin/lead/audio-snapshot] [--keep]
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
REPO = os.path.abspath(os.path.join(TOOLS, "..", "..", ".."))
sys.path.insert(0, TOOLS)
import wavlib as W  # noqa: E402


def sh(cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base-ref", default="origin/lead/audio-snapshot")
    ap.add_argument("--work", default=os.path.join(tempfile.gettempdir(), "audiohq_selftest"))
    ap.add_argument("--only", default="unit,audio,mods,gate")
    ap.add_argument("--negative", action="store_true",
                    help="also break the stand-in game on purpose and check the scripts notice")
    args = ap.parse_args()
    work = os.path.abspath(args.work)
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    only = set(args.only.split(","))
    fails = []

    # ---- 1. unit checks of the analysis code ------------------------------------
    if "unit" in only:
        sys.path.insert(0, TOOLS)
        import audio_check as A
        print("== unit ==")
        # a click-free, level-matched pair passes; a pitch-shifted PSG and a lagging hq fail
        def burst_wav(path, rate, f_hz, lag_ticks=0, gain=1.0, click=False):
            import math
            n = int(rate * 6.0)
            x = []
            for i in range(n):
                tick = int(i * 192.0 / rate)
                x.append(9000 * gain * math.sin(2 * math.pi * f_hz * i / rate) if (tick % 48) < 24 else 0.0)
            x = [0.0] * int(lag_ticks * rate / 192.0) + x
            if click:
                x[len(x) // 2] += 16000
            W.write_wav(path, rate, x)
        d = os.path.join(work, "unit")
        os.makedirs(d)
        burst_wav(os.path.join(d, "ok_ds.wav"), 32768, 440)
        burst_wav(os.path.join(d, "ok_hq.wav"), 48000, 440)
        burst_wav(os.path.join(d, "lag_ds.wav"), 32768, 440)
        burst_wav(os.path.join(d, "lag_hq.wav"), 48000, 440, lag_ticks=3)
        burst_wav(os.path.join(d, "loud_ds.wav"), 32768, 440)
        burst_wav(os.path.join(d, "loud_hq.wav"), 48000, 440, gain=1.5)
        burst_wav(os.path.join(d, "click_ds.wav"), 32768, 440)
        burst_wav(os.path.join(d, "click_hq.wav"), 48000, 440, click=True)
        burst_wav(os.path.join(d, "psgok_ds.wav"), 32768, 523)
        burst_wav(os.path.join(d, "psgok_hq.wav"), 48000, 523)
        burst_wav(os.path.join(d, "psgbad_ds.wav"), 32768, 523)
        burst_wav(os.path.join(d, "psgbad_hq.wav"), 48000, 523 * 1.04)

        def status(name, psg=False):
            return {n: s for n, s, _ in A.analyse_pair(os.path.join(d, name + "_ds.wav"), os.path.join(d, name + "_hq.wav"), psg)}
        expect = [("ok", {}, {"rates", "length", "envelope", "onsets", "level", "level 5s blocks", "clicks"}, set()),
                  ("lag", {}, set(), {"envelope"}), ("loud", {}, set(), {"level"}),
                  ("click", {}, set(), {"clicks"}),
                  ("psgok", {"psg": True}, {"psg pitch"}, set()), ("psgbad", {"psg": True}, set(), {"psg pitch"})]
        for name, kw, must_pass, must_fail in expect:
            st = status(name, **kw)
            for k in must_pass:
                if st.get(k) != "PASS":
                    fails.append("unit %s: %s is %s, expected PASS" % (name, k, st.get(k)))
            for k in must_fail:
                if st.get(k) != "FAIL":
                    fails.append("unit %s: %s is %s, expected FAIL" % (name, k, st.get(k)))
            print("  %-7s %s" % (name, ", ".join("%s=%s" % kv for kv in sorted(st.items()))))

    if not (only & {"audio", "mods", "gate"}):
        print("\n".join(fails) or "selftest: unit checks pass")
        return 1 if fails else 0

    # ---- 2. the harness binaries and the stand-in games ----------------------------
    print("== building the host harness ==")
    bt = os.path.join(REPO, "port", "tests", "audiohq")
    sh(["sh", os.path.join(bt, "build_host_test.sh"), os.path.join(work, "bin")], stdout=subprocess.DEVNULL)
    old = os.path.join(work, "old")
    os.makedirs(old)
    for f in ("sdat.cpp", "mixer.cpp", "sseq.cpp", "sdat.h"):
        src = subprocess.check_output(["git", "show", "%s:port/hal/sdat/%s" % (args.base_ref, f)], cwd=REPO)
        open(os.path.join(old, f), "wb").write(src)
    sh(["g++", "-std=c++14", "-O2", "-DOLD_BUILD", "-w", "-I", os.path.join(bt, "shim"), "-I", old,
        os.path.join(bt, "host_audio_test.cpp")] + [os.path.join(old, f) for f in ("sdat.cpp", "mixer.cpp", "sseq.cpp")]
       + ["-o", os.path.join(work, "bin", "host_base"), "-lm"])
    sdat = os.path.join(work, "aroot", "extracted", "dsd", "files", "data", "sound_data.sdat")
    os.makedirs(os.path.dirname(sdat))
    sh([sys.executable, os.path.join(bt, "make_test_sdat.py"), sdat], stdout=subprocess.DEVNULL)
    exes = {}
    for kind, host in (("tip", "host_audio_test"), ("base", "host_base")):
        p = os.path.join(work, "exe_" + kind, "walk_window")
        os.makedirs(os.path.dirname(p))
        with open(p, "w") as f:
            f.write("#!/bin/sh\nFAKE_FADE=1 FAKE_KIND=%s FAKE_HOST_BIN=%s FAKE_SDAT=%s exec %s %s\n" % (
                kind, os.path.join(work, "bin", host), sdat, sys.executable, os.path.join(HERE, "fake_game.py")))
        os.chmod(p, 0o755)
        exes[kind] = p
    cfgp = os.path.join(work, "config.json")
    json.dump({"exe_tip": exes["tip"], "exe_base": exes["base"], "asset_root": os.path.join(work, "aroot"),
               "out_dir": os.path.join(work, "out"), "route_seconds": 12, "workers": 2, "probe_seqarc": 0, "music_force_id": 0,
               "timeout_s": 300}, open(cfgp, "w"))
    env = dict(os.environ, AUDIOHQ_CONFIG=cfgp)
    for part, script in (("audio", "audio_check.py"), ("mods", "mods_check.py"), ("gate", "gate.py")):
        if part in only:
            print("\n== %s ==" % script)
            r = subprocess.run([sys.executable, os.path.join(TOOLS, script)], env=env, cwd=work)
            outf = os.path.join(work, "out", script.replace(".py", ".txt"))
            if not os.path.exists(outf):
                fails.append("%s wrote no result file" % script)
            elif r.returncode != 0:
                fails.append("%s reported FAIL (see above)" % script)
    if args.negative:
        print("\n== negative: break the stand-in on purpose, the scripts must notice ==")
        for brk, script, extra, needle in (
                ("dswav", "audio_check.py", ["--only", "a", "--routes", "l1_idle"], "(a) ds == base"),
                ("pic", "gate.py", ["--parts", "ident"], "variants differ"),
                ("state", "gate.py", ["--parts", "state"], "state l1_idle")):
            e2 = dict(env)
            wrap = os.path.join(work, "exe_tip", "walk_window")
            txt = open(wrap).read()
            open(wrap, "w").write(txt.replace("FAKE_FADE=1", "FAKE_FADE=1 FAKE_BREAK=%s" % brk))
            r = subprocess.run([sys.executable, os.path.join(TOOLS, script)] + extra, env=e2, cwd=work,
                               capture_output=True, text=True)
            open(wrap, "w").write(txt)
            hit = [l for l in r.stdout.splitlines() if needle in l and " FAIL " in l]
            print("  %-6s %s -> %s" % (brk, script, "noticed: " + hit[0].strip()[:110] if hit else "NOT NOTICED"))
            if not hit:
                fails.append("negative %s: %s did not report a FAIL on %r" % (brk, script, needle))
    print("\n" + ("SELFTEST FAILED:\n  " + "\n  ".join(fails) if fails else "SELFTEST PASSED"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
