#!/usr/bin/env python3
"""portsync run_all.py -- build -> linkage -> gate -> compare, one table.

    python port/tools/portsync/run_all.py                 everything
    python port/tools/portsync/run_all.py --quick         skip the slow rows / routes
    python port/tools/portsync/run_all.py --new-exe EXE   no build: test this exe
    python port/tools/portsync/run_all.py --write-config  make the config file
    python port/tools/portsync/run_all.py --selftest      check the scripts' logic

Stops at the FIRST hard fail (FAIL or ERROR); the steps after it are shown as
NOT RUN. WARN does not stop. The table is also written to
<work_dir>/reports/run_<time>.txt. Exit code 0 = everything passed (or
warned), 1 = a step failed, 2 = the config needs filling in.
"""
import argparse
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import portsync_common as C  # noqa: E402

STEPS = ["build", "linkage", "gate", "compare"]


def run_steps(steps):
    """steps: [(name, callable(state) -> StepResult)]. Runs in order, stops at
    the first hard fail. Returns [StepResult]."""
    state = {}
    out = []
    stopped = None
    for name, fn in steps:
        if stopped:
            out.append(C.StepResult(name, C.NOTRUN, "stopped after %s failed" % stopped))
            continue
        t0 = time.time()
        try:
            r = fn(state)
        except C.ForeignWaitTimeout as e:
            r = C.StepResult(name, C.ERROR, "gave up waiting for another game window: %s" % e)
        except Exception as e:  # a crash in a step is that step's ERROR
            r = C.StepResult(name, C.ERROR, "script error: %r" % (e,))
        r.data.setdefault("minutes", round((time.time() - t0) / 60.0, 1))
        state[name] = r
        out.append(r)
        if r.hard:
            stopped = name
    return out


def report(results):
    rows = [[r.step, r.status, "%s" % r.data.get("minutes", "-"), r.summary[:150]] for r in results]
    t = C.table(rows, ["STEP", "RESULT", "MIN", "WHAT"])
    bad = [r for r in results if r.hard]
    overall = ("OVERALL: FAIL at %s" % bad[0].step) if bad else "OVERALL: PASS"
    return t + "\n" + overall, (1 if bad else 0)


def make_steps(cfg, a):
    label = a.label or time.strftime("r%Y%m%d_%H%M")

    def s_build(st):
        if a.new_exe:
            st["exe"] = os.path.abspath(a.new_exe)
            return C.StepResult("build", C.SKIP, "no build: testing %s (md5 %s)"
                                % (st["exe"], C.md5_file(st["exe"])[:8]))
        import build
        r = build.run_build(cfg, label)
        st["exe"] = r.data.get("exe")
        return r

    def s_linkage(st):
        import linkage
        exe = st["exe"]
        return linkage.run(cfg, os.path.splitext(exe)[0] + ".map", exe)

    def s_gate(st):
        import gate
        return gate.run(cfg, st["exe"], label=label, quick=a.quick)

    def s_compare(st):
        import compare
        return compare.run(cfg, cfg["old_exe"], st["exe"], quick=a.quick, label=label)

    fns = {"build": s_build, "linkage": s_linkage, "gate": s_gate, "compare": s_compare}
    only = a.only.split(",") if a.only else STEPS
    return [(n, fns[n]) for n in STEPS if n in only or (n == "build")]


def main(argv=None):
    ap = C.common_args(argparse.ArgumentParser(description="portsync: build, linkage, gate, compare"))
    ap.add_argument("--quick", action="store_true", help="skip opening / two-star / ending")
    ap.add_argument("--new-exe", help="skip the build and test this exe (its .map next to it)")
    ap.add_argument("--only", help="comma list of steps to run after the build: linkage,gate,compare")
    ap.add_argument("--label")
    ap.add_argument("--selftest", action="store_true", help="run the logic self-test (no game, no build)")
    a = ap.parse_args(argv)
    if a.selftest:
        import test_portsync
        return test_portsync.main()
    r = C.handle_write_config(a)
    if r is not None:
        return r
    need = ["work_dir", "old_exe"] + ([] if a.new_exe else ["build.lock_path"])
    try:
        cfg = C.load_config(a.config, require=need)
    except C.ConfigError as e:
        print("run_all: %s" % e)
        return 2
    logp = C.open_log(cfg, "run_all")
    C.log("portsync run_all, config %s, log %s" % (cfg["_path"], logp))
    results = run_steps(make_steps(cfg, a))
    for r in results:
        if r.details:
            C.log("---- %s details" % r.step)
            for line in r.details:
                C.log(line)
    text, rc = report(results)
    rdir = os.path.join(cfg["work_dir"], "reports")
    os.makedirs(rdir, exist_ok=True)
    rp = os.path.join(rdir, "run_%s.txt" % time.strftime("%Y%m%d_%H%M%S"))
    with open(rp, "w", encoding="ascii", errors="replace") as f:
        f.write(text + "\n")
    C.log("")
    for line in text.splitlines():
        C.log(line)
    C.log("table saved to %s" % rp)
    return rc


if __name__ == "__main__":
    sys.exit(main())
