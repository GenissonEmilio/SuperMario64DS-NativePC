#!/usr/bin/env python3
"""portsync build.py -- build the port through the PC's STAGED build queue.

Same shape as the lanes' stage.py (the PC's build queue), so a build started
here queues FAIRLY with every other build on the machine: the same pools (same
folder, same file names), the same build_lock.py machinery (taken from this
tree's port/tools/build_lock.py, which stage.py vendors unchanged).

    python port/tools/portsync/build.py [--config C] [--label L]

  1. takes a TOTAL slot (pool "total", SM64DS_STAGE_TOTAL, default 3);
  2. writes port/build-port-staged.cmd next to the build script: the same
     script line for line, except that the one full "ninja -C build\\port"
     line becomes "build.py heavy ... -- ninja -C build\\port", plus a few
     "build.py mark" lines that time the phases;
  3. "heavy" asks ninja how much is dirty, then takes a COMPILE token: pool
     "heavy" (SM64DS_STAGE_HEAVY, default 1, -j SM64DS_STAGE_HEAVY_J default
     10) or, for at most SM64DS_STAGE_SMALL_EDGES (600) edges, pool "small"
     (SM64DS_STAGE_SMALL 2, -j SM64DS_STAGE_SMALL_J 6). The token goes back at
     the first "Linking CXX executable";
  4. chill rule: VRChat.exe running -> IDLE priority -j3; League -> IDLE -j6;
  5. after the build: md5 of build/port/walk_window.exe and .map, and a copy
     of both into <work_dir>/exes/<label>/ (later steps run only that copy).

If build.external_stage_py is set in the config, the build is handed to that
stage.py instead (`stage.py run --label L --root R -- cmd /c <script>`).

Other commands:  build.py heavy --root R -- ninja <args>   (used by the staged script)
                 build.py mark NAME | status
"""
import argparse
import contextlib
import ctypes
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import portsync_common as C  # noqa: E402

POLL = 1.0
NOTICE = 60
_bl = None


def bl():
    """This tree's port/tools/build_lock.py (stage.py's vendored copy is the
    same file, unchanged)."""
    global _bl
    if _bl is None:
        _bl = C.load_tree_module("tree_build_lock", "build_lock.py")
        if _bl is None:
            raise RuntimeError("port/tools/build_lock.py not found in this tree")
    return _bl


def env_int(name, default):
    try:
        return int(os.environ.get(name, default))
    except ValueError:
        return default


def pool_dir():
    """Exactly stage.py's rule, so both queues share the same pools."""
    base = os.environ.get("SM64DS_STAGE_DIR")
    if base:
        return base
    p = os.environ.get("SM64DS_BUILD_LOCK_PATH")
    root = os.path.dirname(p) if p else os.path.join(tempfile.gettempdir(), "sm64ds-test-slot")
    return os.path.join(root, "stage")


def log(msg):
    label = os.environ.get("SM64DS_STAGE_LABEL", "")
    print("[stage] %s %s %s" % (time.strftime("%H:%M:%S"), label, msg), flush=True)


def avail_mb():
    if not C.IS_WINDOWS:
        return -1

    class MS(ctypes.Structure):
        _fields_ = [("l", ctypes.c_ulong), ("load", ctypes.c_ulong),
                    ("tp", ctypes.c_ulonglong), ("ap", ctypes.c_ulonglong),
                    ("tpf", ctypes.c_ulonglong), ("apf", ctypes.c_ulonglong),
                    ("tv", ctypes.c_ulonglong), ("av", ctypes.c_ulonglong),
                    ("ae", ctypes.c_ulonglong)]
    m = MS()
    m.l = ctypes.sizeof(MS)
    try:
        ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(m))
        return int(m.ap / 1048576)
    except Exception:
        return -1


# ---------------------------------------------------------------- the pools

def slot_path(name, i):
    return os.path.join(pool_dir(), "%s.slot%d.lock" % (name, i))


def queue_path(name):
    # never created as a file: only its derived <name>.tickets directory is used
    return os.path.join(pool_dir(), "%s.lock" % name)


def pool_acquire(name, size, label, root, timeout=None, poll=POLL):
    """FIFO counting semaphore of `size` slots. Returns the slot path held."""
    b = bl()
    if timeout is None:
        timeout = b.acquire_timeout()
    qp = queue_path(name)
    started = time.time()
    deadline = started + timeout
    next_notice = started + NOTICE
    ticket = None
    ticketless = False
    try:
        while True:
            live = b._live_queue(qp)
            head = live[0] if live else None
            our_turn = (head is None or ticketless or
                        (ticket is not None and head["name"] == ticket["name"]))
            if our_turn:
                for i in range(size):
                    sp = slot_path(name, i)
                    if b._try_create(sp, label, root):
                        return sp
                    if b._break_stale(sp) and b._try_create(sp, label, root):
                        return sp
            else:
                for i in range(size):
                    b._break_stale(slot_path(name, i))
            now = time.time()
            if now >= deadline:
                raise b.BuildLockTimeout(
                    "stage pool '%s' (size %d) not acquired within %.0fs; holders: %s"
                    % (name, size, timeout, "; ".join(b._describe_holder(slot_path(name, i))
                                                      for i in range(size))))
            ticket = b._ensure_ticket(ticket, qp, label, root)
            ticketless = ticket is None
            if now >= next_notice:
                pos = b._queue_position(live, ticket)
                log("waiting %.0fs for pool '%s'%s" % (
                    now - started, name, (", queue position %s of %d" % (pos, len(live))) if pos else ""))
                next_notice = now + NOTICE
            time.sleep(poll)
    finally:
        b._remove_ticket(ticket)


def pool_release(sp):
    if sp:
        bl().release(sp)


def pool_state(name, size):
    b = bl()
    held = [b._describe_holder(slot_path(name, i)) for i in range(size)
            if os.path.exists(slot_path(name, i))]
    return held, b.queue(queue_path(name))


# ----------------------------------------------------------- phase marks

def mark(name):
    mf = os.environ.get("SM64DS_STAGE_MARKS", "")
    if mf:
        try:
            with open(mf, "a", encoding="ascii") as f:
                f.write(json.dumps({"t": time.time(), "m": name, "avail": avail_mb()}) + "\n")
        except OSError:
            pass
    log("mark %s avail=%sMB" % (name, avail_mb()))


# ------------------------------------------------------- script rewriting

NINJA_FULL = re.compile(r'^(\s*)ninja(?:\s+-j\d+)?\s+-C\s+("%~dp0\.\.\\build\\port")\s*$', re.I)
NINJA_REGEN = re.compile(r'^\s*ninja(?:\s+-j\d+)?\s+-C\s+"%~dp0\.\.\\build\\port"\s+build\.ninja\s*$', re.I)
MARKS = [
    ("wave", re.compile(r'^python\s+"%~dp0tools\\guardcache\.py"\s+--prime', re.I)),
    ("serial_guards", re.compile(r'--replay\s+closestplayer_guard\.py', re.I)),
    ("configure", re.compile(r'^set\s+"PORT_NEED_CONFIGURE="', re.I)),
    ("regen", NINJA_REGEN),
    ("gxport", re.compile(r'gxport_guard\.py', re.I)),
    ("postlink", re.compile(r'tools\\pmf_guard\.py', re.I)),
]


def stage_script(src, dst, me=None, py=None):
    """Write dst = src with marks and the heavy wrapper. Returns (heavy, marks)."""
    me = me or os.path.abspath(__file__)
    py = py or sys.executable
    mk = '"%s" "%s" mark ' % (py, me)
    out = []
    seen = {}
    heavy = False
    with open(src, "rb") as f:
        raw = f.read().decode("ascii", "replace")
    nl = "\r\n" if "\r" in raw else "\n"
    for ln in raw.split(nl):
        s = ln.strip()
        if s.lower().startswith("rem"):
            out.append(ln)
            continue
        m = NINJA_FULL.match(ln)
        if m and not NINJA_REGEN.match(ln):
            out.append('"%s" "%s" heavy --root "%%~dp0.." -- ninja -C %s' % (py, me, m.group(2)))
            heavy = True
            continue
        for name, rx in MARKS:
            if name not in seen and rx.search(s):
                seen[name] = 1
                out.append(mk + name)
                break
        out.append(ln)
    with open(dst, "w", encoding="ascii", errors="replace", newline="") as f:
        f.write(nl.join(out))
    return heavy, sorted(seen)


# ------------------------------------------------------------- the heavy step

def running(name):
    if not C.IS_WINDOWS:
        return False
    return any(img.lower() == name.lower() for img, _ in C.list_processes())


def find_ninja():
    w = shutil.which("ninja")
    if w:
        return w
    return os.path.join(os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"),
                        "Microsoft Visual Studio", "2022", "BuildTools", "Common7", "IDE",
                        "CommonExtensions", "Microsoft", "CMake", "Ninja", "ninja.exe")


def probe_ninja_text(text):
    """build.ninja without the always-dirty RERUN_CMAKE / verify_globs edges
    (stage.py's count_edges trick: a plain dry run stops after "2 edges")."""
    rx = re.compile(r"build (build\.ninja:|[^:]*cmake\.verify_globs:)")
    out, skip = [], False
    for ln in text.split("\n"):
        if ln.startswith("build "):
            skip = bool(rx.match(ln.replace("$:", "")))
        elif skip and not ln.startswith(" "):
            skip = False
        if not skip:
            out.append(ln)
    return "\n".join(out)


def parse_dry_run(stdout):
    if "no work to do" in stdout:
        return 0
    m = re.search(r"^\[\d+/(\d+)\]", stdout, re.M)
    return int(m.group(1)) if m else None


def count_edges(ninja_args):
    bd = None
    for i, a in enumerate(ninja_args):
        if a == "-C" and i + 1 < len(ninja_args):
            bd = ninja_args[i + 1]
    if not bd:
        return None
    p = os.path.join(bd, "build_probe.ninja")
    try:
        with open(os.path.join(bd, "build.ninja"), encoding="utf-8", errors="replace") as f:
            text = probe_ninja_text(f.read())
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        r = subprocess.run([find_ninja(), "-n", "-f", "build_probe.ninja", "-C", bd],
                           capture_output=True, text=True, timeout=300)
    except Exception:
        return None
    finally:
        with contextlib.suppress(OSError):
            os.remove(p)
    return parse_dry_run(r.stdout)


def kill_tree(pid):
    if C.IS_WINDOWS:
        subprocess.run(["taskkill", "/T", "/F", "/PID", str(pid)], capture_output=True)


def _install_signals(handler):
    old = {}
    for n in ("SIGINT", "SIGTERM", "SIGBREAK"):
        s = getattr(signal, n, None)
        if s is not None:
            try:
                old[s] = signal.signal(s, handler)
            except (ValueError, OSError):
                pass
    return old


def cmd_heavy(argv):
    if "--" not in argv:
        print("usage: build.py heavy [--root R] -- ninja <args>", file=sys.stderr)
        return 2
    head, child = argv[:argv.index("--")], argv[argv.index("--") + 1:]
    root = head[head.index("--root") + 1] if "--root" in head else ""
    label = os.environ.get("SM64DS_STAGE_LABEL", "heavy")
    # bring build.ninja up to date BEFORE asking what is dirty and before taking
    # a compile token (a RERUN_CMAKE inside the compile would sit on the token)
    if "-C" in child:
        bd = child[child.index("-C") + 1]
        subprocess.run([find_ninja(), "-C", bd, "build.ninja"])
        mark("regen2")
    edges = count_edges(child[1:])
    is_small = edges is not None and edges <= env_int("SM64DS_STAGE_SMALL_EDGES", 600)
    if is_small:
        pool, size, j = "small", env_int("SM64DS_STAGE_SMALL", 2), env_int("SM64DS_STAGE_SMALL_J", 6)
    else:
        pool, size, j = "heavy", env_int("SM64DS_STAGE_HEAVY", 1), env_int("SM64DS_STAGE_HEAVY_J", 10)
    prio = 0
    if running("VRChat.exe"):
        j, prio = min(j, 3), C.IDLE_PRIORITY_CLASS
    elif running("League of Legends.exe"):
        j, prio = min(j, 6), C.IDLE_PRIORITY_CLASS
    mark("compile_wait")
    log("compile class=%s edges=%s -j%d%s" % (pool, edges, j, " IDLE(chill)" if prio else ""))
    state = {"proc": None}

    def _bail(signum, _f):
        p = state["proc"]
        if p is not None and p.poll() is None:
            kill_tree(p.pid)
        raise KeyboardInterrupt("stage heavy: signal %s" % signum)

    _install_signals(_bail)
    sp = None
    t_wait = time.time()
    try:
        sp = pool_acquire(pool, size, label, root)
        mark("compile_start")
        log("compile token '%s' after %.0fs wait, avail=%sMB" % (pool, time.time() - t_wait, avail_mb()))
        exe = child[0] if child[0].lower() != "ninja" else find_ninja()
        proc = subprocess.Popen([exe, "-j%d" % j] + child[1:], stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, creationflags=prio)
        state["proc"] = proc
        released = False
        for raw in proc.stdout:
            sys.stdout.buffer.write(raw)
            sys.stdout.buffer.flush()
            if not released and b"Linking CXX executable" in raw:
                pool_release(sp)
                sp = None
                released = True
                mark("compile_done_token_released")
        rc = proc.wait()
        mark("ninja_end")
        return rc
    except bl().BuildLockTimeout as e:
        print("stage: TIMEOUT -- %s" % e, file=sys.stderr)
        return 1
    except KeyboardInterrupt as e:
        print("stage: interrupted (%s)" % e, file=sys.stderr)
        return 130
    finally:
        pool_release(sp)


# ------------------------------------------------------------------- run

def summarize(path, t_total_got):
    if not path or not os.path.exists(path):
        return
    rows = []
    with open(path, encoding="ascii", errors="replace") as f:
        for ln in f:
            try:
                rows.append(json.loads(ln))
            except ValueError:
                pass
    if not rows:
        return
    parts = []
    prev_t, prev_m = t_total_got, "start"
    for r in rows:
        parts.append("%s->%s %.0fs" % (prev_m, r["m"], r["t"] - prev_t))
        prev_t, prev_m = r["t"], r["m"]
    parts.append("%s->exit %.0fs" % (prev_m, time.time() - prev_t))
    log("PHASES " + "; ".join(parts))
    try:
        rec = {"t": time.strftime("%Y-%m-%dT%H:%M:%S"),
               "label": os.environ.get("SM64DS_STAGE_LABEL", ""),
               "phases": [p.split(" ") for p in parts],
               "min_avail_mb": min([r.get("avail", 0) for r in rows] or [0])}
        with open(os.path.join(pool_dir(), "history.jsonl"), "a", encoding="ascii") as f:
            f.write(json.dumps(rec) + "\n")
    except OSError:
        pass


def staged_run(label, root, child, script, timeout):
    """stage.py's `run`: total slot, staged script, heavy wrapper. rc."""
    os.environ["SM64DS_STAGE_LABEL"] = label
    t_arrive = time.time()
    sp_abs = os.path.abspath(os.path.join(root, script.replace("\\", os.sep)))
    staged = os.path.join(os.path.dirname(sp_abs), "build-port-staged.cmd")
    tot = hold_heavy = mf = None
    state = {"proc": None}

    def _bail(signum, _f):
        p = state["proc"]
        if p is not None and p.poll() is None:
            kill_tree(p.pid)
        raise KeyboardInterrupt("signal %s" % signum)

    old = _install_signals(_bail)
    b = bl()
    try:
        T = env_int("SM64DS_STAGE_TOTAL", 3)
        log("pools in %s  total=%d heavy=%d small=%d" % (
            pool_dir(), T, env_int("SM64DS_STAGE_HEAVY", 1), env_int("SM64DS_STAGE_SMALL", 2)))
        tot = pool_acquire("total", T, label, root, timeout)
        t_got = time.time()
        log("total slot after %.0fs wait, avail=%sMB" % (t_got - t_arrive, avail_mb()))
        floor = env_int("SM64DS_STAGE_MIN_AVAIL_MB", 800)
        waited = 0
        while 0 <= avail_mb() < floor and waited < 600:
            time.sleep(10)
            waited += 10
        if waited:
            log("memory gate waited %ds" % waited)
        env = dict(os.environ)
        heavy_ok, seen = stage_script(sp_abs, staged)
        log("staged script %s marks=%s heavy_wrapped=%s" % (staged, seen, heavy_ok))
        run_child = [(staged if a == script else a) for a in child] if heavy_ok else child
        if not heavy_ok:
            log("NO full-ninja line found: the whole script runs under the compile token")
            hold_heavy = pool_acquire("heavy", env_int("SM64DS_STAGE_HEAVY", 1), label, root, timeout)
        fd, mf = tempfile.mkstemp(prefix="stage_marks_", suffix=".jsonl")
        os.close(fd)
        env["SM64DS_STAGE_MARKS"] = mf
        proc = subprocess.Popen(run_child, env=env, cwd=root)
        state["proc"] = proc
        rc = proc.wait()
        log("child exit %s, total %.0fs since arrival (%.0fs queued)" % (
            rc, time.time() - t_arrive, t_got - t_arrive))
        summarize(mf, t_got)
        return rc
    except b.BuildLockTimeout as e:
        print("build: TIMEOUT -- %s" % e, file=sys.stderr)
        return 1
    except KeyboardInterrupt as e:
        print("build: interrupted (%s); slots released" % e, file=sys.stderr)
        return 130
    finally:
        pool_release(hold_heavy)
        pool_release(tot)
        for s, o in old.items():
            with contextlib.suppress(ValueError, OSError):
                signal.signal(s, o)
        if os.path.exists(staged):
            with contextlib.suppress(OSError):
                os.remove(staged)
        if mf:
            with contextlib.suppress(OSError):
                os.remove(mf)


def run_build(cfg, label):
    """The portsync build step. Returns StepResult (data: exe, map, md5s)."""
    root = cfg["tree_root"]
    bc = cfg["build"]
    script = bc["script"]
    if not os.path.exists(os.path.join(root, script.replace("\\", os.sep))):
        return C.StepResult("build", C.ERROR, "build script %s is not in the tree %s"
                            % (script, root), ["W42 trap (a): copy build-port-k0.cmd into port/ (untracked)."])
    gb = C.free_gb(root)
    if 0 <= gb < float(bc.get("min_free_gb") or 0):
        return C.StepResult("build", C.ERROR, "only %.1f GB free on the tree's drive" % gb)
    tmp = os.path.join(cfg["work_dir"], "tmpbuild")
    os.makedirs(tmp, exist_ok=True)
    logs = os.path.join(cfg["work_dir"], "logs")
    os.makedirs(logs, exist_ok=True)
    logp = os.path.join(logs, "build_%s.log" % label)
    env = dict(os.environ)
    env["TEMP"] = env["TMP"] = tmp
    env["SM64DS_BUILD_LOCK_PATH"] = bc["lock_path"]
    env["SM64DS_BUILD_LOCK_TIMEOUT"] = str(int(bc.get("lock_timeout_seconds") or 14400))
    if bc.get("stage_dir"):
        env["SM64DS_STAGE_DIR"] = bc["stage_dir"]
    child = ["cmd", "/c", script]
    t0 = time.time()
    C.log("build: label %s, tree %s, log %s" % (label, root, logp))
    with open(logp, "w", encoding="ascii", errors="replace") as lf:
        if bc.get("external_stage_py"):
            cmd = [cfg["python"], bc["external_stage_py"], "run", "--label", "portsync-" + label,
                   "--root", root, "--"] + child
            rc = subprocess.call(cmd, cwd=root, env=env, stdout=lf, stderr=subprocess.STDOUT)
        else:
            cmd = [cfg["python"], os.path.abspath(__file__), "_staged", "--label", "portsync-" + label,
                   "--root", root, "--timeout", env["SM64DS_BUILD_LOCK_TIMEOUT"],
                   "--script", script, "--"] + child
            rc = subprocess.call(cmd, cwd=root, env=env, stdout=lf, stderr=subprocess.STDOUT)
        lf.write("BUILD_WRAPPER_EXIT=%s\n" % rc)
    exe = os.path.join(root, "build", "port", "walk_window.exe")
    mapf = os.path.join(root, "build", "port", "walk_window.map")
    mins = (time.time() - t0) / 60.0
    if rc != 0 or not os.path.exists(exe) or not os.path.exists(mapf):
        tail = []
        with open(logp, encoding="ascii", errors="replace") as f:
            tail = [l.rstrip() for l in f.readlines()[-25:]]
        return C.StepResult("build", C.FAIL, "build exit %s after %.0f min (log %s)" % (rc, mins, logp),
                            tail)
    if os.path.getmtime(exe) < t0 - 5:
        note = "exe not rewritten by this build (nothing to do?)"
    else:
        note = ""
    dst = os.path.join(cfg["work_dir"], "exes", label)
    C.rmtree(dst)
    os.makedirs(dst)
    shutil.copyfile(exe, os.path.join(dst, "walk_window.exe"))
    shutil.copyfile(mapf, os.path.join(dst, "walk_window.map"))
    e5, m5 = C.md5_file(os.path.join(dst, "walk_window.exe")), C.md5_file(os.path.join(dst, "walk_window.map"))
    with open(logp, "a", encoding="ascii") as lf:
        lf.write("%s  walk_window.exe\n%s  walk_window.map\nBUILDDONE %s\n" % (e5, m5, label))
    info = {"label": label, "exe": os.path.join(dst, "walk_window.exe"),
            "map": os.path.join(dst, "walk_window.map"), "exe_md5": e5, "map_md5": m5,
            "minutes": round(mins, 1)}
    with open(os.path.join(dst, "build.json"), "w", encoding="ascii") as f:
        json.dump(info, f, indent=1)
    return C.StepResult("build", C.PASS, "exe md5 %s map md5 %s (%.0f min)%s" % (
        e5[:8], m5[:8], mins, ("; " + note) if note else ""), data=info)


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv and argv[0] == "heavy":
        return cmd_heavy(argv[1:])
    if argv and argv[0] == "mark":
        mark(argv[1] if len(argv) > 1 else "mark")
        return 0
    if argv and argv[0] == "_staged":
        rest = argv[1:]
        head, child = rest[:rest.index("--")], rest[rest.index("--") + 1:]

        def opt(n, d=None):
            return head[head.index(n) + 1] if n in head else d
        return staged_run(opt("--label", "portsync"), opt("--root", os.getcwd()), child,
                          opt("--script"), float(opt("--timeout", 14400)))
    if argv and argv[0] == "status":
        for name, size in (("total", env_int("SM64DS_STAGE_TOTAL", 3)),
                           ("heavy", env_int("SM64DS_STAGE_HEAVY", 1)),
                           ("small", env_int("SM64DS_STAGE_SMALL", 2))):
            held, q = pool_state(name, size)
            print("pool %s size %d: %d held, %d waiting" % (name, size, len(held), len(q)))
            for h in held:
                print("   HOLD", h)
        return 0
    ap = C.common_args(argparse.ArgumentParser(description="portsync: staged port build"))
    ap.add_argument("--label", default=time.strftime("b%Y%m%d_%H%M"))
    a = ap.parse_args(argv)
    r = C.handle_write_config(a)
    if r is not None:
        return r
    try:
        cfg = C.load_config(a.config, require=("work_dir", "build.lock_path"))
    except C.ConfigError as e:
        print("build: %s" % e)
        return 2
    C.open_log(cfg, "build")
    res = run_build(cfg, a.label)
    print(C.table([[res.step, res.status, res.summary]], ["STEP", "RESULT", "WHAT"]))
    for d in res.details:
        print("   " + d)
    return 0 if res.status == C.PASS else 1


if __name__ == "__main__":
    sys.exit(main())
