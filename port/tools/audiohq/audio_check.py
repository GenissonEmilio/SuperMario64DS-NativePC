#!/usr/bin/env python3
"""audio_check.py: does the 48 kHz "hq" mixer behave like the DS-rate "ds" one?

Runs short game routes muted, minimized and never focused (SM64DS_VOLUME=0,
SM64DS_NO_AUDIO=1 so nothing reaches a speaker, SM64DS_WAV_DUMP=<file> captures
the mix) and compares the dumps:

  (a) "ds" mode: the WAV dump of each route is byte-identical to the BASE exe's
      (the exe built before this work). Each route is first run twice on the
      base exe; a route that is not repeatable on its own cannot be held to this.
      The kill switch SM64DS_AUDIO_HQ=0 over an hq setting must give the same bytes.
  (b) "hq" mode: a 48000 Hz dump whose tempo and note onsets line up with the ds
      dump within one 192 Hz tick across the route (the envelopes are
      cross-correlated tick by tick), at a similar level (RMS within 1 dB), with
      no clicks (no sample-to-sample jump above what the ds dump has), and with
      PSG (pulse / noise) notes at the same pitch (FFT) as in the ds dump.
  (c) the game's state is identical between base, ds and hq over the route (the
      masked RAM hash the other lanes use: SM64DS_SCENE_RAMHASH with a mask of
      the words that differ between two identical runs).
  (d) cost: the mixer's own measured CPU time per second of audio, hq against
      ds (SM64DS_AUDIO_STATS=1), and the whole process's CPU seconds.

usage: python audio_check.py [--quick] [--routes l1_idle,l1_play,l6_idle,psg]
                             [--only abcd] [--offline DIR]
--offline DIR skips the game and analyses <DIR>/<route>_ds.wav / _hq.wav that
are already there (used by selftest.py).
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audiohq_common as C  # noqa: E402
import wavlib as W  # noqa: E402

TICK = 1.0 / W.TICK_HZ


# ---- routes ---------------------------------------------------------------------

def input_script(first=150, every=75, count=40):
    """A walk-and-jump pattern for SM64DS_PROBE_INPUT (64 presses at most)."""
    steps = ["%d-%d:UP" % (first - 20, first + every * count)]
    for i in range(count):
        steps.append("%d-%d:A" % (first + i * every, first + i * every + 4))
    return ",".join(steps[:64])


def routes(cfg, sd=None):
    frames = int(cfg["route_seconds"] * 60)
    base = {"SM64DS_NO_AUDIO": "1", "SM64DS_WINDOW_SELFTEST": str(frames), "SM64DS_SELFTEST_IDLE": "1",
            "SM64DS_STARS_SEED": "0:01"}
    r = {
        "l1_idle": dict(base, SM64DS_LEVEL="1"),
        "l1_play": dict(base, SM64DS_LEVEL="1", SM64DS_PROBE_INPUT=input_script()),
        "l6_idle": dict(base, SM64DS_LEVEL="6"),
    }
    # the PSG route: music silenced by a mod (see silent_music_mod), the red coin
    # jingles (SEQARC 2 entries 47..55 are pure pulse notes) played on demand
    psg_frames = min(frames, 900)
    found = []
    if sd is not None:
        import sdat_tools
        found = sdat_tools.psg_entries(sd, cfg.get("probe_seqarc", 2))[:9]
    found = found or list(range(47, 56))          # the red coin jingles
    ids = ",".join("%d:%d" % (60 + 90 * i, e) for i, e in enumerate(found))
    r["psg"] = dict(base, SM64DS_LEVEL="1", SM64DS_WINDOW_SELFTEST=str(psg_frames), SM64DS_SND_SYSPROBE=ids)
    return r


def silent_music_mod(names):
    """A mod folder filler that replaces every music track with silence (so only
    sound effects reach the dump)."""
    def fill(folder):
        os.makedirs(os.path.join(folder, "sound"))
        samples = [0] * 8000
        for n in names:
            W.write_wav(os.path.join(folder, "sound", n + ".wav"), 8000, samples)
    return fill


# ---- analysis (no game needed) ----------------------------------------------------

def dominant_in(x, rate, t0, dur=0.06, lo=80.0, hi=6000.0):
    n = int(rate * dur)
    a = int(t0 * rate)
    seg = x[a:a + n]
    if len(seg) < n:
        return None
    size = 16384
    win = [0.5 - 0.5 * __import__("math").cos(2 * __import__("math").pi * i / (n - 1)) for i in range(n)]
    buf = [complex(seg[i] * win[i], 0.0) for i in range(n)] + [0j] * (size - n)
    spec = W._fft(buf)
    k0, k1 = int(lo * size / rate), int(hi * size / rate)
    mag = [abs(spec[k]) for k in range(size // 2)]
    best = max(range(k0, k1), key=lambda k: mag[k])
    if mag[best] < 1e-6:
        return None
    a_, b_, c_ = mag[best - 1], mag[best], mag[best + 1]
    den = a_ - 2 * b_ + c_
    off = 0.5 * (a_ - c_) / den if den else 0.0
    return (best + off) * rate / size


def analyse_pair(ds_path, hq_path, psg=False):
    """Checks of an hq dump against the ds dump of the same route. Returns a list
    of (name, status, detail)."""
    out = []
    ds, hq = W.read_wav(ds_path), W.read_wav(hq_path)
    out.append(("rates", "PASS" if (ds.rate, hq.rate) == (32768, 48000) else "FAIL",
                "ds dump %d Hz, hq dump %d Hz" % (ds.rate, hq.rate)))
    dd, dh = ds.seconds(), hq.seconds()
    out.append(("length", "PASS" if abs(dd - dh) <= TICK + 1e-9 else "FAIL",
                "ds %.3f s, hq %.3f s (limit one tick = %.4f s)" % (dd, dh, TICK)))
    md, mh = ds.mono(), hq.mono()
    ed, eh = W.tick_envelope(md, ds.rate), W.tick_envelope(mh, hq.rate)
    if max(ed + [0.0]) < 1.0 and max(eh + [0.0]) < 1.0:
        out.append(("sound", "INCONCLUSIVE", "both dumps are silent: the route made no sound"))
        return out
    n = min(len(ed), len(eh))
    ed, eh = ed[:n], eh[:n]
    lag, corr = W.best_lag(ed, eh)
    ok = lag == 0 and corr >= 0.98
    out.append(("envelope", "PASS" if ok else "FAIL",
                "best tick lag %+d, correlation %.4f over %d ticks" % (lag, corr, n)))
    od, oh = W.env_onsets(ed), W.env_onsets(eh)
    a_in_b, b_in_a, na, nb = W.match_onsets(od, oh, tol=1)
    if na < 5 and nb < 5:
        out.append(("onsets", "INCONCLUSIVE", "only %d / %d note onsets found, route too quiet or too steady" % (na, nb)))
    else:
        out.append(("onsets", "PASS" if min(a_in_b, b_in_a) >= 0.97 else "FAIL",
                    "%d ds / %d hq onsets, %.1f%% / %.1f%% within one tick of the other" % (
                        na, nb, 100 * a_in_b, 100 * b_in_a)))
    rd, rh = W.rms_db(md), W.rms_db(mh)
    out.append(("level", "PASS" if abs(rd - rh) <= 1.0 else "FAIL",
                "RMS ds %.2f dB, hq %.2f dB (limit 1 dB)" % (rd, rh)))
    # per-block level, so a section that is too loud or too quiet shows even when the average is fine
    worst = 0.0
    wblock = 0
    for b in range(int(dd // 5)):
        bd = md[int(b * 5 * ds.rate):int((b + 1) * 5 * ds.rate)]
        bh = mh[int(b * 5 * hq.rate):int((b + 1) * 5 * hq.rate)]
        a_, c_ = W.rms_db(bd), W.rms_db(bh)
        if a_ > -60 and abs(a_ - c_) > worst:
            worst, wblock = abs(a_ - c_), b
    out.append(("level 5s blocks", "PASS" if worst <= 1.5 else "FAIL",
                "worst block %d s: %.2f dB apart (limit 1.5 dB)" % (wblock * 5, worst)))
    jd, jh = W.max_jump(md), W.max_jump(mh)
    limit = max(jd, 2000.0) * 1.10
    out.append(("clicks", "PASS" if jh <= limit else "FAIL",
                "largest sample-to-sample jump: ds %.0f, hq %.0f (limit %.0f)" % (jd, jh, limit)))
    if psg:
        # the dominant frequency every 80 ms where the ds dump is audible, in both dumps
        bad, tested = [], 0
        t = 0.05
        while t < min(dd, dh) - 0.1:
            if W.rms_db(md[int(t * ds.rate):int((t + 0.06) * ds.rate)]) > -50.0:
                fd, fh = dominant_in(md, ds.rate, t), dominant_in(mh, hq.rate, t)
                if fd is not None and fh is not None:
                    tested += 1
                    if abs(fh - fd) / fd > 0.01:
                        bad.append("t=%.2fs ds %.1f Hz hq %.1f Hz" % (t, fd, fh))
            t += 0.08
        if tested < 5:
            out.append(("psg pitch", "INCONCLUSIVE", "only %d audible windows to compare" % tested))
        else:
            out.append(("psg pitch", "PASS" if len(bad) <= 0.08 * tested else "FAIL",
                        "%d windows, %d where hq is more than 1%% from the ds pitch%s" % (
                            tested, len(bad), "" if not bad else " (" + "; ".join(bad[:3]) + ")")))
    return out


STATS_RE = re.compile(r"\[audio\] stats: mix ([0-9.]+) ms for ([0-9.]+) s of audio at (\d+) Hz = ([0-9.]+)% of real time")


def parse_stats(text):
    m = STATS_RE.findall(text)
    if not m:
        return None
    ms, secs, rate, pct = m[-1]
    return dict(ms=float(ms), seconds=float(secs), rate=int(rate), pct=float(pct))


# ---- state rows ---------------------------------------------------------------------

RAMHASH_RE = re.compile(r"\[scene-ramhash\] f(\d+) ([0-9a-f]{8}) ([0-9a-f]{8})")


def _dump_words(row):
    import struct
    p = row.path("ram.arena")
    if not os.path.exists(p):
        return None
    d = open(p, "rb").read()
    return struct.unpack("<%dI" % (len(d) // 4), d[:len(d) // 4 * 4])


def build_mask(cfg, env, tag):
    """Words of the arena that may legitimately differ, as a mask file of 32-bit word indices.
    Dump the arena at the last frame of four runs, two of the base exe and two of the tip exe
    (ds), and list every word where any of them disagrees:
      * base vs base and tip vs tip: run-to-run noise (timers, thread timing);
      * base vs tip: the two exes are different images, so every host pointer the game keeps in
        its arena (heap objects, vtables, "name pointer 117e4e38" vs "117f9e38" in the logs) moves
        with the image layout. That is not game state, and it is the reason the first version of
        this check failed at its first hashed frame on every route."""
    import struct
    frames = int(env.get("SM64DS_WINDOW_SELFTEST", "600"))
    e = dict(env, SM64DS_SCENE_RAMHASH="1", SM64DS_SCENE_RAMDUMP="%d:{wdir}/ram" % (frames - 2))
    plan = [("base1", cfg["exe_base"]), ("base2", cfg["exe_base"]), ("tip1", cfg["exe_tip"]), ("tip2", cfg["exe_tip"])]
    rows = C.run_parallel([(lambda n=n, x=x: C.launch(cfg, "%s_mask_%s" % (tag, n), x, e,
                                                      settings={"AudioQuality": "ds"}, subdir="state"))
                           for n, x in plan], cfg["workers"])
    ws = [_dump_words(r) for r in rows]
    if any(w is None for w in ws):
        return None, "no arena dump (is SM64DS_SCENE_RAMDUMP supported by this exe?)"
    n = min(len(w) for w in ws)
    idx = [i for i in range(n) if len({w[i] for w in ws}) > 1]
    noise = sum(1 for i in range(n) if ws[0][i] != ws[1][i] or ws[2][i] != ws[3][i])
    mp = os.path.join(cfg["out_dir"], "state", tag + "_mask.bin")
    with open(mp, "wb") as f:
        f.write(struct.pack("<%dI" % len(idx), *idx))
    return mp, "%d of %d arena words masked: %d run-to-run noise, %d more differ only between the two exe images" % (
        len(idx), n, noise, len(idx) - noise)


def state_trace(cfg, name, exe, env, settings, mask):
    e = dict(env, SM64DS_SCENE_RAMHASH="1", SM64DS_SCENE_RAMMASK=mask)
    row = C.launch(cfg, name, exe, e, settings=settings, subdir="state")
    return row, [(int(f), m, a) for f, m, a in RAMHASH_RE.findall(row.text)]


def _first_diff(x, y):
    for p, q in zip(x, y):
        if p != q:
            return "first difference frame %d" % p[0]
    if len(x) != len(y):
        return "%d frames against %d" % (len(y), len(x))
    return ""


def check_state(cfg, res, name, env):
    """(c) for one route. Same exe (ds vs hq): the main-RAM hash and the masked arena hash must be
    equal on every frame. Base exe vs tip exe: the masked arena hash only -- the main-RAM hash
    (0x02000000, 4 MB) also holds host pointers and cannot be masked, so across two different
    images it is not comparable and is not used."""
    mask, note = build_mask(cfg, env, name)
    if not mask:
        res.add("state " + name, "INCONCLUSIVE", note)
        return
    jobs = [lambda: state_trace(cfg, name + "_base", cfg["exe_base"], env, {}, mask),
            lambda: state_trace(cfg, name + "_ds", cfg["exe_tip"], env, {"AudioQuality": "ds"}, mask),
            lambda: state_trace(cfg, name + "_hq", cfg["exe_tip"], env, {"AudioQuality": "hq"}, mask)]
    (rb, tb), (rd, td), (rh, th) = C.run_parallel(jobs, cfg["workers"])
    if not tb or not td or not th:
        res.add("state " + name, "INCONCLUSIVE", "an exe printed no [scene-ramhash] lines")
        return
    ok = td == th
    res.add("state %s ds == hq" % name, "PASS" if ok else "FAIL",
            "%d frames, main RAM + arena hash; %s%s" % (len(td), note, "" if ok else "; " + _first_diff(td, th)))
    arena = lambda t: [(f, a) for f, m, a in t]
    for lab, t in (("ds", td), ("hq", th)):
        same = arena(tb) == arena(t)
        res.add("state %s base == %s" % (name, lab), "PASS" if same else "FAIL",
                "%d frames, masked arena hash%s" % (len(tb), "" if same else "; " + _first_diff(arena(tb), arena(t))))


# ---- the live checks -----------------------------------------------------------------

def live(cfg, which, only):
    C.need(cfg, "exe_tip", "exe_base", "asset_root")
    res = C.Results("audio_check")
    sd = None
    if os.path.exists(cfg.get("sdat", "")):
        import sdat_tools
        sd = sdat_tools.Sdat(cfg["sdat"])
    rts = routes(cfg, sd)
    names = [n for n in (which or list(rts)) if n in rts]
    if "psg" in names and sd is None:
        names.remove("psg")
        res.add("psg route", "INCONCLUSIVE", "cannot read the sound archive (%s), so no silencing mod can be made" % cfg.get("sdat"))

    def run(name, route, exe, settings, extra_env=None, mods=None, tag=""):
        env = dict(route, SM64DS_WAV_DUMP="{wdir}/mix.wav", SM64DS_AUDIO_STATS="1")
        env.update(extra_env or {})
        return C.launch(cfg, "%s_%s" % (name, tag), exe, env, settings=settings, mods=mods, subdir="audio")

    for name in names:
        route = rts[name]
        mods = {"audiohq-silent": silent_music_mod(sd.bgm_names())} if name == "psg" else None
        # ---- runs
        jobs = []
        if not mods:   # the base exe has no recordings feature, so it cannot take the silencing mod
            jobs += [lambda: run(name, route, cfg["exe_base"], {}, tag="base_a"),
                     lambda: run(name, route, cfg["exe_base"], {}, tag="base_b")]
        jobs += [lambda: run(name, route, cfg["exe_tip"], {"AudioQuality": "ds"}, mods=mods, tag="ds"),
                 lambda: run(name, route, cfg["exe_tip"], {"AudioQuality": "hq"}, mods=mods, tag="hq"),
                 lambda: run(name, route, cfg["exe_tip"], {"AudioQuality": "hq"}, {"SM64DS_AUDIO_HQ": "0"}, mods=mods, tag="kill"),
                 lambda: run(name, route, cfg["exe_tip"], {}, mods=mods, tag="default")]
        rows = C.run_parallel(jobs, cfg["workers"])
        got = {r.name[len(name) + 1:]: r for r in rows}
        dump = lambda r: r.path("mix.wav")
        for r in rows:
            if r.rc != 0 or r.fatal():
                res.add("%s run %s" % (name, r.name[len(name) + 1:]), "FAIL",
                        "rc=%s fatal lines=%d: %s" % (r.rc, r.fatal(), C.tail(r.text, 4).replace("\n", " | ")))
        if "a" in only:
            if "base_a" in got:
                det = C.sha256(dump(got["base_a"])) == C.sha256(dump(got["base_b"]))
                if not det:
                    res.add("%s (a) base repeatable" % name, "INCONCLUSIVE",
                            "two runs of the base exe differ, so byte identity is not checked for this route")
                else:
                    same = C.sha256(dump(got["base_a"])) == C.sha256(dump(got["ds"]))
                    res.add("%s (a) ds == base" % name, "PASS" if same else "FAIL",
                            "sha256 %s vs %s" % (C.sha256(dump(got["base_a"]))[:12], C.sha256(dump(got["ds"]))[:12]))
            same = C.sha256(dump(got["kill"])) == C.sha256(dump(got["ds"]))
            res.add("%s (a) SM64DS_AUDIO_HQ=0 == ds" % name, "PASS" if same else "FAIL",
                    "kill switch over an hq setting gives the ds bytes")
            w = W.read_wav(dump(got["default"])).rate if os.path.exists(dump(got["default"])) else 0
            res.add("%s (a) default is hq" % name, "PASS" if w == 48000 else "FAIL",
                    "no AudioQuality key: dump is %d Hz" % w)
        if "b" in only and os.path.exists(dump(got["ds"])) and os.path.exists(dump(got["hq"])):
            for n, s, d in analyse_pair(dump(got["ds"]), dump(got["hq"]), psg=(name == "psg")):
                res.add("%s (b) %s" % (name, n), s, d)
        if "d" in only:
            sd_, sh = parse_stats(got["ds"].text), parse_stats(got["hq"].text)
            busy = C.busy_processes(cfg)
            if busy:
                res.add("%s (d) cost" % name, "SKIP", "machine busy (%s); timing not taken" % ", ".join(busy))
            elif not sd_ or not sh:
                res.add("%s (d) cost" % name, "INCONCLUSIVE", "no [audio] stats line (is SM64DS_AUDIO_STATS in this exe?)")
            else:
                ok = sh["pct"] <= 5.0 and sh["pct"] <= max(3.0 * sd_["pct"], sd_["pct"] + 0.5)
                cpu = ""
                if got["ds"].cpu_seconds is not None and got["hq"].cpu_seconds is not None:
                    cpu = "; process CPU ds %.1f s, hq %.1f s" % (got["ds"].cpu_seconds, got["hq"].cpu_seconds)
                res.add("%s (d) cost" % name, "PASS" if ok else "FAIL",
                        "mixer %.3f%% of real time in ds, %.3f%% in hq (limit: 5%% and 3x ds)%s" % (sd_["pct"], sh["pct"], cpu))
    if "c" in only:
        for name in ("l1_idle", "l6_idle"):
            if name in rts:
                env = dict(rts[name])
                env["SM64DS_WINDOW_SELFTEST"] = str(min(int(env["SM64DS_WINDOW_SELFTEST"]), 900))
                check_state(cfg, res, name, env)
    return res


def offline(dirpath):
    res = C.Results("audio_check (offline)")
    for f in sorted(os.listdir(dirpath)):
        if f.endswith("_ds.wav"):
            n = f[:-7]
            hq = os.path.join(dirpath, n + "_hq.wav")
            if os.path.exists(hq):
                for a, s, d in analyse_pair(os.path.join(dirpath, f), hq, psg=n.startswith("psg")):
                    res.add("%s (b) %s" % (n, a), s, d)
    return res


def main():
    ap = C.common_args(__doc__)
    ap.add_argument("--routes", default="", help="comma list: l1_idle,l1_play,l6_idle,psg (default all)")
    ap.add_argument("--only", default="abcd", help="which checks to run, any of a b c d")
    ap.add_argument("--offline", metavar="DIR", help="analyse existing <route>_ds.wav / _hq.wav in DIR")
    args = ap.parse_args()
    if args.offline:
        res = offline(args.offline)
    else:
        cfg = C.config_from(args)
        res = live(cfg, [r for r in args.routes.split(",") if r], args.only)
        res.save(cfg, "audio_check.txt")
    print()
    print(res.table())
    return 1 if res.failed() else 0


if __name__ == "__main__":
    sys.exit(main())
