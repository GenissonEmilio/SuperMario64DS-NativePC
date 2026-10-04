#!/usr/bin/env python3
"""ARENA SOLO GATE (run mp1, lane ARENAGATE1; the rows of ARENACORE1's gate.py, made a
function the ladder's arena-solo-gate rung calls under its own lock): the arena code
is inert without its env. Runs the same rows on a BASE exe (the frozen stage-5 build:
INT69B's game) and on the TIP under test and compares: identity 6/6 (sha256 of the
selftest picture, idle), F3-off, renderer-on, traps (keys absent / everything on), a
solo VS boot (map 0, 4 bodies, no comms: the [vs] probe positions and pads of every
frame) and a lockstep 2P loopback VS (its two consoles print the same [vs] probe).
Quiet: hidden, muted, minimized, no focus; IDLE priority only while League / VRChat run.

  python arena_solo_gate.py <base_exe> <tip_exe> <asset_root> <outdir>     # standalone, no lock
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import time

try:
    import psutil
except ImportError:
    psutil = None

SI = subprocess.STARTUPINFO()
SI.dwFlags |= subprocess.STARTF_USESHOWWINDOW
SI.wShowWindow = 7  # SW_SHOWMINNOACTIVE
NOCON = getattr(subprocess, "CREATE_NO_WINDOW", 0)


def md5(p):
    return hashlib.md5(open(p, "rb").read()).hexdigest() if os.path.exists(p) else "NO_BMP"


def sha(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest() if os.path.exists(p) else "NO_BMP"


def calm(p):
    """W43 / W47: IDLE priority and half (League) / a quarter (VRChat) of the
    cores only while one of them runs; otherwise the consoles keep the normal
    class like port/tools/mp_ladder.py's. (Always-idle starved the lockstep
    child for 1.2 s under another lane's 12-core build: the host dropped it at
    round 384, a load flake, not a game difference.)"""
    if psutil is None:
        return
    try:
        pp = psutil.Process(p.pid)
        out = subprocess.run(["tasklist"], capture_output=True, text=True,
                             creationflags=NOCON).stdout.lower()
        n = os.cpu_count() or 4
        if "vrchat.exe" in out:
            pp.nice(psutil.IDLE_PRIORITY_CLASS)
            pp.cpu_affinity(list(range(max(1, n // 4))))
        elif "league of legends.exe" in out:
            pp.nice(psutil.IDLE_PRIORITY_CLASS)
            pp.cpu_affinity(list(range(max(1, n // 2))))
    except Exception:
        pass


def start(exe, root, wdir, extra, tag):
    if os.path.isdir(wdir):
        shutil.rmtree(wdir, ignore_errors=True)
    os.makedirs(os.path.join(wdir, "tmp"), exist_ok=True)
    mine = os.path.join(wdir, "walk_window.exe")
    shutil.copyfile(exe, mine)
    os.chmod(mine, 0o666)
    env = {k: v for k, v in os.environ.items() if not k.startswith("SM64DS_")}
    env["TEMP"] = env["TMP"] = os.path.join(wdir, "tmp")
    env.update({"SM64DS_NO_FOCUS": "1", "SM64DS_VOLUME": "0", "SM64DS_MINIMIZED": "1",
                "SM64DS_NO_DIALOG": "1", "SM64DS_INSTANCE": "ac1_" + tag,
                "SM64DS_ASSET_ROOT": root, "SM64DS_FAULTS_FATAL": "1",
                "SM64DS_SAVE_PATH": os.path.join(wdir, "sm64ds.sav")})
    env.update({k: str(v) for k, v in extra.items()})
    fh = open(os.path.join(wdir, "out.txt"), "wb")
    p = subprocess.Popen([mine], cwd=wdir, env=env, startupinfo=SI, stdin=subprocess.DEVNULL,
                         creationflags=NOCON, stdout=fh, stderr=subprocess.STDOUT)
    calm(p)
    return p, fh


def finish(p, fh, wdir, timeout=600):
    try:
        rc = p.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        p.kill()
        p.wait()
        rc = "TIMEOUT"
    fh.close()
    return rc, open(os.path.join(wdir, "out.txt"), encoding="utf-8", errors="replace").read()


def run(exe, root, wdir, extra, tag, timeout=600):
    p, fh = start(exe, root, wdir, extra, tag)
    return finish(p, fh, wdir, timeout)


FAULT = re.compile(r"\bFAULT\b|c0000005|c0000409|\bFATAL\b")
VSL = re.compile(r"^\[vs\] f\d+ slot\d+ [^\n]*", re.M)


_MAPS = {}


def map_syms(exe):
    """host address -> symbol from the .map beside the exe (state= is the host
    address of a pl_state_face; it moves with any code layout change)."""
    mp = os.path.splitext(exe)[0] + ".map"
    if mp not in _MAPS:
        d = {}
        if os.path.exists(mp):
            rx = re.compile(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s", re.I)
            for l in open(mp, encoding="latin-1"):
                m = rx.match(l)
                if m:
                    d.setdefault(int(m.group(2), 16), re.sub(r"\?A0x[0-9a-f]+@", "?A@", m.group(1)))
        _MAPS[mp] = d
    return _MAPS[mp]


def vs_lines(t, exe=None):
    # actor addresses differ between runs: drop the actor= column; state= is a
    # host code address: name it through the exe's own map (anon-namespace hash dropped)
    syms = map_syms(exe) if exe else {}

    def st(m):
        a = int(m.group(1), 16)
        return "state=" + syms.get(a, m.group(1))
    return [re.sub(r"state=([0-9a-f]{8})", st, re.sub(r"actor=\S+ ", "", l))
            for l in VSL.findall(t)]


def run_rows(base, tip, root, out):
    """-> (lines, ok). The caller holds the ladder lock."""
    os.makedirs(out, exist_ok=True)
    lines = ["GATE base=%s tip=%s" % (md5(base)[:8], md5(tip)[:8])]
    ok_all = True
    try:
        rows = []
        for lvl in ("1", "6", "8"):
            for fr in ("600", "900"):
                rows.append(("ident_l%s_f%s" % (lvl, fr), {"SM64DS_LEVEL": lvl,
                             "SM64DS_WINDOW_SELFTEST": fr, "SM64DS_SELFTEST_IDLE": "1"}, "sha"))
        rows.append(("f3off", {"SM64DS_LEVEL": "1", "SM64DS_WINDOW_SELFTEST": "600"}, "md5"))
        rows.append(("rendereron", {"SM64DS_LEVEL": "1", "SM64DS_WINDOW_SELFTEST": "600",
                                    "SM64DS_RENDERER": "1"}, "md5"))
        for name, extra, kind in rows:
            got = {}
            for which, exe in (("base", base), ("tip", tip)):
                w = os.path.join(out, which, name)
                rc, t = run(exe, root, w, extra, which + name)
                bmp = os.path.join(w, "walk_window_selftest.bmp")
                got[which] = (rc, len(FAULT.findall(t)), sha(bmp) if kind == "sha" else md5(bmp))
            same = got["base"] == got["tip"]
            ok_all &= same and got["tip"][0] == 0
            lines.append("%-16s base rc=%s faults=%d %s=%s | tip rc=%s faults=%d %s=%s  %s" % (
                name, got["base"][0], got["base"][1], kind, got["base"][2][:12],
                got["tip"][0], got["tip"][1], kind, got["tip"][2][:12],
                "SAME" if same else "DIFFERENT"))
        for name, extra in (("traps_absent", {"SM64DS_SCENE": "1", "SM64DS_SCENE_FRAMES": "300"}),
                            ("traps_all", {"SM64DS_SCENE": "1", "SM64DS_SCENE_FRAMES": "300",
                                           "SM64DS_RENDER_SCALE": "4", "SM64DS_SMOOTH_MODELS": "2",
                                           "SM64DS_TEXTURE_FILTER": "2",
                                           "SM64DS_ANTI_ALIASING": "1", "SM64DS_RENDERER": "1",
                                           "SM64DS_PRESENT_BACKEND": "1"})):
            rc, t = run(tip, root, os.path.join(out, "tip", name), extra, name)
            # the scene prints one summary line either way:
            #   [scene] unmatched-body traps entered: N (...)
            # score the count (0 = pass); a missing summary is a fail (-1). The old
            # scorer counted the summary line itself as a hit and failed a clean run.
            m = re.search(r"unmatched-body traps entered: (\d+)", t)
            entered = int(m.group(1)) if m else -1
            ok_all &= rc == 0 and entered == 0
            lines.append("%-16s tip rc=%s faults=%d traps_entered=%d" % (name, rc,
                                                                      len(FAULT.findall(t)), entered))
        # solo VS, 4 bodies, no comms, a scripted walk: the arena code must not
        # change one byte of the [vs] probe (positions, pads, states) on any frame
        vs = {"SM64DS_VS_MAP": "0", "SM64DS_VS_PLAYERS": "4", "SM64DS_VS_PROBE": "1",
              "SM64DS_WINDOW_SELFTEST": "500",
              "SM64DS_PROBE_INPUT": "150-260:RIGHT,270:A,300-380:UP,400:A"}
        got = {}
        for which, exe in (("base", base), ("tip", tip)):
            w = os.path.join(out, which, "vssolo4")
            rc, t = run(exe, root, w, vs, which + "vssolo4")
            got[which] = (rc, len(FAULT.findall(t)), vs_lines(t, exe))
        same = got["base"][2] == got["tip"][2] and got["base"][2]
        ok_all &= bool(same) and got["tip"][0] == 0
        lines.append("vssolo4          base rc=%s lines=%d | tip rc=%s lines=%d  [vs] probe %s" % (
            got["base"][0], len(got["base"][2]), got["tip"][0], len(got["tip"][2]),
            "IDENTICAL" if same else "DIFFERENT"))
        # lockstep 2P loopback, unpaced, a scripted walk on the parent: both
        # consoles' [vs] probe identical base vs tip
        port = 40000 + (os.getpid() % 1000) * 8
        for which, exe in (("base", base), ("tip", tip)):
            ps = []
            for k in range(2):
                e = {"SM64DS_VS_MAP": "0", "SM64DS_VS_PLAYERS": "2", "SM64DS_VS_PROBE": "1",
                     "SM64DS_COMMS_ROLE": "parent" if k == 0 else "child",
                     "SM64DS_COMMS_PORT": str(port), "SM64DS_COMMS_FANOUT": "1",
                     "SM64DS_WINDOW_SELFTEST": str(420 + (90 if k == 0 else 0)),
                     "SM64DS_PROBE_INPUT": "150-260:RIGHT,270:A,300-380:UP" if k == 0 else
                                           "160-250:LEFT,280:A"}
                if k:
                    e["SM64DS_COMMS_SLOT"] = "1"
                w = os.path.join(out, which, "lock2_p%d" % k)
                ps.append((start(exe, root, w, e, which + "lock2p%d" % k), w))
                time.sleep(0.5)
            res = [finish(p, fh, w) for (p, fh), w in ps]
            got[which] = [(rc, len(FAULT.findall(t)), vs_lines(t, exe)) for rc, t in res]
            port += 4
        # lockstep is a replica pair: on each exe both consoles must print the
        # same [vs] probe frame for frame (the frames both reached), rc 0
        for which in ("base", "tip"):
            (r0, f0, l0), (r1, f1, l1) = got[which]
            n = min(len(l0), len(l1))
            same = n > 0 and l0[:n] == l1[:n]
            if which == "tip":
                ok_all &= same and r0 == 0 and r1 == 0 and f0 == 0 and f1 == 0
            lines.append("lock2 %-4s       p0 rc=%s lines=%d p1 rc=%s lines=%d faults=%d/%d  p0 == p1 over %d lines: %s" % (
                which, r0, len(l0), r1, len(l1), f0, f1, n, "IDENTICAL" if same else "DIFFERENT"))
    finally:
        pass
    lines.append("GATE %s" % ("PASS" if ok_all else "FAIL"))
    with open(os.path.join(out, "gate.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    return lines, ok_all


if __name__ == "__main__":
    b, t, r, o = sys.argv[1:5]
    ls, okk = run_rows(b, t, r, o)
    print("\n".join(ls))
    sys.exit(0 if okk else 1)
