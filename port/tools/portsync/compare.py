#!/usr/bin/env python3
"""portsync compare.py -- run the SAME scripted routes on the OLD exe and the
NEW exe and compare them frame by frame.

    python port/tools/portsync/compare.py [--config C] --new EXE [--old EXE]
                                          [--routes a,b] [--quick] [--list-routes]

WHAT IS COMPARED, PER FRAME (all of it is the harness's own instruments,
inert unless set; see port/tests/walk_window.cpp):
  state   SM64DS_SCENE_RAMHASH=1 -> "[scene-ramhash] f<frame> <main> <arena>"
          every tick, on the level loop AND the scene loop: FNV-1a of the
          4 MB DS main RAM and of the game arena. SM64DS_SCENE_RAMMASK=<file>
          (u32 word indexes into the arena) zeroes words before hashing;
          SM64DS_SCENE_RAMDUMP=<frame>:<path> writes the arena of that tick
          to <path>.arena. (the masked RAM hash recipe of P120 EXACTNESS)
  picture SM64DS_SCENE_PICHASH=1 -> "[pichash] t<tick> <hash>" for every scene
          tick (title, file select, star select, minigames), and
          SM64DS_PRESENTER_HASH=<prefix> -> <prefix>.main, "<serial> <hash>"
          of every picture the game thread hands the presenter (level loop).
          Plus the final walk_window_selftest.bmp.

THE NOISE FLOOR. Every route runs the OLD exe twice (compare.noise_runs) and,
with compare.new_noise, the NEW exe twice too. A frame / tick whose value the
two runs of ONE exe disagree on is unstable and is excluded (reported, not
compared). A field unstable on more than compare.max_unstable_fraction of its
frames is excluded whole -- for the arena that triggers THE MASK:
  1. each run also dumps the arena at the route's dump frame;
  2. words that differ between two runs of one exe = noise (uninitialised
     words, clocks, host pointers): masked;
  3. words that differ between OLD and NEW where both values are pointers
     into each exe's own image and name the same symbol + offset (the code
     moved, the meaning did not) = relocation: masked;
  4. the routes re-run with SM64DS_SCENE_RAMMASK; at a first differing frame
     both exes are dumped AT that frame and classified again (up to
     compare.drill_max times). A word that is neither noise nor relocation is
     a REAL difference: FAIL, with the word, both values and their symbols.
NEW-only instability (a frame stable on OLD but not on NEW) is reported as a
WARN: it smells of a new uninitialised read.

A route whose knobs are not spelled in either exe is SKIPPED with the reason
(the harness does not support it). A route that never reached its marker
(e.g. the pole route never entered a Climb / SwingPlayer state) is a WARN:
tune its spawn in the config.
"""
import argparse
import json
import os
import re
import struct
import sys
import time
from collections import OrderedDict
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import portsync_common as C  # noqa: E402

ARENA_BASE = 0x30000000          # hal/os_arena.cpp ARENA_FIXED_BASE
ARENA_SPAN = (0x30000000, 0x40000000)
DS_SPAN = (0x01ff8000, 0x02800000)

STATE_KNOBS = ["SM64DS_SCENE_RAMHASH", "SM64DS_SCENE_RAMDUMP", "SM64DS_SCENE_RAMMASK"]
PIC_KNOBS = ["SM64DS_SCENE_PICHASH", "SM64DS_PRESENTER_HASH"]

# Player state functions (config/arm9/overlays/ov002/symbols.txt), printed by
# SM64DS_TRACE_STATE=2 as "[state] 0x<addr>" on every change.
POLE_STATES = {"0x020cbd34": "St_Climb_Init", "0x020cb5bc": "St_Climb_Main",
               "0x020da3b0": "St_SwingPlayer_Init", "0x020d9fec": "St_SwingPlayer_Main"}

TAIL2_TOUCH = "560-563:44:68,620-623:44:68,680-683:44:68,760-763:44:68,860-863:44:68"

# Every route: env (with {frames}), frames, optional fresh save, markers.
ROUTES = OrderedDict([
    ("title_fileselect", {
        "desc": "boot -> title -> file select (SM64DS_SKIP_MENU taps the title)",
        "frames": 900, "fresh_save": True,
        "env": {"SM64DS_SKIP_MENU": "1", "SM64DS_SCENE_WINDOW": "1", "SM64DS_SCENE_FRAMES": "{frames}"}}),
    ("castle_grounds", {
        "desc": "castle grounds, the selftest walk + hop",
        "frames": 900, "env": {"SM64DS_LEVEL": "1"}}),
    ("lvl6", {"desc": "level 6 (Bob-omb Battlefield), walk + hop", "frames": 900,
              "env": {"SM64DS_LEVEL": "6"}}),
    ("lvl8", {"desc": "level 8, walk + hop", "frames": 900, "env": {"SM64DS_LEVEL": "8"}}),
    ("minigame_slot", {
        "desc": "minigame scene 364 (dScMgSlot1_c), windowed scene path",
        "frames": 600,
        "env": {"SM64DS_SCENE": "364", "SM64DS_SCENE_WINDOW": "1", "SM64DS_SCENE_FRAMES": "{frames}"}}),
    ("star_select", {
        "desc": "the star select (scene 4, dScStarSel_c::Behavior), no input",
        "frames": 600,
        "env": {"SM64DS_SCENE": "4", "SM64DS_SCENE_WINDOW": "1", "SM64DS_SCENE_FRAMES": "{frames}"}}),
    ("pole_swing", {
        "desc": "level 13 at the POLES1 pole (SPAWN 100,2100,3680), walking: Player Climb / SwingPlayer states",
        "frames": 600, "watch_states": POLE_STATES,
        "env": {"SM64DS_LEVEL": "13", "SM64DS_SPAWN": "100,2100,3680", "SM64DS_TRACE_STATE": "2"}}),
    ("opening", {
        "desc": "the opening (tail2_states_proof.py recipe: new file on a fresh save)",
        "frames": 4000, "fresh_save": True, "slow": True,
        "expect": ["the opening is ARMED for this entry"],
        "env": {"SM64DS_SKIP_MENU": "1", "SM64DS_SCENE_WINDOW": "1", "SM64DS_SCENE_FRAMES": "100000",
                "SM64DS_TOUCH_PROBE": TAIL2_TOUCH, "SM64DS_NO_AUDIO": "1", "SM64DS_NO_PLAYLOG": "1"}}),
    ("ending", {
        "desc": "the ending to THE END (v1_end.py recipe: level 40, star cutscene spawn 283:7)",
        "frames": 10660, "slow": True, "expect": ["[lvl] change:"],
        "env": {"SM64DS_LEVEL": "40", "SM64DS_SELFTEST_IDLE": "1", "SM64DS_SPAWN_ACTOR": "283:7"}}),
])


def routes(cfg):
    out = OrderedDict()
    over = cfg.get("compare", {}).get("routes") or {}
    for name, r in ROUTES.items():
        r = dict(r, env=dict(r["env"]))
        o = over.get(name) or {}
        for k, v in o.items():
            if k == "env":
                r["env"].update(v)
            else:
                r[k] = v
        out[name] = r
    for name, o in over.items():           # extra routes from the config
        if name not in out and isinstance(o, dict) and o.get("env"):
            out[name] = dict({"desc": "config route", "frames": 600}, **o)
    return out


def route_env(route, wdir, mode):
    frames = int(route["frames"])
    env = {k: str(v).replace("{frames}", str(frames)) for k, v in route["env"].items()}
    env["SM64DS_WINDOW_SELFTEST"] = str(frames)
    env["SM64DS_SCENE_RAMHASH"] = "1"
    if mode.get("pic", True):
        env["SM64DS_SCENE_PICHASH"] = "1"
        env["SM64DS_PRESENTER_HASH"] = os.path.join(wdir, "pres")
    if route.get("fresh_save"):
        env["SM64DS_SAVE_PATH"] = os.path.join(wdir, "sm64ds.sav")
    if mode.get("dump") is not None:
        env["SM64DS_SCENE_RAMDUMP"] = "%d:%s" % (mode["dump"], os.path.join(wdir, "dump"))
    if mode.get("mask"):
        env["SM64DS_SCENE_RAMMASK"] = mode["mask"]
    return env


# ------------------------------------------------------------ the streams

RAMHASH = re.compile(r"\[scene-ramhash\] f(-?\d+) ([0-9a-fA-F]{8}) ([0-9a-fA-F]{8})")
PICHASH = re.compile(r"\[pichash\] t(\d+) ([0-9a-fA-F]{8})")
PRES = re.compile(r"^(-?\d+) ([0-9a-fA-F]{8})\s*$")
FIELDS = ("pic", "main", "arena")


def parse_streams(out, presenter_main=None):
    s = {"pic": OrderedDict(), "main": OrderedDict(), "arena": OrderedDict()}
    seen = {}
    for m in RAMHASH.finditer(out):
        f = int(m.group(1))
        n = seen.get(f, 0)
        seen[f] = n + 1
        k = "f%d#%d" % (f, n)
        s["main"][k] = m.group(2).lower()
        s["arena"][k] = m.group(3).lower()
    for m in PICHASH.finditer(out):
        s["pic"]["t" + m.group(1)] = m.group(2).lower()
    for ln in (presenter_main or "").splitlines():
        m = PRES.match(ln)
        if m:
            s["pic"]["s" + m.group(1)] = m.group(2).lower()
    return s


def key_frame(k):
    m = re.match(r"[fts](-?\d+)", k)
    return int(m.group(1)) if m else None


def evaluate_field(olds, news, frac):
    """olds / news: one {key: value} per run of that exe. Pure."""
    res = {"status": "unavailable", "keys": 0, "old_unstable": [], "new_only_unstable": [],
           "compared": 0, "first_diff": None}
    if not any(olds):
        return res
    order = list(olds[0].keys())
    have = set(order)
    for o in olds[1:]:
        for k in o:
            if k not in have:
                order.append(k)
                have.add(k)
    old_stable = OrderedDict((k, olds[0][k]) for k in order
                             if all(k in o and o[k] == olds[0][k] for o in olds))
    res["keys"] = len(order)
    res["old_unstable"] = [k for k in order if k not in old_stable]
    if order and float(len(res["old_unstable"])) / len(order) > frac:
        res["status"] = "unstable"
        return res
    news = [n for n in news] or [OrderedDict()]
    new_stable = {k: news[0][k] for k in news[0] if all(k in n and n[k] == news[0][k] for n in news)}
    new_any = set()
    for n in news:
        new_any |= set(n)
    new_unstable = new_any - set(new_stable)
    first = None
    for k in order:
        if k not in old_stable:
            continue
        if k in new_unstable:
            res["new_only_unstable"].append(k)
            continue
        if k not in new_stable:
            first = (k, old_stable[k], "MISSING")
            break
        res["compared"] += 1
        if new_stable[k] != old_stable[k]:
            first = (k, old_stable[k], new_stable[k])
            break
    if first is None:
        extra = [k for k in news[0] if k not in have]
        if extra:
            first = (extra[0], "MISSING", news[0][extra[0]])
    res["first_diff"] = first
    res["status"] = "diff" if first else "same"
    return res


def evaluate(old_streams, new_streams, frac, fields=FIELDS):
    return OrderedDict((f, evaluate_field([s[f] for s in old_streams], [s[f] for s in new_streams], frac))
                       for f in fields)


# --------------------------------------------------------------- the mask

def words(blob):
    n = len(blob) // 4
    return struct.unpack("<%dI" % n, blob[:n * 4])


def diff_words(a, b):
    wa, wb = words(a), words(b)
    n = min(len(wa), len(wb))
    return [i for i in range(n) if wa[i] != wb[i]]


def noise_words(dumps):
    out = set()
    for d in dumps[1:]:
        out |= set(diff_words(dumps[0], d))
    return out


class Image(object):
    """An exe's image range + its map, for pointer classification."""

    def __init__(self, base, end, mapinfo):
        self.base, self.end, self.map = base, end, mapinfo

    def has(self, v):
        return self.base <= v < self.end

    def sym(self, v):
        if not self.has(v) or self.map is None:
            return None
        return self.map.symbolize(v)


def classify(idx, old_words, new_words, old_img, new_img):
    """{index: 'reloc' | 'hostptr' | 'real'} for differing words. Pure."""
    out = {}
    for i in idx:
        a, b = old_words[i], new_words[i]
        sa = old_img.sym(a) if old_img else None
        sb = new_img.sym(b) if new_img else None
        if sa is not None and sa == sb:
            out[i] = "reloc"
        elif (0x10000 <= a < 0x80000000 and 0x10000 <= b < 0x80000000 and
              not any(lo <= v < hi for v in (a, b) for lo, hi in (ARENA_SPAN, DS_SPAN)) and
              not (old_img and old_img.has(a)) and not (new_img and new_img.has(b))):
            out[i] = "hostptr"
        else:
            out[i] = "real"
    return out


def write_mask(path, idx):
    idx = sorted(set(idx))
    with open(path, "wb") as f:
        f.write(struct.pack("<%dI" % len(idx), *idx))
    return len(idx)


def read_mask(path):
    with open(path, "rb") as f:
        return list(words(f.read()))


def describe_word(i, a, b, old_img, new_img):
    def sy(img, v):
        s = img.sym(v) if img else None
        return ("%s+0x%x" % s) if s else "-"
    return ("arena+0x%06x (0x%08x if at the fixed base): old 0x%08x [%s]  new 0x%08x [%s]"
            % (i * 4, ARENA_BASE + i * 4, a, sy(old_img, a), b, sy(new_img, b)))


# ------------------------------------------------------------- the routes

def load_image(exe):
    try:
        pe = C.pe_info(exe)
    except Exception:
        return None
    m = os.path.splitext(exe)[0] + ".map"
    mi = C.MapInfo(m) if os.path.exists(m) else None
    return Image(pe["base"], pe["end"], mi)


def read(p):
    try:
        with open(p, "rb") as f:
            return f.read()
    except OSError:
        return None


def read_text(p):
    b = read(p)
    return b.decode("ascii", "replace") if b is not None else None


class Runner(object):
    """Launches route runs (parallel up to runs.max_parallel)."""

    def __init__(self, cfg, label):
        self.cfg, self.label = cfg, label
        self.n = 0

    def rowdir(self, tag):
        return os.path.join(self.cfg["work_dir"], "rows", tag)

    def batch(self, route_name, route, specs):
        """specs: [(which 'old'|'new', exe, mode dict)] -> [(which, RunResult, streams)]"""
        ccfg = self.cfg["compare"]
        jobs = []
        for which, exe, mode in specs:
            self.n += 1
            tag = "c_%s_%s_%s%d" % (self.label, route_name, which, self.n)
            env = route_env(route, self.rowdir(tag), mode)
            jobs.append((which, exe, tag, env))

        def one(j):
            which, exe, tag, env = j
            r = C.launch(self.cfg, exe, tag, env, settings_mode=ccfg.get("settings_mode", "off"),
                         settings_extra=ccfg.get("settings_extra") or None,
                         timeout=max(int(self.cfg["runs"]["timeout_seconds"]), int(route["frames"]) // 2))
            st = parse_streams(r.out, read_text(os.path.join(r.wdir, "pres.main")))
            return which, r, st
        with ThreadPoolExecutor(max(1, int(self.cfg["runs"].get("max_parallel") or 1))) as ex:
            return list(ex.map(one, jobs))


def run_problems(results):
    bad = []
    for which, r, _ in results:
        if not r.ok:
            why = "TIMEOUT" if r.timed_out else "rc=%s" % r.rc
            if r.fatal_lines:
                why += " " + r.fatal_lines[0].strip()[:100]
            elif r.quarantine_lines:
                why += " " + r.quarantine_lines[0].strip()[:100]
            bad.append((which, why, r.wdir))
    return bad


def final_pictures(results):
    olds = set(C.sha256_file(r.bmp())[:8] for w, r, _ in results if w == "old")
    news = set(C.sha256_file(r.bmp())[:8] for w, r, _ in results if w == "new")
    return olds, news


def marker_notes(route, results):
    notes = []
    old_out = [r.out for w, r, _ in results if w == "old"]
    new_out = [r.out for w, r, _ in results if w == "new"]
    for e in route.get("expect") or []:
        if old_out and e not in old_out[0]:
            notes.append("WEAK: OLD never printed %r (route did not reach its marker)" % e)
    ws = route.get("watch_states")
    if ws:
        def entered(text):
            return sorted(set(ws[a] for a in re.findall(r"\[state\] (0x[0-9a-f]{8})", text) if a in ws))
        eo = entered(old_out[0]) if old_out else []
        en = entered(new_out[0]) if new_out else []
        if not eo:
            notes.append("WEAK: OLD never entered %s (tune the spawn in the config)" % "/".join(sorted(set(ws.values()))))
        else:
            notes.append("states entered OLD %s NEW %s" % (",".join(eo), ",".join(en)))
    return notes


def compare_route(cfg, runner, name, route, old_exe, new_exe, images):
    ccfg = cfg["compare"]
    frac = float(ccfg.get("max_unstable_fraction", 0.5))
    n_old = max(2, int(ccfg.get("noise_runs") or 2))
    n_new = n_old if ccfg.get("new_noise", True) else 1
    row = {"route": name, "desc": route.get("desc", ""), "status": C.PASS, "first_diff": "-",
           "excluded": "-", "notes": []}
    knobs = sorted(set(list(route["env"].keys()) + STATE_KNOBS))
    for tag, exe in (("OLD", old_exe), ("NEW", new_exe)):
        miss = C.exe_supports(exe, knobs)
        if miss:
            row.update(status=C.SKIP, notes=["the %s exe does not support %s (harness lacks it)"
                                             % (tag, ", ".join(miss))])
            return row
    pic_ok = not C.exe_supports(old_exe, PIC_KNOBS[:1]) or not C.exe_supports(old_exe, PIC_KNOBS[1:])
    dump_at = int(route.get("dump_frame") or max(1, int(route["frames"]) // 2))
    C.log("route %s: %s" % (name, route.get("desc", "")))

    def specs(mode):
        mode = dict(mode, pic=pic_ok)
        return [("old", old_exe, mode)] * n_old + [("new", new_exe, mode)] * n_new

    # phase 1: unmasked, with an arena dump at the dump frame
    res = runner.batch(name, route, specs({"dump": dump_at}))
    bad = run_problems(res)
    row["notes"] += marker_notes(route, res)
    if bad:
        olds = [b for b in bad if b[0] == "old"]
        if olds:
            row.update(status=C.ERROR, first_diff="-",
                       notes=row["notes"] + ["OLD exe fails this route: %s (%s); fix the route first"
                                             % (olds[0][1], olds[0][2])])
        else:
            row.update(status=C.FAIL, first_diff="run",
                       notes=row["notes"] + ["NEW exe fails this route: %s (%s)" % (bad[0][1], bad[0][2])])
        return row
    mask_info = []
    ev = evaluate([s for w, _, s in res if w == "old"], [s for w, _, s in res if w == "new"], frac)
    if ev["arena"]["status"] in ("unstable", "diff") and ev["pic"]["status"] != "diff" \
            and ev["main"]["status"] != "diff":
        ev, mask_info = masked_phases(cfg, runner, name, route, res, ev, images, specs, frac, dump_at,
                                      old_exe, new_exe)
    for d in mask_info:
        row["notes"].append(d)
    olds, news = final_pictures(res)
    return finish_row(row, ev, olds, news)


def masked_phases(cfg, runner, name, route, res, ev, images, specs, frac, dump_at, old_exe, new_exe):
    """Build the arena mask from the dumps, re-run masked, drill into the first
    differing frame. Returns (evaluation, notes)."""
    ccfg = cfg["compare"]
    old_img, new_img = images
    notes = []
    od = [read(os.path.join(r.wdir, "dump.arena")) for w, r, _ in res if w == "old"]
    nd = [read(os.path.join(r.wdir, "dump.arena")) for w, r, _ in res if w == "new"]
    if not all(od) or not all(nd):
        notes.append("no arena dump at f%d (route ends earlier?): arena compared unmasked" % dump_at)
        return ev, notes
    mask, real = build_mask(od, nd, old_img, new_img, ccfg.get("mask_host_pointers"), notes, "f%d" % dump_at)
    mdir = os.path.join(cfg["work_dir"], "compare", runner.label)
    os.makedirs(mdir, exist_ok=True)
    mpath = os.path.join(mdir, "%s.mask" % name)
    for it in range(int(ccfg.get("drill_max", 3)) + 1):
        write_mask(mpath, mask)
        res2 = runner.batch(name, route, specs({"mask": mpath}))
        bad = run_problems(res2)
        if bad:
            notes.append("masked re-run failed: %s %s" % (bad[0][0], bad[0][1]))
            ev["arena"]["status"] = "diff"
            ev["arena"]["first_diff"] = ("run", bad[0][0], bad[0][1])
            return ev, notes
        ev = evaluate([s for w, _, s in res2 if w == "old"], [s for w, _, s in res2 if w == "new"], frac)
        a = ev["arena"]
        notes.append("masked pass %d: %d words masked, arena %s" % (it + 1, len(mask), a["status"]))
        if a["status"] != "diff" or a["first_diff"][0] == "run":
            return ev, notes
        k = a["first_diff"][0]
        f = key_frame(k)
        if f is None or it == int(ccfg.get("drill_max", 3)):
            return ev, notes
        dm = {"dump": f, "mask": mpath, "pic": False}
        dres = runner.batch(name, route, [("old", old_exe, dm), ("old", old_exe, dm), ("new", new_exe, dm)])
        od2 = [read(os.path.join(r.wdir, "dump.arena")) for w, r, _ in dres if w == "old"]
        nd2 = [read(os.path.join(r.wdir, "dump.arena")) for w, r, _ in dres if w == "new"]
        if not all(od2) or not all(nd2):
            notes.append("drill at %s: no dump written" % k)
            return ev, notes
        before = set(mask)
        mask2, real2 = build_mask(od2, nd2, old_img, new_img, ccfg.get("mask_host_pointers"),
                                  notes, k, base_mask=before)
        if real2:
            a["first_diff"] = (k, a["first_diff"][1], a["first_diff"][2])
            a["real_words"] = real2
            return ev, notes
        if mask2 == before:
            notes.append("drill at %s: no differing word outside the mask in the dump "
                         "(the frame repeats or the difference is in main RAM)" % k)
            return ev, notes
        mask = mask2
    return ev, notes


def build_mask(od, nd, old_img, new_img, mask_host, notes, where, base_mask=None):
    """(mask set, [real word descriptions]). Pure apart from `notes`."""
    mask = set(base_mask or ())
    if len(od[0]) != len(nd[0]):
        notes.append("%s: arena size differs, old %d new %d bytes (compared over the common part)"
                     % (where, len(od[0]), len(nd[0])))
    noise = noise_words(od) | noise_words(nd)
    new_noise = noise_words(nd) - noise_words(od)
    cross = [i for i in diff_words(od[0], nd[0]) if i not in noise and i not in mask]
    ow, nw = words(od[0]), words(nd[0])
    cls = classify(cross, ow, nw, old_img, new_img)
    counts = {}
    for c in cls.values():
        counts[c] = counts.get(c, 0) + 1
    notes.append("%s: noise %d words (%d NEW-only), OLD/NEW differ %d: %s" % (
        where, len(noise), len(new_noise), len(cross),
        ", ".join("%s %d" % kv for kv in sorted(counts.items())) or "none"))
    mask |= noise
    mask |= set(i for i, c in cls.items() if c == "reloc" or (mask_host and c == "hostptr"))
    real = [describe_word(i, ow[i], nw[i], old_img, new_img) + ("" if c == "real" else "  (%s)" % c)
            for i, c in sorted(cls.items()) if c == "real" or (c == "hostptr" and not mask_host)]
    return mask, real


def finish_row(row, ev, old_pics, new_pics):
    diffs, excl, warn = [], [], []
    usable = 0
    for f, r in ev.items():
        if r["status"] == "diff":
            k, a, b = r["first_diff"]
            diffs.append("%s %s (old %s new %s)" % (f, k, a, b))
            for w in (r.get("real_words") or [])[:12]:
                row["notes"].append("REAL " + w)
        if r["status"] == "unstable":
            excl.append("%s whole field (%d/%d unstable)" % (f, len(r["old_unstable"]), r["keys"]))
        elif r["old_unstable"]:
            excl.append("%s %d of %d (first %s)" % (f, len(r["old_unstable"]), r["keys"], r["old_unstable"][0]))
        if r["new_only_unstable"]:
            warn.append("%s: %d frames stable on OLD but not on NEW (first %s): a new uninitialised read?"
                        % (f, len(r["new_only_unstable"]), r["new_only_unstable"][0]))
        if r["status"] in ("same", "diff"):
            usable += 1
    if len(old_pics) == 1 and old_pics != new_pics:
        diffs.append("final picture old %s new %s" % (sorted(old_pics)[0], ",".join(sorted(new_pics))))
    elif len(old_pics) > 1:
        excl.append("final picture (OLD runs disagree)")
    row["excluded"] = "; ".join(excl) or "none"
    row["notes"] += warn
    if diffs:
        row["status"] = C.FAIL
        row["first_diff"] = " | ".join(diffs)
    elif not usable:
        row["status"] = C.ERROR
        row["notes"].append("nothing comparable: every field unavailable or unstable")
    elif warn or any(n.startswith("WEAK") for n in row["notes"]):
        row["status"] = C.WARN
    return row


# ------------------------------------------------------------------- run

def run(cfg, old_exe, new_exe, names=None, quick=False, label=None):
    label = label or time.strftime("c%m%d_%H%M")
    rts = routes(cfg)
    for p in (old_exe, new_exe):
        if not os.path.exists(p):
            return C.StepResult("compare", C.ERROR, "no exe at %s" % p)
    images = (load_image(old_exe), load_image(new_exe))
    runner = Runner(cfg, label)
    rows = []
    with C.phase_lock(cfg, "portsync-compare"):
        for name, route in rts.items():
            if names and name not in names:
                continue
            if route.get("enabled") is False:
                rows.append({"route": name, "status": C.SKIP, "first_diff": "-", "excluded": "-",
                             "notes": ["disabled in the config"]})
                continue
            if quick and route.get("slow"):
                rows.append({"route": name, "status": C.SKIP, "first_diff": "-", "excluded": "-",
                             "notes": ["slow route, skipped by --quick"]})
                continue
            try:
                rows.append(compare_route(cfg, runner, name, route, old_exe, new_exe, images))
            except C.ForeignWaitTimeout as e:
                rows.append({"route": name, "status": C.ERROR, "first_diff": "-", "excluded": "-",
                             "notes": [str(e)]})
            r = rows[-1]
            C.log("route %s: %s %s" % (name, r["status"], r["first_diff"]))
    out = os.path.join(cfg["work_dir"], "compare", label)
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "report.json"), "w", encoding="ascii") as f:
        json.dump({"old": old_exe, "new": new_exe, "old_md5": C.md5_file(old_exe),
                   "new_md5": C.md5_file(new_exe), "rows": rows}, f, indent=1)
    return summarize(rows)


def summarize(rows):
    t = C.table([[r["route"], r["status"], r["first_diff"][:90], r["excluded"][:60]] for r in rows],
                ["ROUTE", "RESULT", "FIRST DIFFERENCE", "EXCLUDED AS NOISE"]).splitlines()
    for r in rows:
        for n in r["notes"]:
            t.append("  %s: %s" % (r["route"], n))
    hard = [r for r in rows if r["status"] in C.HARD]
    warn = [r for r in rows if r["status"] == C.WARN]
    ran = [r for r in rows if r["status"] != C.SKIP]
    status = C.FAIL if hard else (C.WARN if warn else (C.PASS if ran else C.SKIP))
    summ = "%d routes: %d same, %d differ/error, %d warn, %d skipped" % (
        len(rows), sum(1 for r in rows if r["status"] == C.PASS), len(hard), len(warn),
        len(rows) - len(ran))
    if hard:
        summ += "; first: %s %s" % (hard[0]["route"], hard[0]["first_diff"][:80])
    return C.StepResult("compare", status, summ, t, data={"rows": rows})


def main(argv=None):
    ap = C.common_args(argparse.ArgumentParser(description="portsync: OLD vs NEW exe, frame by frame"))
    ap.add_argument("--old", help="OLD exe (default: old_exe from the config)")
    ap.add_argument("--new", help="NEW exe (default: the tree's build/port/walk_window.exe)")
    ap.add_argument("--routes", help="comma list of routes")
    ap.add_argument("--quick", action="store_true", help="skip the slow routes (opening, ending)")
    ap.add_argument("--list-routes", action="store_true")
    ap.add_argument("--label")
    a = ap.parse_args(argv)
    r = C.handle_write_config(a)
    if r is not None:
        return r
    if a.list_routes:
        for n, rt in ROUTES.items():
            print("%-18s %5d frames  %s" % (n, rt["frames"], rt["desc"]))
        return 0
    try:
        cfg = C.load_config(a.config, require=("work_dir", "old_exe"))
    except C.ConfigError as e:
        print("compare: %s" % e)
        return 2
    C.open_log(cfg, "compare")
    new = a.new or os.path.join(cfg["tree_root"], "build", "port", "walk_window.exe")
    res = run(cfg, a.old or cfg["old_exe"], new, a.routes.split(",") if a.routes else None,
              a.quick, a.label)
    for line in res.details:
        C.log(line)
    print(C.table([[res.step, res.status, res.summary]], ["STEP", "RESULT", "WHAT"]))
    return 0 if res.status in (C.PASS, C.WARN) else 1


if __name__ == "__main__":
    sys.exit(main())
