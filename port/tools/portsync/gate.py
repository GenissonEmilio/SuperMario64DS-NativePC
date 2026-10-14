#!/usr/bin/env python3
"""portsync gate.py -- the W42 section 7 gate rows, in BOTH settings modes.

    python port/tools/portsync/gate.py [--config C] [--exe EXE] [--modes off,on]
                                       [--parts ident,traps,...] [--quick]

THE TWO SETTINGS MODES (P120 GATE NOTE). Since the menus / HUD defaults flipped
ON, the old pictures only come back with settings.json
{"OneScreenMenus": false, "HudLayout": false}:
  off  that settings.json is written into the row folder -> the W42 references
  on   NO settings.json (absent keys = new menus + N64 HUD) -> the new references
Every row says which mode it ran in.

ROWS (the starter kit's v1_gate.py / pe2_gate.py / pe2_ident.py recipes, own
quiet launches, at most runs.max_parallel at once):
  ident   identity: sha256 of walk_window_selftest.bmp (first 8 hex) for
          SM64DS_LEVEL 1 / 6 / 8 at SM64DS_WINDOW_SELFTEST 600 and 900 with
          SM64DS_SELFTEST_IDLE=1; F3-off (level 1, f600, no IDLE) and
          renderer-on (SM64DS_RENDERER=1) as md5 of the bmp. Both modes.
  traps   SM64DS_SCENE=1 300 frames, both arms (absent / all knobs on):
          "unmatched-body traps entered: 0".
  irq     heap after 600 frames and root heap at the end, level 1 and 6.
  ab      SM64DS_RASTER_AB=1 levels 1 / 6 / 19: VERDICT=PASS, 0 bad frames.
  twod    SM64DS_TWOD_VERIFY=1 levels 1 / 6 / 19: 0 differ, 0 MISMATCH lines.
  opening port/tools/tail2_states_proof.py (the opening gate, 4000 frames).
  twostar port/tools/twostar_fade_proof.py.
  ending  the whole ending to THE END (level 40, 10660 frames, spawn 283:7).
traps / irq / ab / twod / ending run in mode "off" only (they are not
pictures; the ending picture reference is an OFF one). opening and twostar are
the tree's own tools: they run the TREE's build/port/walk_window.exe with their
own quiet launch, so they are skipped when the exe under test is not that one.

A row with no reference for its mode is RECORD (value printed, no verdict).
--quick drops opening / twostar / ending.
"""
import argparse
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import portsync_common as C  # noqa: E402

# W42 section 7 (OFF) and the current ON references from the task brief. The
# ON lvl6 value is given once ("l6 64035479"); like W42's single lvl6 value it
# stands for both f600 and f900.
REFERENCES = {
    "off": {
        "ident_l1_f600": "0a913ea7", "ident_l1_f900": "754f4973",
        "ident_l6_f600": "cd4c7a33", "ident_l6_f900": "cd4c7a33",
        "ident_l8_f600": "12486be4", "ident_l8_f900": "650f9a20",
        "f3off": "946a90a4", "rendereron": "c49dbf32",
        "traps_absent": 0, "traps_all": 0,
        "irq_L1": {"heap": 164800, "root": 5920128},
        "irq_L6": {"heap": 67592, "root": 5862008},
        "ab_L1": "PASS", "ab_L6": "PASS", "ab_L19": "PASS",
        "tw_L1": 0, "tw_L6": 0, "tw_L19": 0,
        "opening": {"distinct": 362, "frames": "2504..3999"},
        "twostar": "PASS",
        "ending": {"theend": "d44a9218", "changes": 22},
    },
    "on": {
        "ident_l6_f600": "64035479", "ident_l6_f900": "64035479",
        "ident_l8_f600": "6f8b80a4", "ident_l8_f900": "03ead794",
    },
}

ALL_PARTS = ["ident", "traps", "irq", "ab", "twod", "opening", "twostar", "ending"]
SLOW_PARTS = ["opening", "twostar", "ending"]

AB = re.compile(r"\[raster-ab\] summary over (\d+) frame\(s\).*?mismatches colour (\d+) depth (\d+) "
                r"coverage (\d+) stencil (\d+) attrid (\d+) tlattr (\d+), frames with any (\d+) VERDICT=(\w+)")
TW = re.compile(r"\[twod\] SUMMARY (\w+) verify: (\d+) [\w-]+ compared, (\d+) differ")
TRAPS = re.compile(r"unmatched-body traps entered: (\d+)")
HEAP = re.compile(r"\[heap\] (\d+) free after (\d+) frames")
ROOT = re.compile(r"\[heap\] root heap (\d+) free at the end of the run")
OPEN_PASS = re.compile(r"OPENING GATE: PASS -- .*takes (\d+) distinct positions over frames (\d+)\.\.(\d+)")
TWOSTAR = re.compile(r"TWO-STAR FADE PROOF: (\w+) -- (.*)")

NA = {"SM64DS_NO_AUDIO": "1"}


def references(cfg):
    ref = {m: dict(v) for m, v in REFERENCES.items()}
    for mode, rows in (cfg.get("gate", {}).get("references") or {}).items():
        ref.setdefault(mode, {}).update(rows)
    return ref


# ---------------------------------------------------------------- the rows

def ident_rows():
    rows = []
    for lvl in ("1", "6", "8"):
        for fr in ("600", "900"):
            rows.append(("ident_l%s_f%s" % (lvl, fr),
                         {"SM64DS_LEVEL": lvl, "SM64DS_WINDOW_SELFTEST": fr, "SM64DS_SELFTEST_IDLE": "1"},
                         "sha8"))
    rows.append(("f3off", {"SM64DS_LEVEL": "1", "SM64DS_WINDOW_SELFTEST": "600"}, "md58"))
    rows.append(("rendereron", {"SM64DS_LEVEL": "1", "SM64DS_WINDOW_SELFTEST": "600",
                                "SM64DS_RENDERER": "1"}, "md58"))
    return rows


def other_rows(parts):
    rows = []
    if "traps" in parts:
        rows.append(("traps_absent", {"SM64DS_SCENE": "1", "SM64DS_SCENE_FRAMES": "300"}, "traps"))
        rows.append(("traps_all", {"SM64DS_SCENE": "1", "SM64DS_SCENE_FRAMES": "300",
                                   "SM64DS_RENDER_SCALE": "4", "SM64DS_SMOOTH_MODELS": "2",
                                   "SM64DS_TEXTURE_FILTER": "2", "SM64DS_ANTI_ALIASING": "1",
                                   "SM64DS_RENDERER": "1", "SM64DS_PRESENT_BACKEND": "1"}, "traps"))
    if "irq" in parts:
        for lvl in ("1", "6"):
            rows.append(("irq_L%s" % lvl, {"SM64DS_LEVEL": lvl, "SM64DS_WINDOW_SELFTEST": "600",
                                           "SM64DS_IRQ_CENSUS": "1"}, "irq"))
    if "ab" in parts:
        for n in ("1", "6", "19"):
            rows.append(("ab_L%s" % n, dict(NA, SM64DS_RASTER_AB="1", SM64DS_LEVEL=n,
                                            SM64DS_WINDOW_SELFTEST="300", SM64DS_STARS_SEED="0:01"), "ab"))
    if "twod" in parts:
        for n in ("1", "6", "19"):
            rows.append(("tw_L%s" % n, dict(NA, SM64DS_TWOD_VERIFY="1", SM64DS_LEVEL=n,
                                            SM64DS_WINDOW_SELFTEST="600", SM64DS_STARS_SEED="0:01"), "twod"))
    if "ending" in parts:
        rows.append(("ending", {"SM64DS_LEVEL": "40", "SM64DS_WINDOW_SELFTEST": "10660",
                                "SM64DS_SELFTEST_IDLE": "1", "SM64DS_TRACE_SPAWN": "1",
                                "SM64DS_TRACE_STATE": "2", "SM64DS_SPAWN_ACTOR": "283:7"}, "ending"))
    return rows


# --------------------------------------------------------- row evaluation

def measure(kind, out, bmp_sha256, bmp_md5):
    """The row's value out of its log / picture. Pure (selftest-able)."""
    if kind == "sha8":
        return bmp_sha256[:8] if bmp_sha256 != "NO_FILE" else None
    if kind == "md58":
        return bmp_md5[:8] if bmp_md5 != "NO_FILE" else None
    if kind == "traps":
        m = TRAPS.findall(out)
        return int(m[-1]) if m else None
    if kind == "irq":
        h = HEAP.findall(out)
        r = ROOT.findall(out)
        if not h and not r:
            return None
        return {"heap": int(h[-1][0]) if h else None, "root": int(r[-1]) if r else None}
    if kind == "ab":
        m = AB.findall(out)
        if not m:
            return None
        last = m[-1]
        return last[8] if int(last[7]) == 0 else "%s(%s bad frames)" % (last[8], last[7])
    if kind == "twod":
        s = TW.findall(out)
        if not s:
            return None
        mism = sum(1 for l in out.splitlines() if "[twod]" in l and "MISMATCH" in l)
        return sum(int(x[2]) for x in s) + mism
    if kind == "ending":
        q = sum(1 for l in out.splitlines() if "[quarantine]" in l)
        back = sum(1 for l in out.splitlines() if "BACKSTOP" in l)
        ch = sum(1 for l in out.splitlines() if "[lvl] change:" in l)
        return {"theend": bmp_md5[:8] if bmp_md5 != "NO_FILE" else None, "changes": ch,
                "quarantine": q, "backstop": back}
    raise ValueError(kind)


def verdict(name, kind, value, ref, rc_ok):
    """(status, text). ref None -> RECORD."""
    if not rc_ok:
        return C.FAIL, "run failed"
    if value is None:
        return C.FAIL, "no value (line / picture missing)"
    if kind == "ending":
        bad = []
        if value["quarantine"] or value["backstop"]:
            bad.append("quarantine %d backstop %d" % (value["quarantine"], value["backstop"]))
        if ref is None:
            return (C.FAIL if bad else C.RECORD), "; ".join(bad) or "no reference"
        if value["theend"] != ref["theend"]:
            bad.append("THE END %s != %s" % (value["theend"], ref["theend"]))
        if value["changes"] != ref["changes"]:
            bad.append("%d level changes != %d" % (value["changes"], ref["changes"]))
        return (C.FAIL, "; ".join(bad)) if bad else (C.PASS, "== reference")
    if ref is None:
        return C.RECORD, "no reference for this mode"
    if kind == "irq":
        bad = ["%s %s != %s" % (k, value.get(k), v) for k, v in ref.items() if value.get(k) != v]
        return (C.FAIL, "; ".join(bad)) if bad else (C.PASS, "== reference")
    if value == ref:
        return C.PASS, "== reference"
    return C.FAIL, "%s != reference %s" % (value, ref)


def show(value):
    if isinstance(value, dict):
        return " ".join("%s=%s" % (k, value[k]) for k in sorted(value))
    return str(value)


def run_row(cfg, exe, name, env, kind, mode, refs, label):
    tag = "g_%s_%s_%s" % (label, mode, name)
    timeout = 4800 if kind == "ending" else None
    r = C.launch(cfg, exe, tag, env, settings_mode=mode, timeout=timeout)
    value = measure(kind, r.out, C.sha256_file(r.bmp()), C.md5_file(r.bmp()))
    rc_ok = (not r.timed_out) and r.rc == 0 and not r.fatal_lines
    ref = refs.get(mode, {}).get(name)
    st, why = verdict(name, kind, value, ref, rc_ok)
    return [name, mode, st, show(value), show(ref) if ref is not None else "-",
            "%s; %s" % (r.describe(), why)]


# ---------------------------------------------------------- tree tools

def tree_tool_row(cfg, exe, which, refs):
    root = cfg["tree_root"]
    tree_exe = os.path.join(root, "build", "port", "walk_window.exe")
    if C.md5_file(tree_exe) != C.md5_file(exe):
        return [which, "tool", C.SKIP, "-", "-",
                "the tree's build/port/walk_window.exe is not the exe under test "
                "(this tool only runs the tree exe)"]
    C.wait_for_foreign(cfg["runs"])
    out_dir = os.path.join(cfg["work_dir"], "gate_tools")
    os.makedirs(out_dir, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("SM64DS_")}
    env["TEMP"] = env["TMP"] = out_dir
    if which == "opening":
        cmd = [cfg["python"], os.path.join(root, "port", "tools", "tail2_states_proof.py"), root,
               "--frames", "4000", "--outroot", out_dir, "--tag", "portsync_opening"]
    else:
        cmd = [cfg["python"], os.path.join(root, "port", "tools", "twostar_fade_proof.py"), root]
    p = subprocess.run(cmd, cwd=root, env=env, capture_output=True, text=True,
                       errors="replace", timeout=7200)
    text = p.stdout + p.stderr
    with open(os.path.join(out_dir, which + ".log"), "w", encoding="ascii", errors="replace") as f:
        f.write(text)
    ref = refs.get("off", {}).get(which)
    if which == "opening":
        m = OPEN_PASS.search(text)
        if not m:
            line = [l.strip() for l in text.splitlines() if "OPENING GATE" in l]
            return [which, "tool", C.FAIL, "-", show(ref), (line[-1] if line else "no OPENING GATE line")[:160]]
        val = {"distinct": int(m.group(1)), "frames": "%s..%s" % (m.group(2), m.group(3))}
        st = C.PASS if (ref is None or val == ref) else C.FAIL
        return [which, "tool", st, show(val), show(ref), "rc=%s" % p.returncode]
    m = TWOSTAR.findall(text)
    if not m:
        return [which, "tool", C.FAIL, "-", show(ref), "no TWO-STAR FADE PROOF line, rc=%s" % p.returncode]
    st = C.PASS if m[-1][0] == "PASS" else C.FAIL
    return [which, "tool", st, m[-1][0], show(ref), m[-1][1][:160]]


# ------------------------------------------------------------------- run

def run(cfg, exe, modes=None, parts=None, label="gate", quick=False):
    modes = modes or cfg["gate"].get("modes") or ["off", "on"]
    parts = list(parts or cfg["gate"].get("parts") or ALL_PARTS)
    if quick:
        parts = [p for p in parts if p not in SLOW_PARTS]
    refs = references(cfg)
    if not os.path.exists(exe):
        return C.StepResult("gate", C.ERROR, "no exe at %s" % exe)
    jobs = []
    if "ident" in parts:
        for mode in modes:
            for name, env, kind in ident_rows():
                jobs.append((name, env, kind, mode))
    for name, env, kind in other_rows(parts):
        jobs.append((name, env, kind, "off"))
    rows = []
    with C.phase_lock(cfg, "portsync-gate"):
        workers = max(1, int(cfg["runs"].get("max_parallel") or 1))
        with ThreadPoolExecutor(workers) as ex:
            futs = [ex.submit(run_row, cfg, exe, n, e, k, m, refs, label) for n, e, k, m in jobs]
            for f in futs:
                try:
                    rows.append(f.result())
                except C.ForeignWaitTimeout as e:
                    rows.append(["?", "?", C.ERROR, "-", "-", str(e)])
        for which in ("opening", "twostar"):
            if which in parts:
                try:
                    rows.append(tree_tool_row(cfg, exe, which, refs))
                except Exception as e:  # a tool crash is a row, not a traceback
                    rows.append([which, "tool", C.ERROR, "-", "-", repr(e)[:160]])
    return summarize(rows, exe, modes)


def summarize(rows, exe, modes):
    nfail = sum(1 for r in rows if r[2] in C.HARD)
    nrec = sum(1 for r in rows if r[2] == C.RECORD)
    npass = sum(1 for r in rows if r[2] == C.PASS)
    nskip = sum(1 for r in rows if r[2] == C.SKIP)
    details = C.table(rows, ["ROW", "MODE", "RESULT", "VALUE", "REFERENCE", "RUN"]).splitlines()
    status = C.FAIL if nfail else C.PASS
    first = [r for r in rows if r[2] in C.HARD]
    summ = "%d rows: %d pass, %d fail, %d record-only, %d skipped (modes %s)" % (
        len(rows), npass, nfail, nrec, nskip, "+".join(modes))
    if first:
        summ += "; first fail: %s/%s %s" % (first[0][0], first[0][1], first[0][5][-80:])
    return C.StepResult("gate", status, summ, details, data={"rows": rows, "exe_md5": C.md5_file(exe)})


def main(argv=None):
    ap = C.common_args(argparse.ArgumentParser(description="portsync: W42 gate rows, both settings modes"))
    ap.add_argument("--exe", help="exe to gate (default: the tree's build/port/walk_window.exe)")
    ap.add_argument("--modes", help="comma list of off,on")
    ap.add_argument("--parts", help="comma list of " + ",".join(ALL_PARTS))
    ap.add_argument("--quick", action="store_true", help="skip opening / twostar / ending")
    ap.add_argument("--label", default="gate")
    a = ap.parse_args(argv)
    r = C.handle_write_config(a)
    if r is not None:
        return r
    try:
        cfg = C.load_config(a.config, require=("work_dir",))
    except C.ConfigError as e:
        print("gate: %s" % e)
        return 2
    C.open_log(cfg, "gate")
    exe = a.exe or os.path.join(cfg["tree_root"], "build", "port", "walk_window.exe")
    res = run(cfg, exe, a.modes.split(",") if a.modes else None,
              a.parts.split(",") if a.parts else None, a.label, a.quick)
    for line in res.details:
        C.log(line)
    print(C.table([[res.step, res.status, res.summary]], ["STEP", "RESULT", "WHAT"]))
    return 0 if res.status == C.PASS else 1


if __name__ == "__main__":
    sys.exit(main())
