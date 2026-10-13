#!/usr/bin/env python3
"""build.py: build the game exe from a tree through the machine's build queue.

The shape of the staged queue the other lanes use (stage.py): when config has
stage_py, the build goes through `stage.py run`; else through the tree's own
port/tools/build_lock.py if it has one (the machine-wide lock); else it runs the
build script directly. Either way TEMP / TMP point at a private folder, the free
disk space is checked first, and the finished exe and its .map are copied to
out_dir/exes/<label>/ with their md5, because a stale exe looks fresh.

usage: python build.py [--which tip|base] [--label NAME] [--tree PATH]
  tip   builds config["repo"] (this branch)            -> out_dir/exes/tip/
  base  builds config["base_tree"] (a tree at the snapshot you compare against)
        -> out_dir/exes/base/. Set base_tree in the config; make it with
        `git worktree add <path> origin/lead/audio-snapshot` and do the same
        one-time worktree set-up any fresh tree needs (asset links, the asset
        catalog). If you already have a baseline exe, skip this and set exe_base.
Extra arguments after -- go to the build script.
"""
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audiohq_common as C  # noqa: E402


def main():
    ap = C.common_args(__doc__)
    ap.add_argument("--which", default="tip", choices=("tip", "base"))
    ap.add_argument("--label", default="")
    ap.add_argument("--tree", default="")
    ap.add_argument("rest", nargs="*", help="extra arguments for the build script (after --)")
    args = ap.parse_args()
    cfg = C.config_from(args)
    label = args.label or args.which
    tree = args.tree or (cfg["repo"] if args.which == "tip" else cfg.get("base_tree", ""))
    if not tree or not os.path.isdir(tree):
        sys.exit("no tree to build: %r (set repo / base_tree in the config, or --tree)" % tree)
    out = os.path.join(cfg["out_dir"], "exes", label)
    os.makedirs(out, exist_ok=True)
    logdir = os.path.join(cfg["out_dir"], "logs")
    os.makedirs(logdir, exist_ok=True)
    free_gb = shutil.disk_usage(tree).free / 1e9
    if free_gb < 5.0:
        sys.exit("only %.1f GB free on the tree's drive; a build needs room (pause under 5 GB)" % free_gb)
    busy = C.busy_processes(cfg)
    if busy:
        print("machine busy (%s): the build runs at low priority" % ", ".join(busy))
    tmp = os.path.join(cfg["out_dir"], "tmpbuild")
    os.makedirs(tmp, exist_ok=True)
    env = dict(os.environ)
    env["TEMP"] = env["TMP"] = tmp
    if cfg.get("build_lock_path"):
        env["SM64DS_BUILD_LOCK_PATH"] = cfg["build_lock_path"]
    env["SM64DS_BUILD_LOCK_TIMEOUT"] = "14400"
    shell = ["cmd", "/c"] if os.name == "nt" else ["sh", "-c"]
    script = cfg["build_cmd"]
    lock_py = os.path.join(tree, "port", "tools", "build_lock.py")
    if cfg.get("stage_py"):
        cmd = [sys.executable, cfg["stage_py"], "run", "--label", "audiohq1-" + label, "--root", tree, "--"] + shell + [script] + args.rest
    elif os.path.exists(lock_py):
        cmd = [sys.executable, lock_py, "run", "--label", "audiohq1-" + label, "--root", tree, "--"] + shell + [script] + args.rest
    else:
        cmd = shell + [script] + args.rest
    kw = {}
    if os.name == "nt":
        kw["creationflags"] = 0x00004000 if busy else 0          # BELOW_NORMAL_PRIORITY_CLASS
    log = os.path.join(logdir, "build_%s.log" % label)
    print("building %s in %s\n  %s\n  log: %s" % (label, tree, " ".join(cmd), log), flush=True)
    t0 = time.time()
    with open(log, "wb") as fh:
        rc = subprocess.call(cmd, cwd=tree, env=env, stdout=fh, stderr=subprocess.STDOUT, **kw)
    print("build finished: rc=%d, %.0f s" % (rc, time.time() - t0))
    exe = os.path.join(tree, "build", "port", "walk_window.exe")
    if rc != 0 or not os.path.exists(exe):
        print("BUILD FAILED. The last lines of the log:\n" + C.tail(open(log, errors="replace").read(), 30))
        return 1
    for ext in (".exe", ".map"):
        src = os.path.splitext(exe)[0] + ext
        if os.path.exists(src):
            shutil.copyfile(src, os.path.join(out, "walk_window" + ext))
    got = os.path.join(out, "walk_window.exe")
    print("exe  %s\n  md5 %s\nmap  md5 %s" % (got, C.md5(got), C.md5(os.path.splitext(got)[0] + ".map")))
    print('config line:  "exe_%s": "%s"' % (args.which, got.replace("\\", "/")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
