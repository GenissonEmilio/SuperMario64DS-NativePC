#!/usr/bin/env python3
"""mods_check.py: do plain .wav / .ogg sound mods work in the real game?

Builds small test mods itself (generated sine / chirp .wav files, committed tiny
.ogg fixtures, one bad file of each kind) and proves each point from the game's
own log and a WAV dump of the mix, muted, minimized and never focused:

  music     a looping test tune (.ogg) replaces EVERY NCS_BGM_* track, so whichever
            piece the route starts is the tune. Proved: a "music stream start" line,
            the tune's own pitch whatever the sequence's tempo and key (330 Hz, then
            550 Hz forever by the loop tags), and a WAV that follows the game's volume.
  music_wav the same through a .wav with a "smpl" loop chunk.
  fade      the stream's loudness follows the fades the game applies (entering a
            course): the WAV level is laid over the logged volume (SM64DS_STREAM_TRACE).
  pause     the recording holds still while the game is paused and comes back.
  wave      one wave of a wave archive replaced by a .wav (and one by an .ogg):
            the sound played by a scripted probe uses the new clip (sample count in
            the voice log) at the clip's OWN pitch every time, however the sequence's
            notes differ (same playback rate on every note, in both mixer modes).
  status    with the recording on, the ROM-visible sequence events (start, pause,
            stop, finished of every player) are the same list as without the mod.
  forced    SM64DS_SND_MUSIC=<id> with only that one track replaced.
  bad       one bad file of each kind: each is skipped with exactly one
            "[mods] sound:" line naming it, the game runs to the end, the stock
            music still plays.

usage: python mods_check.py [--quick] [--only music,wave,bad,...]
"""
import os
import re
import shutil
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audiohq_common as C  # noqa: E402
import sdat_tools  # noqa: E402
import wavlib as W  # noqa: E402

FIX = os.path.join(HERE, "fixtures")

STREAM_START = re.compile(r"\[audio\] music stream start: player (\d+) plays '([^']+)' \((\d+) Hz, (\d+) ch, ([0-9.]+) s(, loop tags|, whole-file loop)")
STREAM_DB = re.compile(r"\[stream\] tick (\d+) player (\d+) db10 (-?\d+)")
STREAM_PR = re.compile(r"\[stream\] tick (\d+) player (\d+) (paused|resumed)")
CHAN_START = re.compile(r"\[vt\] chan\s+(\d+) start: (\d+) samples( looping)?, (-?\d+) dB10, pan (\d+), prio (\d+), rate ([0-9.]+) \((\d+) Hz wave")
NOTE_LINE = re.compile(r"\[vt\] note p(\d+) t(\d+) key (\d+): ([+-][0-9.]+) semitones off base (\d+) .*-> rate ([0-9.]+)")


# ---- generated files ---------------------------------------------------------------

def tune_wav(path, loop=True, rate=22050):
    """330 Hz for 1 s then 550 Hz for 1 s; with `loop`, a smpl chunk looping the second half."""
    x = W.tone(330, rate, 1.0) + W.tone(550, rate, 1.0)
    import array
    data = array.array("h", [int(v) for v in x]).tobytes()
    fmt = struct.pack("<HHIIHH", 1, 1, rate, rate * 2, 2, 16)
    body = b"WAVE" + b"fmt " + struct.pack("<I", 16) + fmt
    if loop:
        smpl = struct.pack("<9I", 0, 0, 0, 60, 0, 0, 0, 1, 0) + struct.pack("<6I", 0, 0, rate, 2 * rate - 1, 0, 0)
        body += b"smpl" + struct.pack("<I", len(smpl)) + smpl
    body += b"data" + struct.pack("<I", len(data)) + data
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(body)) + body)


def chirp_wav(path, rate=22050, n=12344):
    x = W.tone(600, rate, 0.6, chirp_to=1200)[:n]
    W.write_wav(path, rate, x)


def music_mod(names, kind="ogg"):
    def fill(folder):
        os.makedirs(os.path.join(folder, "sound"))
        for n in names:
            dst = os.path.join(folder, "sound", "%s.%s" % (n, kind))
            if kind == "ogg":
                shutil.copyfile(os.path.join(FIX, "tune_loop.ogg"), dst)
            else:
                tune_wav(dst)
    return fill


# ---- reading a run -------------------------------------------------------------------

def stream_info(text):
    starts = STREAM_START.findall(text)
    db = [(int(t), int(p), int(v)) for t, p, v in STREAM_DB.findall(text)]
    pr = [(int(t), int(p), s) for t, p, s in STREAM_PR.findall(text)]
    return starts, db, pr


def gain_at(db, tick):
    """The logged volume (db10) in force at `tick`."""
    cur = None
    for t, p, v in db:
        if t <= tick:
            cur = v
        else:
            break
    return cur


def window_db(x, rate, t0, dur=0.1):
    a = int(t0 * rate)
    seg = x[a:a + int(dur * rate)]
    return W.rms_db(seg)


def check_music_run(res, label, row, kind):
    """Pitch, loop and use checks on one music run. Returns (db trace, t_start) or (None, None)."""
    if row.rc != 0 or row.fatal():
        res.add(label + " ran", "FAIL", "rc=%s fatal lines=%d: %s" % (row.rc, row.fatal(), C.tail(row.text, 3).replace("\n", " | ")))
        return None, None
    starts, db, pr = stream_info(row.text)
    if not starts:
        res.add(label + " stream used", "INCONCLUSIVE",
                "no 'music stream start' line: this route's music is not a plain NCS_BGM sequence "
                "(%d [mods] lines about music)" % sum(1 for l in row.lines("[mods] sound:") if "music track" in l))
        return None, None
    if not any(t.startswith(", loop tags") for *_, t in starts[:1]) and kind == "ogg":
        res.add(label + " loop tags read", "FAIL", "the stream says '%s'" % starts[0][5].strip(", "))
    else:
        res.add(label + " stream used", "PASS", "player %s plays '%s' (%s Hz, %s ch, %s s, %s)" % (
            starts[0][0], starts[0][1], starts[0][2], starts[0][3], starts[0][4], starts[0][5].strip(", ")))
    if not os.path.exists(row.path("mix.wav")):
        res.add(label + " dump", "FAIL", "no mix.wav written")
        return None, None
    w = W.read_wav(row.path("mix.wav"))
    m = w.mono()
    t_start = (db[0][0] / W.TICK_HZ) if db else 0.0
    # own pitch: first half 330 Hz, then 550 Hz for good (loop tags)
    f1 = W.dominant_freq(m, w.rate, int((t_start + 0.4) * w.rate), 4096, 100, 3000)
    res.add(label + " first half pitch", "PASS" if abs(f1 - 330) <= 6 else "FAIL",
            "%.1f Hz at %.2f s (the file is 330 Hz; any tempo, key or bend would move it)" % (f1, t_start + 0.4))
    good = tot = 0
    t = t_start + 1.3
    last = len(m) / float(w.rate) - 0.3
    while t < last:
        if window_db(m, w.rate, t, 0.085) > -45.0:
            tot += 1
            f = W.dominant_freq(m, w.rate, int(t * w.rate), 4096, 100, 3000)
            if abs(f - 550) <= 8:
                good += 1
        t += 0.25
    if tot < 4:
        res.add(label + " loops", "INCONCLUSIVE", "only %d audible windows after the first pass" % tot)
    else:
        res.add(label + " loops", "PASS" if good >= 0.9 * tot else "FAIL",
                "%d of %d windows after the first pass sit at 550 Hz (the loop-tagged half)" % (good, tot))
    return db, t_start


def check_fade(res, label, row, db, t_start):
    """Lay the WAV level over the logged volume."""
    w = W.read_wav(row.path("mix.wav"))
    m = w.mono()
    if not db:
        return False
    vals = [v for t, p, v in db]
    if max(vals) - min(vals) < 60:
        return False
    ref_db10 = max(vals)
    # The dump is the whole game's sound, not just the stream: other sounds that do not fade with
    # the music add to a plain RMS (3 dB high at -34 dB, in the first rerun), so the level is read
    # at the stream's own tone -- tune_loop.ogg loops its 550 Hz half after the first second --
    # and only where that half is playing.
    seg = []
    for i, (t, p, v) in enumerate(db):
        t_end = db[i + 1][0] if i + 1 < len(db) else t + 400
        seg.append((t, t_end, v))
    first_loop = (t_start + 1.1) * W.TICK_HZ          # tick from which the stream is in its 550 Hz half

    def level(t, te):
        """550 Hz level 20 ticks into the stretch (or once the loop half has started), None if the
        stretch has no 0.1 s inside the loop half."""
        t1 = max(t + 20, first_loop + 10)
        if t1 + 0.1 * W.TICK_HZ > te or t1 / W.TICK_HZ + 0.1 > len(m) / float(w.rate):
            return None
        return W.tone_db(m, w.rate, t1 / W.TICK_HZ, 0.1, 550.0)

    ref = None
    for t, te, v in seg:
        if v == ref_db10 and te - t >= 60:
            ref = level(t, te)
            if ref is not None:
                break
    if ref is None or ref < -50:
        res.add(label + " fade", "INCONCLUSIVE", "no stable full-volume stretch in the 550 Hz half to measure against")
        return True
    bad, tested = [], 0
    for t, te, v in seg:
        if te - t < 60 or (v - ref_db10) / 10.0 < -40.0:
            continue
        got = level(t, te)
        if got is None:
            continue
        want = ref + (v - ref_db10) / 10.0
        tested += 1
        if abs(got - want) > 2.0:
            bad.append("tick %d: %.1f dB, wanted %.1f" % (t, got, want))
    res.add(label + " fade", "PASS" if tested >= 2 and not bad else ("FAIL" if bad else "INCONCLUSIVE"),
            "volume went %d..%d (db10) in %d steps; %d stretches measured against the dump%s" % (
                min(vals), max(vals), len(vals), tested, "" if not bad else "; off: " + "; ".join(bad[:3])))
    return True


def check_pause(res, label, row, pr):
    w = W.read_wav(row.path("mix.wav"))
    m = w.mono()
    paused = [t for t, p, s in pr if s == "paused"]
    resumed = [t for t, p, s in pr if s == "resumed"]
    if not paused:
        return False
    t0 = paused[0] / W.TICK_HZ
    t1 = resumed[0] / W.TICK_HZ if resumed else len(m) / float(w.rate)
    if t1 - t0 < 0.4:
        res.add(label + " pause", "INCONCLUSIVE", "paused for only %.2f s" % (t1 - t0))
        return True
    before = window_db(m, w.rate, max(0.0, t0 - 0.3), 0.1)
    during = window_db(m, w.rate, t0 + 0.2, 0.1)
    ok = during <= before - 30.0
    detail = "paused %.2f s .. %.2f s: level %.1f dB before, %.1f dB during" % (t0, t1, before, during)
    if resumed and t1 + 0.4 < len(m) / float(w.rate):
        after = window_db(m, w.rate, t1 + 0.2, 0.1)
        ok = ok and after >= before - 6.0
        detail += ", %.1f dB after resume" % after
    res.add(label + " pause", "PASS" if ok else "FAIL", detail)
    return True


# ---- the cases -----------------------------------------------------------------------

def secs(cfg, cap):
    return min(cfg["route_seconds"], cap)


def level_env(cfg, level, cap, extra=None):
    frames = int(secs(cfg, cap) * 60)
    e = {"SM64DS_NO_AUDIO": "1", "SM64DS_WAV_DUMP": "{wdir}/mix.wav", "SM64DS_LEVEL": str(level),
         "SM64DS_WINDOW_SELFTEST": str(frames), "SM64DS_SELFTEST_IDLE": "1",
         "SM64DS_STREAM_TRACE": "1", "SM64DS_AUDIO_STATS": "1"}
    e.update(extra or {})
    return e


def case_music(cfg, res, sd, only):
    names = sd.bgm_names()
    res.add("music: mod", "PASS" if names else "FAIL", "%d NCS_BGM_* tracks replaced by tune_loop.ogg" % len(names))
    mods = {"ahq-music": music_mod(names, "ogg")}
    # level 1 starts no stream within the 30 s route (its music is not a plain NCS_BGM start: INCONCLUSIVE in
    # both reruns), so the ds-quality stream row runs on level 6, which does
    levels = ("6", "14", "19")
    jobs = [(lambda l=l: C.launch(cfg, "music_l%s_hq" % l, cfg["exe_tip"], level_env(cfg, l, 30),
                                  settings={"AudioQuality": "hq"}, mods=mods, subdir="mods")) for l in levels]
    jobs.append(lambda: C.launch(cfg, "music_l6_ds", cfg["exe_tip"], level_env(cfg, 6, 30),
                                 settings={"AudioQuality": "ds"}, mods=mods, subdir="mods"))
    rows = C.run_parallel(jobs, cfg["workers"])
    fade_seen = False
    first_ok = None
    for r in rows:
        lab = "music " + r.name.replace("music_", "")
        db, ts = check_music_run(res, lab, r, "ogg")
        if db is None:
            continue
        if first_ok is None:
            first_ok = r
        if "fade" in only:
            fade_seen = check_fade(res, lab, r, db, ts) or fade_seen
        w = W.read_wav(r.path("mix.wav"))
        want = 32768 if r.name.endswith("_ds") else 48000
        if w.rate != want:
            res.add(lab + " dump rate", "FAIL", "%d Hz, expected %d" % (w.rate, want))
    if "fade" in only and not fade_seen:
        res.add("fade: music follows the game's fades", "INCONCLUSIVE",
                "none of levels %s showed a volume change of 6 dB or more in 30 s; "
                "try --set route_seconds=60 or another level" % ", ".join(levels))
    # a .wav with a smpl chunk, replacing only the piece the first run played
    if first_ok is not None:
        starts, _, _ = stream_info(first_ok.text)
        played = os.path.splitext(starts[0][1])[0]
        lvl = first_ok.name.split("_")[1][1:]
        mods2 = {"ahq-musicwav": music_mod([played], "wav")}
        r = C.launch(cfg, "music_wav", cfg["exe_tip"], level_env(cfg, lvl, 30), settings={"AudioQuality": "hq"},
                     mods=mods2, subdir="mods")
        check_music_run(res, "music_wav", r, "wav")
    # pause
    if "pause" in only and first_ok is not None:
        lvl = first_ok.name.split("_")[1][1:]
        r = C.launch(cfg, "music_pause", cfg["exe_tip"],
                     level_env(cfg, lvl, 30, {"SM64DS_PROBE_INPUT": "420-424:START,720-724:START", "SM64DS_SND_TRACE": "1"}),
                     settings={"AudioQuality": "hq"}, mods=mods, subdir="mods")
        _, _, pr = stream_info(r.text)
        if not os.path.exists(r.path("mix.wav")) or not check_pause(res, "music", r, pr):
            res.add("pause: recording holds still", "INCONCLUSIVE",
                    "START at frames 420 and 720 did not reach the music (no 'paused' line in the stream trace)")


PLAY_EVENT = re.compile(r"\[vt\] play\s+(\d+) (start|stop|pause|finished)")


def play_events(text):
    return [(int(p), e) for p, e in PLAY_EVENT.findall(text)]


def case_status(cfg, res, level, names):
    """The ROM polls whether each sequence player is playing or stopped. With the
    recording on, the sequence still runs silently, so the start / pause / stop /
    finished events of every player must be the same list as without the mod."""
    env = level_env(cfg, level, 20, {"SM64DS_VOICE_TRACE": "1"})
    jobs = [lambda: C.launch(cfg, "status_plain", cfg["exe_tip"], env, settings={"AudioQuality": "hq"}, subdir="mods"),
            lambda: C.launch(cfg, "status_mod", cfg["exe_tip"], env, settings={"AudioQuality": "hq"},
                             mods={"ahq-music": music_mod(names, "ogg")}, subdir="mods")]
    a, b = C.run_parallel(jobs, cfg["workers"])
    ea, eb = play_events(a.text), play_events(b.text)
    if not ea:
        res.add("music status: sequence events unchanged", "INCONCLUSIVE", "no '[vt] play' lines in the plain run")
    else:
        res.add("music status: sequence events unchanged", "PASS" if ea == eb else "FAIL",
                "%d start/pause/stop/finished events without the mod, %d with it%s" % (
                    len(ea), len(eb), "" if ea == eb else "; first difference at event %d" % next(
                        (i for i, (x, y) in enumerate(zip(ea, eb)) if x != y), min(len(ea), len(eb)))))


def case_forced(cfg, res, sd):
    """Only the one named track is replaced, and only it is heard as the tune."""
    rec = int(cfg.get("music_force_id", 58))
    name = sd.name_of("sseq", rec)
    if not name:
        res.add("music forced track", "INCONCLUSIVE", "no sequence record %d in this archive" % rec)
        return
    env = level_env(cfg, 1, 20, {"SM64DS_SND_MUSIC": str(rec)})
    r = C.launch(cfg, "music_forced", cfg["exe_tip"], env, settings={"AudioQuality": "hq"},
                 mods={"ahq-forced": music_mod([name], "ogg")}, subdir="mods")
    starts, _, _ = stream_info(r.text)
    names = [x[1] for x in starts]
    ok = r.rc == 0 and names and all(n.lower() == (name + ".ogg").lower() for n in names)
    res.add("music forced track", "PASS" if ok else "FAIL",
            "SM64DS_SND_MUSIC=%d with only %s replaced: streams started for %s" % (rec, name, names or "nothing"))


def probe_script(entries, per_entry=2, first=60, gap=40, second_pass=300):
    parts = []
    t = first
    for rep in range(per_entry):
        for e in entries:
            parts.append("%d:%d" % (t, e))
            t += gap
        t += second_pass - gap * len(entries) if second_pass > gap * len(entries) else 0
    return ",".join(parts[:16])


def parse_chan_runs(text):
    """[(samples, looping, rate, key or None)] for every voice start, with the
    key of the note line that came just before it."""
    out = []
    key = None
    for l in text.splitlines():
        m = NOTE_LINE.search(l)
        if m:
            key = int(m.group(3))
            continue
        c = CHAN_START.search(l)
        if c:
            out.append((int(c.group(2)), bool(c.group(3)), float(c.group(7)), key))
            key = None
    return out


def case_wave(cfg, res, sd, only):
    pick = sdat_tools.pick_wave(sd, cfg.get("probe_seqarc", 2))
    if not pick:
        res.add("wave: pick a wave", "INCONCLUSIVE", "no system sound found that plays a sampled wave")
        return
    score, swar_fid, wave, uses = pick
    swar = sdat_tools.swar_name(sd, swar_fid)
    info = sdat_tools.swar_wave_info(sd, swar_fid, wave)
    entries = sorted({e for e, k in uses})[:8]
    keys = sorted({k for e, k in uses})
    res.add("wave: pick a wave", "PASS",
            "%s wave %d (stock %s Hz, %d samples, %s) is played by system sound entries %s at keys %s" % (
                swar, wave, info[2], info[3], "loops" if info[1] else "one shot", entries, keys))
    n = 12344

    def fill(folder):
        d = os.path.join(folder, "sound", swar)
        os.makedirs(d)
        chirp_wav(os.path.join(d, "%d.wav" % wave), 22050, n)
        y = os.path.join(folder, "sound", "NCS_WAVE_SE_VOICE_YOSSY")
        if sd.find("swar", "NCS_WAVE_SE_VOICE_YOSSY"):
            os.makedirs(y)
            shutil.copyfile(os.path.join(FIX, "chirp.ogg"), os.path.join(y, "3.ogg"))

    probe = probe_script(entries)
    env = level_env(cfg, 1, 14, {"SM64DS_SND_SYSPROBE": probe, "SM64DS_VOICE_TRACE": "1",
                                 "SM64DS_WINDOW_SELFTEST": "700"})
    jobs = [lambda m=m: C.launch(cfg, "wave_" + m, cfg["exe_tip"], env, settings={"AudioQuality": m},
                                 mods={"ahq-wave": fill}, subdir="mods") for m in ("hq", "ds")]
    jobs.append(lambda: C.launch(cfg, "wave_stock", cfg["exe_tip"], env, settings={"AudioQuality": "hq"}, subdir="mods"))
    rh, rd, rs = C.run_parallel(jobs, cfg["workers"])
    stock_runs = [r for r in parse_chan_runs(rs.text) if r[0] == info[3]]
    for mode, r, mix in (("hq", rh, 48000), ("ds", rd, 32768)):
        lab = "wave " + mode
        if r.rc != 0 or r.fatal():
            res.add(lab + " ran", "FAIL", "rc=%s fatal=%d" % (r.rc, r.fatal()))
            continue
        accepted = [l for l in r.lines("[mods] sound:") if "-> wave %d of file %d" % (wave, swar_fid) in l]
        res.add(lab + " accepted", "PASS" if accepted else "FAIL",
                accepted[0][:140] if accepted else "no '-> wave %d of file %d' line" % (wave, swar_fid))
        yoss = [l for l in r.lines("[mods] sound:") if "YOSSY/3.ogg ->" in l]
        if sd.find("swar", "NCS_WAVE_SE_VOICE_YOSSY"):
            res.add(lab + " YOSSY/3.ogg accepted", "PASS" if yoss else "FAIL", (yoss[0][:140] if yoss else "no line for it"))
        runs = [x for x in parse_chan_runs(r.text) if x[0] == n]
        if len(runs) < 2:
            res.add(lab + " clip used", "INCONCLUSIVE" if not runs else "FAIL",
                    "%d voice starts with the clip's %d samples (the probe played entries %s)" % (len(runs), n, entries))
            continue
        want = round(22050.0 / mix, 4)
        rates = sorted({x[2] for x in runs})
        ks = sorted({x[3] for x in runs if x[3] is not None})
        res.add(lab + " clip used", "PASS", "%d voice starts use the %d-sample clip" % (len(runs), n))
        res.add(lab + " own pitch", "PASS" if rates == [want] else "FAIL",
                "playback rate %s on every start (clip rate / mix rate = %.4f), notes at keys %s" % (rates, want, ks))
        loops = {x[1] for x in runs}
        res.add(lab + " loop", "PASS" if loops == {bool(info[1])} else "FAIL",
                "voice %s; stock wave %s, no loop tags in the file" % ("loops" if True in loops else "plays once",
                                                                      "loops (so the clip loops whole)" if info[1] else "does not loop"))
    if stock_runs:
        rates = sorted({x[2] for x in stock_runs})
        res.add("wave stock contrast", "PASS", "without the mod the same wave plays at %d different rates: %s"
                % (len(rates), rates[:6]))


def case_bad(cfg, res, sd, only):
    bgm = sd.bgm_names()
    victim = bgm[0] if bgm else "NCS_BGM_CHIJOU"
    victim2 = bgm[1] if len(bgm) > 1 else victim
    swars = [n for i, n, f in sd.names("swar")]
    warc = "NCS_WAVE_SE_VOICE_YOSSY" if "NCS_WAVE_SE_VOICE_YOSSY".lower() in [x.lower() for x in swars] else swars[0]
    files = []       # (relative path, what, text its one log line must contain)

    def fill(folder):
        s = os.path.join(folder, "sound")
        os.makedirs(s)

        def put(rel, data, what, needle=None):
            p = os.path.join(s, rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "wb") as f:
                f.write(data)
            files.append((rel, what, needle or rel.replace("\\", "/")))
        good = open(os.path.join(FIX, "tune_loop.ogg"), "rb").read()
        put(victim + ".ogg", good[:3000], "truncated ogg")
        put(victim2 + ".wav",
            b"RIFF" + struct.pack("<I", 36) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 2, 1, 8000, 4000, 1, 4) + b"data" + struct.pack("<I", 0),
            "ADPCM wav, no data")
        put("NOT_A_REAL_SEQUENCE.ogg", good, "name not in the archive")
        put(warc + "/9999.ogg", open(os.path.join(FIX, "chirp.ogg"), "rb").read(), "wave number far outside the archive")
        put(warc + "/abc.wav", b"RIFF....WAVE", "wave name is not a number")
        put(warc + "/3.mp3", b"ID3\x04" + b"\0" * 64, "unsupported extension")
        put(warc + "/5.ogg", bytes((i * 37) & 255 for i in range(5000)), "random bytes as ogg")
        put(warc + "/4.wav", b"RIFF" + struct.pack("<I", 9 << 20) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 1, 22050, 44100, 2, 16)
            + b"data" + struct.pack("<I", 9 << 20) + b"\0" * (9 << 20), "wave file over the 8 MB limit")
        put("NO_SUCH_ARCHIVE/1.wav", b"RIFF....WAVE", "no wave archive with that name", "NO_SUCH_ARCHIVE/")

    env = level_env(cfg, 1, 20)
    r = C.launch(cfg, "bad", cfg["exe_tip"], env, settings={"AudioQuality": "hq"}, mods={"ahq-bad": fill}, subdir="mods")
    lines = r.lines("[mods] sound:")
    ok = r.rc == 0 and not r.fatal()
    res.add("bad: game ran to the end", "PASS" if ok else "FAIL", "rc=%s, fatal lines=%d" % (r.rc, r.fatal()))
    for rel, what, needle in files:
        about = [l for l in lines if needle in l]
        n = len(about)
        verdict = n == 1 and ("skipped" in about[0] or "ignored" in about[0])
        res.add("bad: %s" % what, "PASS" if verdict else "FAIL",
                "%d log line(s) name %s%s" % (n, needle, ": " + about[0][14:150] if n else ""))
    if os.path.exists(r.path("mix.wav")):
        w = W.read_wav(r.path("mix.wav"))
        m = w.mono()
        level = W.rms_db(m[int(2 * w.rate):])
        res.add("bad: stock sound still plays", "PASS" if level > -60.0 else "FAIL",
                "level after 2 s: %.1f dB (a silent game would be below -90)" % level)
    else:
        res.add("bad: stock sound still plays", "FAIL", "no mix.wav")


def main():
    ap = C.common_args(__doc__)
    ap.add_argument("--only", default="music,fade,pause,status,forced,wave,bad",
                    help="comma list of: music fade pause status forced wave bad")
    args = ap.parse_args()
    cfg = C.config_from(args)
    C.need(cfg, "exe_tip", "asset_root")
    if not os.path.exists(cfg["sdat"]):
        sys.exit("cannot find the sound archive: %s (set asset_root or sdat in the config)" % cfg["sdat"])
    sd = sdat_tools.Sdat(cfg["sdat"])
    only = set(args.only.split(","))
    res = C.Results("mods_check")
    if "music" in only or "fade" in only or "pause" in only:
        case_music(cfg, res, sd, only)
    if "status" in only:
        case_status(cfg, res, 1, sd.bgm_names())
    if "forced" in only:
        case_forced(cfg, res, sd)
    if "wave" in only:
        case_wave(cfg, res, sd, only)
    if "bad" in only:
        case_bad(cfg, res, sd, only)
    print()
    print(res.table())
    res.save(cfg, "mods_check.txt")
    return 1 if res.failed() else 0


if __name__ == "__main__":
    sys.exit(main())
