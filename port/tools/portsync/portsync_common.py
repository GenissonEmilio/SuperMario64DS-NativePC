"""Shared code for the portsync scripts (build.py, linkage.py, gate.py,
compare.py, run_all.py). Python 3 standard library only; Windows is the target,
but everything here imports on Linux so the selftest can run anywhere.

What lives here:
  CONFIG     the ONE machine config file (portsync_config.json next to these
             scripts, or --config PATH, or the PORTSYNC_CONFIG variable).
             `--write-config` on any script scaffolds it with placeholders.
             No machine path is baked into any script.
  ETIQUETTE  every game run is quiet: a fresh copy of the exe in its own row
             folder, minimized and never activated (STARTUPINFO
             SW_SHOWMINNOACTIVE + CREATE_NO_WINDOW + SM64DS_NO_FOCUS=1 +
             SM64DS_MINIMIZED=1), muted (SM64DS_VOLUME=0), no dialogs
             (SM64DS_NO_DIALOG=1), every inherited SM64DS_* dropped, a private
             TEMP. The recipe is the starter kit's v1_q.py / pe2_q.py. Only the
             pid we started is ever killed (taskkill /T /PID <our pid>, never
             /IM). Before every launch we wait while a walk_window.exe or
             SM64DSLauncher.exe that we did NOT start is running (someone may
             be playing). If VRChat or a League match runs, the game runs at
             IDLE priority (W42 chill rule).
  PARSERS    MSVC /MAP rows, PE32 imports / image base / size, hash helpers.
  TABLE      the plain PASS / FAIL table every script prints.
"""
import bisect
import contextlib
import copy
import csv
import hashlib
import importlib.util
import io
import json
import os
import re
import shutil
import stat
import struct
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)                       # <tree>/port/tools
THIS_TREE = os.path.dirname(os.path.dirname(TOOLS))  # <tree>
DEFAULT_CONFIG = os.path.join(HERE, "portsync_config.json")
CONFIG_ENV = "PORTSYNC_CONFIG"

IS_WINDOWS = os.name == "nt"
SW_SHOWMINNOACTIVE = 7
CREATE_NO_WINDOW = 0x08000000
IDLE_PRIORITY_CLASS = 0x40

# The two settings modes of the P120 GATE NOTE. "off" = the keys written false
# (matches the old references); "on" = no settings.json at all (absent keys =
# new menus + N64 HUD, the new defaults).
SETTINGS_MODES = {
    "off": {"OneScreenMenus": False, "HudLayout": False},
    "on": None,
}

PASS, FAIL, WARN, SKIP, ERROR, RECORD, NOTRUN = (
    "PASS", "FAIL", "WARN", "SKIP", "ERROR", "RECORD", "NOT RUN")
HARD = (FAIL, ERROR)


# ------------------------------------------------------------------ config

PLACEHOLDER = re.compile(r"^<.*>$", re.S)

CONFIG_TEMPLATE = {
    "_help": [
        "portsync config. Fill in every value that is still in <angle brackets>.",
        "Paths may be absolute or relative to this file. Use forward slashes or",
        "doubled backslashes. An empty string means 'use the default'.",
        "tree_root empty = the checkout these scripts live in.",
        "asset_root empty = tree_root (it must hold the NitroFS index:",
        "run `python tools/asset_catalog.py generate <ROM>` there once).",
        "reference_map empty = the .map that sits next to old_exe.",
    ],
    "tree_root": "",
    "work_dir": "<FOLDER FOR RUNS, LOGS AND EXE COPIES (a big drive is best)>",
    "old_exe": "<FULL PATH TO THE OLD walk_window.exe; its .map must sit next to it>",
    "reference_map": "",
    "asset_root": "",
    "python": "",
    "build": {
        "script": "port\\build-port-k0.cmd",
        "lock_path": "<FULL PATH OF THE MACHINE BUILD LOCK FILE (the same file every build on this PC uses), e.g. X:/tmp/sm64ds-test-slot/port_build.lock>",
        "lock_timeout_seconds": 14400,
        "stage_dir": "",
        "external_stage_py": "",
        "min_free_gb": 5,
    },
    "runs": {
        "max_parallel": 2,
        "timeout_seconds": 2400,
        "foreign_processes": ["walk_window.exe", "SM64DSLauncher.exe"],
        "foreign_poll_seconds": 30,
        "foreign_max_wait_seconds": 0,
        "chill_processes": ["VRChat.exe", "League of Legends.exe"],
        "test_lock_path": "",
        "keep_row_dirs": False,
    },
    "gate": {
        "modes": ["off", "on"],
        "parts": ["ident", "traps", "irq", "ab", "twod", "opening", "twostar", "ending"],
        "references": {},
    },
    "compare": {
        "settings_mode": "off",
        "settings_extra": {},
        "noise_runs": 2,
        "new_noise": True,
        "drill_max": 3,
        "max_unstable_fraction": 0.5,
        "mask_host_pointers": False,
        "routes": {},
    },
}

# keys whose values are paths (resolved against the config file's folder)
PATH_KEYS = [("tree_root",), ("work_dir",), ("old_exe",), ("reference_map",),
             ("asset_root",), ("build", "lock_path"), ("build", "stage_dir"),
             ("build", "external_stage_py"), ("runs", "test_lock_path")]


class ConfigError(Exception):
    pass


def config_path(arg=None):
    return os.path.abspath(arg or os.environ.get(CONFIG_ENV) or DEFAULT_CONFIG)


def write_config(path, force=False):
    """Scaffold the config with placeholders. Refuses to overwrite."""
    if os.path.exists(path) and not force:
        raise ConfigError("%s already exists (use --force to overwrite)" % path)
    d = os.path.dirname(os.path.abspath(path))
    if d:
        os.makedirs(d, exist_ok=True)
    with open(path, "w", encoding="ascii", newline="\n") as f:
        json.dump(CONFIG_TEMPLATE, f, indent=2, sort_keys=False)
        f.write("\n")
    return path


def _merge(base, over):
    out = copy.deepcopy(base)
    for k, v in (over or {}).items():
        if isinstance(v, dict) and isinstance(out.get(k), dict):
            out[k] = _merge(out[k], v)
        else:
            out[k] = v
    return out


def _get(cfg, keys):
    v = cfg
    for k in keys:
        v = v.get(k) if isinstance(v, dict) else None
    return v


def _set(cfg, keys, value):
    d = cfg
    for k in keys[:-1]:
        d = d.setdefault(k, {})
    d[keys[-1]] = value


def placeholders(cfg, prefix=""):
    """Every key still holding an <angle bracket> value."""
    out = []
    for k, v in cfg.items():
        if k.startswith("_"):
            continue
        name = prefix + k
        if isinstance(v, dict):
            out += placeholders(v, name + ".")
        elif isinstance(v, str) and PLACEHOLDER.match(v.strip()):
            out.append(name)
    return out


def load_config(path=None, require=()):
    """Load + default-merge + resolve paths. `require` names dotted keys that
    must be filled in for the calling step (e.g. "old_exe", "build.lock_path")."""
    path = config_path(path)
    if not os.path.exists(path):
        raise ConfigError("no config at %s. Make one with:  python %s --write-config"
                          % (path, os.path.join("port", "tools", "portsync", "run_all.py")))
    try:
        with open(path, encoding="utf-8") as f:
            user = json.load(f)
    except ValueError as e:
        raise ConfigError("%s is not valid JSON: %s" % (path, e))
    cfg = _merge(CONFIG_TEMPLATE, user)
    base = os.path.dirname(path)
    for keys in PATH_KEYS:
        v = _get(cfg, keys)
        if isinstance(v, str) and v and not PLACEHOLDER.match(v.strip()):
            v = os.path.expandvars(os.path.expanduser(v))
            if not os.path.isabs(v):
                v = os.path.join(base, v)
            _set(cfg, keys, os.path.normpath(v))
    if not cfg["tree_root"]:
        cfg["tree_root"] = THIS_TREE
    if not cfg["asset_root"]:
        cfg["asset_root"] = cfg["tree_root"]
    if not cfg["python"]:
        cfg["python"] = sys.executable
    cfg["_path"] = path
    missing = [k for k in placeholders(cfg) if k in require or
               any(k.startswith(r + ".") for r in require)]
    if missing:
        raise ConfigError("fill in these values in %s first: %s" % (path, ", ".join(missing)))
    return cfg


def reference_map(cfg):
    if cfg.get("reference_map"):
        return cfg["reference_map"]
    return os.path.splitext(cfg["old_exe"])[0] + ".map"


def common_args(ap):
    ap.add_argument("--config", help="config file (default: portsync_config.json next to the scripts)")
    ap.add_argument("--write-config", action="store_true", help="scaffold the config with placeholders and stop")
    ap.add_argument("--force", action="store_true", help="with --write-config: overwrite")
    return ap


def handle_write_config(a):
    if not a.write_config:
        return None
    p = config_path(a.config)
    try:
        write_config(p, a.force)
    except ConfigError as e:
        print("portsync: %s" % e)
        return 2
    print("portsync: wrote %s -- open it and fill in every <...> value" % p)
    return 0


# ---------------------------------------------------------------- logging

_log_lock = threading.Lock()
_log_file = None


def open_log(cfg, step):
    """Mirror log() into <work_dir>/logs/<step>_<stamp>.txt."""
    global _log_file
    d = os.path.join(cfg["work_dir"], "logs")
    os.makedirs(d, exist_ok=True)
    p = os.path.join(d, "%s_%s.txt" % (step, time.strftime("%Y%m%d_%H%M%S")))
    _log_file = open(p, "a", encoding="ascii", errors="replace")
    return p


def log(msg=""):
    line = "[%s] %s" % (time.strftime("%H:%M:%S"), msg) if msg else ""
    with _log_lock:
        print(line, flush=True)
        if _log_file:
            _log_file.write(line + "\n")
            _log_file.flush()


# ---------------------------------------------------------------- results

class StepResult(object):
    """One step's verdict. status: PASS / FAIL / WARN / SKIP / ERROR."""

    def __init__(self, step, status, summary, details=None, data=None):
        self.step, self.status, self.summary = step, status, summary
        self.details = list(details or [])
        self.data = dict(data or {})

    @property
    def hard(self):
        return self.status in HARD

    def __repr__(self):
        return "StepResult(%r, %r, %r)" % (self.step, self.status, self.summary)


def table(rows, headers):
    """Plain ASCII table (list of lists of str)."""
    rows = [[str(c) for c in r] for r in rows]
    w = [len(h) for h in headers]
    for r in rows:
        for i, c in enumerate(r):
            w[i] = max(w[i], len(c))
    sep = "+-" + "-+-".join("-" * x for x in w) + "-+"
    out = [sep, "| " + " | ".join(h.ljust(w[i]) for i, h in enumerate(headers)) + " |", sep]
    for r in rows:
        out.append("| " + " | ".join(c.ljust(w[i]) for i, c in enumerate(r)) + " |")
    out.append(sep)
    return "\n".join(out)


def asciify(s):
    return s.encode("ascii", "replace").decode("ascii")


# ----------------------------------------------------------------- hashes

def md5_file(p):
    if not os.path.exists(p):
        return "NO_FILE"
    h = hashlib.md5()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def sha256_file(p):
    if not os.path.exists(p):
        return "NO_FILE"
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def exe_supports(exe, names):
    """Which of these env knob names are NOT spelled in the exe (getenv strings
    are literals in the image). Returns the missing ones."""
    try:
        with open(exe, "rb") as f:
            blob = f.read()
    except OSError:
        return list(names)
    return [n for n in names if n.encode("ascii") + b"\0" not in blob]


def free_gb(path):
    try:
        return shutil.disk_usage(path).free / float(1 << 30)
    except OSError:
        return -1.0


# --------------------------------------------------------- load tree tools

def load_tree_module(name, filename, tree=None):
    """Import port/tools/<filename> of a tree under a private module name (so
    our own linkage.py / gate.py never shadow the tree's)."""
    tools = os.path.join(tree or THIS_TREE, "port", "tools")
    if tools not in sys.path:
        sys.path.append(tools)     # appended: our own folder stays first
    p = os.path.join(tools, filename)
    if not os.path.exists(p):
        return None
    spec = importlib.util.spec_from_file_location(name, p)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# ------------------------------------------------------- process etiquette

_own_lock = threading.Lock()
OWN_PIDS = set()


def own_pids():
    with _own_lock:
        return set(OWN_PIDS)


def list_processes():
    """[(image name, pid)] of every process (Windows: tasklist). On another OS
    this returns [] -- there is no game to protect there."""
    if not IS_WINDOWS:
        return []
    try:
        out = subprocess.run(["tasklist", "/FO", "CSV", "/NH"], capture_output=True,
                             text=True, timeout=60,
                             creationflags=CREATE_NO_WINDOW).stdout
    except Exception:
        return []
    rows = []
    for r in csv.reader(io.StringIO(out)):
        if len(r) >= 2:
            try:
                rows.append((r[0], int(r[1])))
            except ValueError:
                pass
    return rows


def foreign_running(names, lister=None, own=None):
    """Processes named in `names` (case-insensitive) that we did not start."""
    want = set(n.lower() for n in names)
    own = own_pids() if own is None else own
    return [(img, pid) for img, pid in (lister or list_processes)()
            if img.lower() in want and pid not in own]


class ForeignWaitTimeout(Exception):
    pass


def wait_for_foreign(runs_cfg, lister=None, sleep=time.sleep, clock=time.time,
                     say=None, own=None):
    """Block while a game process we did not start is running. Returns the
    seconds waited. runs_cfg: the config's "runs" block."""
    say = say or log
    names = runs_cfg.get("foreign_processes") or []
    poll = max(1, int(runs_cfg.get("foreign_poll_seconds") or 30))
    cap = float(runs_cfg.get("foreign_max_wait_seconds") or 0)
    t0 = clock()
    told = False
    while True:
        hits = foreign_running(names, lister, own)
        if not hits:
            if told:
                say("the other game window is gone; carrying on after %.0fs" % (clock() - t0))
            return clock() - t0
        if not told:
            say("waiting: %s is running and was not started by this script "
                "(someone may be playing). Checking every %ds."
                % (", ".join("%s pid %d" % h for h in hits), poll))
            told = True
        if cap and clock() - t0 >= cap:
            raise ForeignWaitTimeout("still running after %.0fs: %s" % (
                clock() - t0, ", ".join("%s pid %d" % h for h in hits)))
        sleep(poll)


def chill_active(runs_cfg, lister=None):
    names = set(n.lower() for n in runs_cfg.get("chill_processes") or [])
    return any(img.lower() in names for img, _ in (lister or list_processes)())


def kill_own(proc):
    """Kill exactly the process we started (and its own children), by pid."""
    if proc.poll() is not None:
        return
    if IS_WINDOWS:
        subprocess.run(["taskkill", "/T", "/F", "/PID", str(proc.pid)],
                       capture_output=True, creationflags=CREATE_NO_WINDOW)
    if proc.poll() is None:
        proc.kill()
    try:
        proc.wait(timeout=30)
    except Exception:
        pass


def _rw(func, path, _exc):
    try:
        os.chmod(path, stat.S_IWRITE | stat.S_IREAD)
    except OSError:
        pass
    func(path)


def rmtree(p):
    if os.path.isdir(p):
        shutil.rmtree(p, onerror=_rw)


def write_settings(wdir, mode, extra=None):
    """Settings for one row. mode "on": no settings.json at all; "off": the
    two keys false (+ extra keys); a dict: written as is."""
    p = os.path.join(wdir, "settings.json")
    if os.path.exists(p):
        os.remove(p)
    if isinstance(mode, dict):
        data = dict(mode)
    else:
        if mode not in SETTINGS_MODES:
            raise ValueError("unknown settings mode %r" % (mode,))
        data = SETTINGS_MODES[mode]
        data = None if data is None else dict(data)
    if extra:
        data = dict(data or {})
        data.update(extra)
    if data is None:
        return None
    with open(p, "w", encoding="ascii") as f:
        json.dump(data, f)
    return p


def prep_row(work_dir, name, exe, settings_mode="off", settings_extra=None, with_map=True):
    """Fresh row folder with our own writable copy of the exe (+ map)."""
    wdir = os.path.join(work_dir, "rows", name)
    rmtree(wdir)
    os.makedirs(wdir)
    mine = os.path.join(wdir, "walk_window.exe")
    shutil.copyfile(exe, mine)
    os.chmod(mine, stat.S_IWRITE | stat.S_IREAD | stat.S_IEXEC)   # clears the read-only bit
    if with_map:
        m = os.path.splitext(exe)[0] + ".map"
        if os.path.exists(m):
            shutil.copyfile(m, os.path.join(wdir, "walk_window.map"))
    write_settings(wdir, settings_mode, settings_extra)
    return wdir, mine


def quiet_env(wdir, name, asset_root, extra, faults_fatal=True, base=None):
    env = {k: v for k, v in (base if base is not None else os.environ).items()
           if not k.startswith("SM64DS_")}
    env["TEMP"] = env["TMP"] = wdir
    env.update({"SM64DS_NO_FOCUS": "1", "SM64DS_VOLUME": "0", "SM64DS_MINIMIZED": "1",
                "SM64DS_NO_DIALOG": "1", "SM64DS_INSTANCE": "ps_" + name,
                "SM64DS_ASSET_ROOT": asset_root})
    if faults_fatal:
        env["SM64DS_FAULTS_FATAL"] = "1"
    env.update({k: str(v) for k, v in (extra or {}).items()})
    return env


class RunResult(object):
    def __init__(self, name, wdir, rc, seconds, out, exe_md5, timed_out):
        self.name, self.wdir, self.rc, self.seconds = name, wdir, rc, seconds
        self.out, self.exe_md5, self.timed_out = out, exe_md5, timed_out

    @property
    def fatal_lines(self):
        return [l for l in self.out.splitlines() if "FATAL" in l or l.startswith("FAULT")]

    @property
    def quarantine_lines(self):
        return [l for l in self.out.splitlines() if "[quarantine]" in l]

    @property
    def ok(self):
        return ((not self.timed_out) and self.rc == 0 and not self.fatal_lines
                and not self.quarantine_lines)

    def bmp(self):
        return os.path.join(self.wdir, "walk_window_selftest.bmp")

    def describe(self):
        rc = "TIMEOUT" if self.timed_out else self.rc
        return "rc=%s fatal=%d %.0fs md5=%s" % (rc, len(self.fatal_lines), self.seconds,
                                              self.exe_md5[:8])


_launch_slots = {}
_launch_slots_lock = threading.Lock()


def _slot(cfg):
    n = max(1, int(cfg["runs"].get("max_parallel") or 1))
    with _launch_slots_lock:
        if n not in _launch_slots:
            _launch_slots[n] = threading.BoundedSemaphore(n)
        return _launch_slots[n]


@contextlib.contextmanager
def phase_lock(cfg, label):
    """Optional: hold the machine-wide windowed test slot (port/tools/
    slot_lock.py) for a whole step, when runs.test_lock_path is set."""
    p = cfg["runs"].get("test_lock_path")
    if not p:
        yield
        return
    os.environ["SM64DS_TEST_LOCK_PATH"] = p
    sl = load_tree_module("tree_slot_lock", "slot_lock.py", cfg["tree_root"])
    if sl is None:
        log("test_lock_path is set but port/tools/slot_lock.py is missing; running without it")
        yield
        return
    with sl.slot_reentrant(label=label):
        yield


def launch(cfg, exe, name, extra_env, settings_mode="off", settings_extra=None,
           timeout=None, faults_fatal=True, wdir=None, extra_files=None):
    """One quiet game run. Returns RunResult. Blocks while a foreign game runs."""
    with _slot(cfg):
        if wdir is None:
            wdir, mine = prep_row(cfg["work_dir"], name, exe, settings_mode, settings_extra)
        else:
            mine = os.path.join(wdir, "walk_window.exe")
        for src, dst in (extra_files or []):
            shutil.copyfile(src, os.path.join(wdir, dst))
        wait_for_foreign(cfg["runs"])
        flags = CREATE_NO_WINDOW if IS_WINDOWS else 0
        chill = IS_WINDOWS and chill_active(cfg["runs"])
        if chill:
            flags |= IDLE_PRIORITY_CLASS
        env = quiet_env(wdir, name, cfg["asset_root"], extra_env, faults_fatal)
        with open(os.path.join(wdir, "env.txt"), "w", encoding="ascii", errors="replace") as f:
            for k in sorted(env):
                if k.startswith("SM64DS_"):
                    f.write("%s=%s\n" % (k, env[k]))
        si = None
        if IS_WINDOWS:
            si = subprocess.STARTUPINFO()
            si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            si.wShowWindow = SW_SHOWMINNOACTIVE
        timeout = timeout or int(cfg["runs"].get("timeout_seconds") or 2400)
        md5 = md5_file(mine)
        log_path = os.path.join(wdir, "out.txt")
        t0 = time.time()
        timed_out = False
        with open(log_path, "wb") as fh:
            p = subprocess.Popen([mine], cwd=wdir, env=env, stdout=fh,
                                 stderr=subprocess.STDOUT, startupinfo=si,
                                 creationflags=flags)
            with _own_lock:
                OWN_PIDS.add(p.pid)
            try:
                rc = p.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                kill_own(p)
                rc, timed_out = None, True
            finally:
                with _own_lock:
                    OWN_PIDS.discard(p.pid)
        with open(log_path, encoding="utf-8", errors="replace") as f:
            out = f.read()
        r = RunResult(name, wdir, rc, time.time() - t0, out, md5, timed_out)
        log("   run %-34s %s%s" % (name, r.describe(), " IDLE(chill)" if chill else ""))
        if not cfg["runs"].get("keep_row_dirs"):
            for fn in ("walk_window.exe",):
                with contextlib.suppress(OSError):
                    os.remove(os.path.join(wdir, fn))
        return r


# ------------------------------------------------------------- map parsing

MAP_ROW = re.compile(r"^\s+([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+(\S+)\s+([0-9a-fA-F]{8})"
                     r"\s+(?:[fi]\s+)*(\S+)\s*$")
MAP_BASE = re.compile(r"Preferred load address is\s+([0-9a-fA-F]+)")


def undecorate(sym):
    """MSVC cdecl decoration is exactly ONE leading underscore (see
    port/tools/linkage.py's THE DECORATION RULE)."""
    return sym[1:] if sym.startswith("_") else sym


def obj_basename(obj):
    o = obj.replace("\\", "/").split("/")[-1]
    return o.split(":")[-1] if ":" in o and not o.startswith("<") else o


def obj_stem(obj):
    b = obj_basename(obj)
    for suf in (".cpp.obj", ".c.obj", ".obj"):
        if b.endswith(suf):
            return b[:-len(suf)]
    return b


class MapInfo(object):
    def __init__(self, path):
        self.path = path
        self.base = None
        self.rows = []          # (sym undecorated, addr, obj basename, static)
        static = False
        with open(path, errors="replace") as f:
            for line in f:
                if self.base is None:
                    m = MAP_BASE.search(line)
                    if m:
                        self.base = int(m.group(1), 16)
                if line.strip() == "Static symbols":
                    static = True
                m = MAP_ROW.match(line)
                if m and m.group(1) != "0000":
                    self.rows.append((undecorate(m.group(3)), int(m.group(4), 16),
                                      obj_basename(m.group(5)), static))
        pub = sorted((a, s) for s, a, o, st in self.rows if a)
        self._addrs = [a for a, _ in pub]
        self._names = [s for _, s in pub]

    def defined(self):
        """{symbol: obj} of public (non-static) rows; first definition wins."""
        d = {}
        for s, a, o, st in self.rows:
            if not st and s not in d:
                d[s] = o
        return d

    def objects(self):
        return set(o for _, _, o, _ in self.rows)

    def symbolize(self, addr):
        """(name, offset) of the symbol at or below addr, or None."""
        i = bisect.bisect_right(self._addrs, addr) - 1
        if i < 0:
            return None
        return (self._names[i], addr - self._addrs[i])


# --------------------------------------------------------------- PE parsing

def pe_info(path):
    """Image base, size, end, DLL list and imported names of a PE32/PE32+."""
    with open(path, "rb") as f:
        b = f.read()
    if b[:2] != b"MZ":
        raise ValueError("%s: not an MZ image" % path)
    pe = struct.unpack_from("<I", b, 0x3c)[0]
    if b[pe:pe + 4] != b"PE\0\0":
        raise ValueError("%s: no PE signature" % path)
    nsec = struct.unpack_from("<H", b, pe + 6)[0]
    opt_size = struct.unpack_from("<H", b, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from("<H", b, opt)[0]
    if magic == 0x10b:
        base = struct.unpack_from("<I", b, opt + 28)[0]
        dd = opt + 96
        thunk_size, ord_flag = 4, 0x80000000
    elif magic == 0x20b:
        base = struct.unpack_from("<Q", b, opt + 24)[0]
        dd = opt + 112
        thunk_size, ord_flag = 8, 1 << 63
    else:
        raise ValueError("%s: unknown optional header magic 0x%x" % (path, magic))
    size = struct.unpack_from("<I", b, opt + 56)[0]
    imp_rva = struct.unpack_from("<I", b, dd + 8)[0]
    secs = []
    so = opt + opt_size
    for i in range(nsec):
        vsize, va, rsize, rptr = struct.unpack_from("<IIII", b, so + i * 40 + 8)
        secs.append((va, max(vsize, rsize), rptr))

    def off(rva):
        for va, sz, rp in secs:
            if va <= rva < va + sz:
                return rp + rva - va
        raise ValueError("rva 0x%x outside every section" % rva)

    def cstr(o):
        e = b.index(b"\0", o)
        return b[o:e].decode("ascii", "replace")

    dlls, names = [], []
    if imp_rva:
        o = off(imp_rva)
        while True:
            ilt, _ts, _fc, name_rva, iat = struct.unpack_from("<IIIII", b, o)
            if not (ilt or name_rva or iat):
                break
            dlls.append(cstr(off(name_rva)))
            t = off(ilt or iat)
            while True:
                v = struct.unpack_from("<Q" if thunk_size == 8 else "<I", b, t)[0]
                if not v:
                    break
                if v & ord_flag:
                    names.append("ord%d" % (v & 0xffff))
                else:
                    names.append(cstr(off(v & 0x7fffffff) + 2))
                t += thunk_size
            o += 20
    sha = hashlib.sha256("\n".join(sorted(names)).encode()).hexdigest()
    return {"base": base, "size": size, "end": base + size, "dlls": dlls,
            "imports": names, "import_count": len(names), "import_sha256": sha}


def build_test_pe(base, size, dlls):
    """A minimal PE32 image with an import table, for the selftest.
    dlls: [(dll name, [import names])]."""
    sec_rva, sec_raw = 0x1000, 0x200
    data = bytearray()
    ndesc = len(dlls) + 1
    desc_off = 0
    data += b"\0" * (20 * ndesc)
    descs = []
    for dll, imps in dlls:
        ilt_off = len(data)
        data += b"\0" * (4 * (len(imps) + 1))
        hn = []
        for nm in imps:
            hn.append(len(data))
            data += b"\0\0" + nm.encode() + b"\0"
            if len(data) % 2:
                data += b"\0"
        for i, h in enumerate(hn):
            struct.pack_into("<I", data, ilt_off + 4 * i, sec_rva + h)
        name_off = len(data)
        data += dll.encode() + b"\0"
        descs.append((ilt_off, name_off))
    for i, (ilt_off, name_off) in enumerate(descs):
        struct.pack_into("<IIIII", data, desc_off + 20 * i, sec_rva + ilt_off, 0, 0,
                         sec_rva + name_off, sec_rva + ilt_off)
    while len(data) % 0x200:
        data += b"\0"
    pe = 0x80
    img = bytearray(sec_raw)
    img[0:2] = b"MZ"
    struct.pack_into("<I", img, 0x3c, pe)
    img[pe:pe + 4] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", img, pe + 4, 0x14c, 1, 0, 0, 0, 0xE0, 0x102)
    opt = pe + 24
    struct.pack_into("<H", img, opt, 0x10b)
    struct.pack_into("<I", img, opt + 28, base)
    struct.pack_into("<II", img, opt + 32, 0x1000, 0x200)
    struct.pack_into("<I", img, opt + 56, size)
    struct.pack_into("<I", img, opt + 60, 0x200)
    struct.pack_into("<I", img, opt + 92, 16)
    struct.pack_into("<II", img, opt + 96 + 8, sec_rva, 20 * ndesc)
    so = opt + 0xE0
    img[so:so + 8] = b".idata\0\0"
    struct.pack_into("<IIII", img, so + 8, len(data), sec_rva, len(data), sec_raw)
    return bytes(img) + bytes(data)
