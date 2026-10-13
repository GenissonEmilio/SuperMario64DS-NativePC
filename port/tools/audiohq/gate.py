#!/usr/bin/env python3
"""gate.py: the audio work must not change the game.

The identity rows of the lane gate (W42 section 7) plus the GATE NOTE modes,
each run on the BASE exe (built before this work) and on the TIP exe in BOTH
AudioQuality modes ("ds" and "hq") and with no AudioQuality key at all. A row
passes when every variant gives the same answer; the pictures (sha256 of the
selftest image), the trap counts, the RASTER_AB and TWOD_VERIFY verdicts, the
heap lines and the game's state hash (masked RAM hash) must not move.

usage: python gate.py [--quick] [--parts ident,ab,twod,irq]   (state is also available; audio_check (c) already runs it)
   ident  pictures at levels 1, 6, 8 (frames 600 and 900), F3-off, renderer on,
          trap counts; each in the default modes and in the legacy-menu mode
          (OneScreenMenus / HudLayout false)
   ab     RASTER_AB at levels 1, 6, 19      twod  TWOD_VERIFY at levels 1, 6, 19
   irq    heap / thread lines at levels 1 and 6
   state  the masked RAM hash, frame by frame, at levels 1 and 6
Each answer is printed; the old reference values are printed beside as
information only (they were taken on 0.5.6 and the baseline has moved since).
"""
import hashlib
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audiohq_common as C  # noqa: E402
import audio_check  # noqa: E402

REF_W42 = {   # informational: 0.5.6 values from W42 section 7
    "ident_l1": "0a913ea7 / 754f4973", "ident_l6": "cd4c7a33", "ident_l8": "12486be4 / 650f9a20",
    "f3off": "946a90a4", "rendereron": "c49dbf32", "traps": "0 of 24 both arms",
}

AB = re.compile(r"\[raster-ab\] summary over (\d+) frame\(s\).*?mismatches colour (\d+) depth (\d+) coverage (\d+) stencil (\d+) attrid (\d+) tlattr (\d+), frames with any (\d+) VERDICT=(\w+)")
TW = re.compile(r"\[twod\] SUMMARY (\w+) verify: (\d+) [\w-]+ compared, (\d+) differ")


def pic(row):
    return "sha256=" + C.sha256(row.path("walk_window_selftest.bmp"))[:8]


TRAP_SUMMARY = re.compile(r"unmatched-body traps entered: (\d+)")


def traps(row):
    """Traps entered. The game's own summary line ("traps entered: 0 (none of the 24 trapping
    sites was reached)") says 'trapping site' even when the count is 0, so it must not be
    counted as a trap line: use its number, else count the other trap lines."""
    m = TRAP_SUMMARY.findall(row.text)
    if m:
        return "trap-lines=%d" % int(m[-1])
    return "trap-lines=%d" % sum(1 for l in row.text.splitlines()
                                 if "unmatched-body trap" in l.lower() or "trapping site" in l.lower())


def ab(row):
    m = AB.findall(row.text)
    return "NO-AB-LINE" if not m else "frames %s colour %s depth %s coverage %s stencil %s attrid %s tlattr %s badframes %s VERDICT=%s" % m[-1]


def tw(row):
    s = TW.findall(row.text)
    mism = sum(1 for l in row.text.splitlines() if "[twod]" in l and "MISMATCH" in l)
    return " ".join("%s=%s/%s" % x for x in s) + " mismatch-lines=%d" % mism


def irq(row):
    keep = [l.strip()[:160] for l in row.text.splitlines()
            if l.strip().startswith(("[heap]", "[thr]", "[lvl] root heap")) or "dmadone" in l]
    return hashlib.md5("\n".join(keep[:40]).encode()).hexdigest()[:8] + " (%d lines)" % len(keep[:40])


def absolute_problem(name, tok):
    """Rows with an absolute requirement besides 'all variants agree'."""
    # traps_*: no absolute rule. The 0.9.x baseline decides the count; the row fails when any
    # variant differs from the base ("variants differ"), i.e. when the audio work adds a trap.
    if name.startswith("ab_") and not re.search(r"colour 0 depth 0 coverage 0 stencil 0 attrid 0 tlattr 0 badframes 0", tok):
        return "RASTER_AB must show 0 bad"
    if name.startswith("tw_") and (" mismatch-lines=0" not in tok or re.search(r"=\d+/[1-9]", tok)):
        return "TWOD_VERIFY must show 0 differ"
    if "NO-AB-LINE" in tok:
        return "no raster-ab summary line"
    return ""


def rows_for(parts):
    out = []
    if "ident" in parts:
        for lvl in ("1", "6", "8"):
            for fr in ("600", "900"):
                out.append(("ident_l%s_f%s" % (lvl, fr), {"SM64DS_LEVEL": lvl, "SM64DS_WINDOW_SELFTEST": fr,
                                                          "SM64DS_SELFTEST_IDLE": "1"}, pic))
        out.append(("f3off", {"SM64DS_LEVEL": "1", "SM64DS_WINDOW_SELFTEST": "600"}, pic))
        out.append(("rendereron", {"SM64DS_LEVEL": "1", "SM64DS_WINDOW_SELFTEST": "600", "SM64DS_RENDERER": "1"}, pic))
        out.append(("traps_absent", {"SM64DS_SCENE": "1", "SM64DS_SCENE_FRAMES": "300"}, traps))
        out.append(("traps_all", {"SM64DS_SCENE": "1", "SM64DS_SCENE_FRAMES": "300", "SM64DS_RENDER_SCALE": "4",
                                  "SM64DS_SMOOTH_MODELS": "2", "SM64DS_TEXTURE_FILTER": "2", "SM64DS_ANTI_ALIASING": "1",
                                  "SM64DS_RENDERER": "1", "SM64DS_PRESENT_BACKEND": "1"}, traps))
    if "irq" in parts:
        for lvl in ("1", "6"):
            out.append(("irq_L%s" % lvl, {"SM64DS_LEVEL": lvl, "SM64DS_WINDOW_SELFTEST": "600", "SM64DS_IRQ_CENSUS": "1"}, irq))
    na = {"SM64DS_NO_AUDIO": "1"}
    if "ab" in parts:
        for n in (1, 6, 19):
            out.append(("ab_L%d" % n, dict(na, SM64DS_RASTER_AB="1", SM64DS_LEVEL=n, SM64DS_WINDOW_SELFTEST=300,
                                           SM64DS_STARS_SEED="0:01"), ab))
    if "twod" in parts:
        for n in (1, 6, 19):
            out.append(("tw_L%d" % n, dict(na, SM64DS_TWOD_VERIFY="1", SM64DS_LEVEL=n, SM64DS_WINDOW_SELFTEST=600,
                                           SM64DS_STARS_SEED="0:01"), tw))
    return out


def variants(cfg):
    """(label, exe, settings) for every way a row is run."""
    return [("base", cfg["exe_base"], {}),
            ("tip_ds", cfg["exe_tip"], {"AudioQuality": "ds"}),
            ("tip_hq", cfg["exe_tip"], {"AudioQuality": "hq"}),
            ("tip_default", cfg["exe_tip"], {})]


LEGACY_ROWS = ("ident_l1_f600", "ident_l6_f600", "ident_l8_f600", "f3off", "rendereron")
MODES = [("default", {}), ("legacy-menus", {"OneScreenMenus": False, "HudLayout": False})]


def main():
    ap = C.common_args(__doc__)
    ap.add_argument("--parts", default="ident,ab,twod,irq", help="also: state (the masked RAM hash; audio_check does it by default)")
    args = ap.parse_args()
    cfg = C.config_from(args)
    C.need(cfg, "exe_tip", "exe_base", "asset_root")
    parts = set(args.parts.split(","))
    res = C.Results("gate")
    rows = rows_for(parts)
    jobs, meta = [], []
    for (name, env, chk) in rows:
        modes = MODES if name in LEGACY_ROWS else MODES[:1]
        for mname, extra in modes:
            for vlabel, exe, st in variants(cfg):
                s = dict(st)
                s.update(extra)
                tag = "%s_%s_%s" % (name, mname[:3], vlabel)
                jobs.append(lambda tag=tag, exe=exe, env=env, s=s: C.launch(cfg, tag, exe, env, settings=s, subdir="gate"))
                meta.append((name, mname, vlabel, chk))
    got = C.run_parallel(jobs, cfg["workers"]) if jobs else []
    table = {}
    for (name, mname, vlabel, chk), row in zip(meta, got):
        bad = "" if (row.rc == 0 and not row.fatal()) else " rc=%s fatal=%d" % (row.rc, row.fatal())
        table.setdefault((name, mname), {})[vlabel] = chk(row) + bad
    for (name, mname), v in table.items():
        vals = set(v.values())
        label = "%s [%s]" % (name, mname)
        if len(vals) != 1:
            diff = "; ".join("%s: %s" % (k, x) for k, x in sorted(v.items()))
            res.add(label, "FAIL", "variants differ: " + diff)
            continue
        tok = v["base"]
        why = absolute_problem(name, tok)
        res.add(label, "FAIL" if why else "PASS", "all four variants: %s%s" % (tok, "; " + why if why else ""))
    if "state" in parts:
        for name in ("l1_idle", "l6_idle"):
            lvl = name[1]
            env = {"SM64DS_NO_AUDIO": "1", "SM64DS_LEVEL": lvl, "SM64DS_WINDOW_SELFTEST": "900",
                   "SM64DS_SELFTEST_IDLE": "1", "SM64DS_STARS_SEED": "0:01"}
            audio_check.check_state(cfg, res, name, env)
    print()
    print(res.table())
    print("(reference values from W42 section 7, 0.5.6, information only: %s)" % "; ".join(
        "%s %s" % (k, v) for k, v in REF_W42.items()))
    res.save(cfg, "gate.txt")
    return 1 if res.failed() else 0


if __name__ == "__main__":
    sys.exit(main())
