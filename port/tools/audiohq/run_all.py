#!/usr/bin/env python3
"""run_all.py: one command, one PASS / FAIL table.

    python run_all.py              runs audio_check, mods_check and gate
    python run_all.py --quick      the same with short routes (about a third of the time)
    python run_all.py --parts audio,mods      only some of them (audio mods gate)
    python run_all.py --build      build the tip exe first (build.py)

Needs config.json (see README.md). Nothing here plays sound or takes the focus:
every game launch is muted, minimized and never activated. When it is done the
table is printed and saved in <out_dir>/RESULTS.txt; if anything FAILed (or could
not be decided) a folder <out_dir>/send_back/ and send_back.zip hold exactly what
to send back.
"""
import os
import shutil
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audiohq_common as C  # noqa: E402

SCRIPTS = {"audio": "audio_check.py", "mods": "mods_check.py", "gate": "gate.py"}
OUTFILE = {"audio": "audio_check.txt", "mods": "mods_check.txt", "gate": "gate.txt"}


def collect(cfg, tables):
    sb = os.path.join(cfg["out_dir"], "send_back")
    if os.path.isdir(sb):
        shutil.rmtree(sb)
    os.makedirs(sb)
    with open(os.path.join(sb, "RESULTS.txt"), "w", encoding="utf-8") as f:
        f.write("\n\n".join(tables) + "\n")
    info = ["exe_tip  %s  md5 %s" % (cfg.get("exe_tip"), C.md5(cfg.get("exe_tip", ""))),
            "exe_base %s  md5 %s" % (cfg.get("exe_base"), C.md5(cfg.get("exe_base", ""))),
            "route_seconds %s, workers %s" % (cfg["route_seconds"], cfg["workers"])]
    with open(os.path.join(sb, "versions.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(info) + "\n")
    n = 0
    for base, dirs, files in os.walk(cfg["out_dir"]):
        if os.sep + "send_back" in base or os.sep + "exes" in base or os.sep + "tmpbuild" in base:
            continue
        if "out.txt" in files:
            rel = os.path.relpath(base, cfg["out_dir"]).replace(os.sep, "_")
            lines = open(os.path.join(base, "out.txt"), errors="replace").read().splitlines()
            keep = lines if len(lines) <= 600 else lines[:200] + ["... (%d lines left out) ..." % (len(lines) - 500)] + lines[-300:]
            with open(os.path.join(sb, rel + ".log.txt"), "w", encoding="utf-8") as f:
                f.write("\n".join(keep) + "\n")
            n += 1
    z = os.path.join(cfg["out_dir"], "send_back.zip")
    with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED) as zf:
        for fn in os.listdir(sb):
            zf.write(os.path.join(sb, fn), fn)
    return sb, z, n


def main():
    ap = C.common_args(__doc__)
    ap.add_argument("--parts", default="audio,mods,gate")
    ap.add_argument("--build", action="store_true", help="build the tip exe first")
    ap.add_argument("rest", nargs="*")
    args = ap.parse_args()
    cfg = C.config_from(args)
    passthru = []
    if args.config:
        passthru += ["--config", args.config]
    for s in args.set:
        passthru += ["--set", s]
    if args.quick:
        passthru.append("--quick")
    codes = {}
    if args.build:
        print("== build ==", flush=True)
        rc = subprocess.call([sys.executable, os.path.join(HERE, "build.py")] + passthru)
        if rc:
            print("the build failed; stopping")
            return 1
        cfg = C.config_from(args)
        cfg["exe_tip"] = os.path.join(cfg["out_dir"], "exes", "tip", "walk_window.exe")
        passthru += ["--set", 'exe_tip=%s' % __import__("json").dumps(cfg["exe_tip"])]
    for part in args.parts.split(","):
        part = part.strip()
        if part not in SCRIPTS:
            sys.exit("unknown part %r (audio mods gate)" % part)
        print("\n==== %s ====" % part, flush=True)
        codes[part] = subprocess.call([sys.executable, os.path.join(HERE, SCRIPTS[part])] + passthru)
    tables = []
    for part in codes:
        p = os.path.join(cfg["out_dir"], OUTFILE[part])
        tables.append(open(p, encoding="utf-8").read().rstrip() if os.path.exists(p)
                      else "== %s ==\nNO RESULT FILE (the script stopped before it finished)" % part)
    allrows = "\n\n".join(tables)
    undecided = allrows.count(" INCONCLUSIVE ")
    failed = any(codes.values()) or " FAIL " in allrows or "NO RESULT FILE" in allrows
    print("\n" + "=" * 72)
    print(allrows)
    print("=" * 72)
    with open(os.path.join(cfg["out_dir"], "RESULTS.txt"), "w", encoding="utf-8") as f:
        f.write(allrows + "\n")
    if failed or undecided:
        sb, z, n = collect(cfg, tables)
        print("\n%s. SEND BACK: %s  (it holds RESULTS.txt, the exe versions and the game log of each of the %d runs;"
              " the folder %s has the same files)" % ("SOMETHING FAILED" if failed else "NOTHING FAILED but %d row(s) could not be decided" % undecided,
                                                      z, n, sb))
    else:
        print("\nALL PASS. Nothing to send back.")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
