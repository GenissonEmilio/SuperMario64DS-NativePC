#!/usr/bin/env python3
"""Host test of the onset comparison (audio_check 'onsets' row): notes whose attack sits on the
6 dB detector threshold must still match; missing, moved or added notes must not."""
import os, random, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import wavlib as W


def build(onsets, seed, drop=(), shift=0, extra=(), n=11520):
    rng = random.Random(seed)
    env = [1.0] * n
    for i, t in enumerate(onsets):
        if i in drop:
            continue
        t += shift
        amp = 10 ** (rng.uniform(5.6, 6.4) / 20.0)   # attack of 6 dB +-0.4: sits on the detector threshold
        for k in range(20):
            env[t + k] = max(env[t + k], amp * 0.8 ** k)
    for t in extra:
        env[t] = 10 ** (12 / 20.0)
    return [v * 800.0 for v in env]


def run(name, hq, ds, want):
    od, oh = W.env_onsets(ds), W.env_onsets(hq)
    s1, s2 = W.soft_match_onsets(od, hq)[0], W.soft_match_onsets(oh, ds)[0]
    st = W.match_onsets(od, oh, 1)
    ok = (min(s1, s2) >= 0.97) == want
    print("%-32s ds %d hq %d  strict %.1f/%.1f%%  soft %.1f/%.1f%%  %s" % (
        name, len(od), len(oh), 100 * st[0], 100 * st[1], 100 * s1, 100 * s2, "ok" if ok else "WRONG"))
    return ok


ons = sorted(random.Random(7).sample(range(10, 11400, 25), 430))
ds = build(ons, 1)
ok = run("threshold flips (test noise)", build(ons, 2), ds, True)
ok &= run("10% of the notes missing", build(ons, 2, drop=set(range(0, 430, 10))), ds, False)
ok &= run("notes 3 ticks late", build(ons, 2, shift=3), ds, False)
ok &= run("40 notes added", build(ons, 2, extra=[t + 12 for t in ons[::10]]), ds, False)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
