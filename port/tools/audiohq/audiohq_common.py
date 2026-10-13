"""Shared helpers for the audiohq checks: one config file, quiet game launches,
a PASS / FAIL table. Plain python 3.8+, standard library only.

Every machine-specific path lives in ONE place: config.json next to this file
(copy config.example.json), or the file named by --config / AUDIOHQ_CONFIG, and
any key can be overridden on a script's command line with --set key=value.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, HERE)

DEFAULTS = {
    # the game exe built from THIS branch, and the one built before this work
    "exe_tip": "",
    "exe_base": "",
    # the private asset root the game needs (SM64DS_ASSET_ROOT): the folder that
    # holds extracted/ ... the same one the other run scripts use
    "asset_root": "",
    # where rows, logs and results go (made if missing)
    "out_dir": os.path.join(tempfile.gettempdir(), "audiohq"),
    # sound_data.sdat to read names from (default: <asset_root>/extracted/dsd/files/data/sound_data.sdat)
    "sdat": "",
    # the tree builds come from, and how (build.py)
    "repo": REPO,
    "build_cmd": "port\\build-port-k0.cmd",
    "stage_py": "",
    "build_lock_path": "",
    # a settings.json to start every row from (a path; empty = just what the check writes)
    "settings_base": "",
    # how long the audio routes run, in seconds of game time (60 = the full check)
    "route_seconds": 60,
    # two game launches at a time at most
    "workers": 2,
    # per-row wall-clock limit, seconds
    "timeout_s": 1800,
    # the sequence archive record whose entries the game's system-sound probe can play (2 in the real archive)
    "probe_seqarc": 2,
    # the SEQ record SM64DS_SND_MUSIC forces at boot for the "forced track" check (58 = NCS_BGM_CHIJOU)
    "music_force_id": 58,
    # processes that mean "the machine is busy" (timing rows are skipped, not failed)
    "busy_processes": ["VRChat.exe", "League of Legends.exe"],
}


def load_config(argv_overrides=None, path=None):
    cfg = dict(DEFAULTS)
    path = path or os.environ.get("AUDIOHQ_CONFIG") or os.path.join(HERE, "config.json")
    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        for k, v in data.items():
            if k.startswith("_"):
                continue
            cfg[k] = v
        cfg["_config_path"] = path
    else:
        cfg["_config_path"] = None
    for item in argv_overrides or []:
        if "=" not in item:
            sys.exit("--set wants key=value, got %r" % item)
        k, v = item.split("=", 1)
        try:
            cfg[k] = json.loads(v)
        except ValueError:
            cfg[k] = v
    cfg["out_dir"] = os.path.abspath(cfg["out_dir"])
    if not cfg["sdat"] and cfg["asset_root"]:
        cfg["sdat"] = os.path.join(cfg["asset_root"], "extracted", "dsd", "files", "data", "sound_data.sdat")
    return cfg


def common_args(desc):
    ap = argparse.ArgumentParser(description=desc)
    ap.add_argument("--config", help="config file (default: config.json next to the scripts)")
    ap.add_argument("--set", action="append", default=[], metavar="KEY=VALUE",
                    help="override a config key, for example --set route_seconds=20")
    ap.add_argument("--quick", action="store_true", help="shorter routes (route_seconds=20)")
    return ap


def config_from(args):
    cfg = load_config(args.set, args.config)
    if getattr(args, "quick", False):
        cfg["route_seconds"] = min(cfg["route_seconds"], 20)
    return cfg


def need(cfg, *keys):
    miss = [k for k in keys if not cfg.get(k)]
    if miss:
        sys.exit("config is missing %s. Copy config.example.json to config.json and fill it in "
                 "(see README.md)." % ", ".join(miss))
    for k in keys:
        if k.startswith("exe_") and not os.path.exists(cfg[k]):
            sys.exit("%s does not exist: %s" % (k, cfg[k]))


# ---- the machine-chill rule ---------------------------------------------------

def busy_processes(cfg):
    """Names from cfg['busy_processes'] that are running right now (Windows only)."""
    if os.name != "nt":
        return []
    try:
        out = subprocess.run(["tasklist"], capture_output=True, text=True, timeout=30).stdout.lower()
    except Exception:
        return []
    return [p for p in cfg["busy_processes"] if p.lower() in out]


# ---- quiet launches --------------------------------------------------------------

def _clear_ro(p):
    try:
        os.chmod(p, 0o666)
    except OSError:
        pass


def md5(p):
    if not os.path.exists(p):
        return "NO_FILE"
    h = hashlib.md5()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def sha256(p):
    if not os.path.exists(p):
        return "NO_FILE"
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


class Row:
    """Everything one launch produced."""
    def __init__(self, name, wdir):
        self.name = name
        self.wdir = wdir
        self.rc = None
        self.seconds = 0.0
        self.cpu_seconds = None
        self.text = ""          # stdout + stderr of the game
        self.exe = ""
        self.exe_md5 = ""

    def path(self, *p):
        return os.path.join(self.wdir, *p)

    def lines(self, prefix):
        return [l for l in self.text.splitlines() if l.startswith(prefix)]

    def fatal(self):
        return sum(1 for l in self.text.splitlines()
                   if "FATAL" in l or l.startswith("FAULT") or "EXCEPTION_" in l)


def prepare_row(cfg, name, exe, settings=None, mods=None, subdir="rows"):
    """A fresh private folder for one launch: a copy of the exe (and its .map),
    a settings.json, and any mods. `settings` is a dict merged over settings_base.
    `mods` maps mod id -> function(mod_folder) that fills the folder in; the ids
    are put in EnabledMods in the order given."""
    wdir = os.path.join(cfg["out_dir"], subdir, name)
    if os.path.isdir(wdir):
        shutil.rmtree(wdir, onerror=lambda f, p, e: (_clear_ro(p), f(p)))
    os.makedirs(wdir)
    mine = os.path.join(wdir, os.path.basename(exe))
    shutil.copyfile(exe, mine)
    _clear_ro(mine)
    try:
        os.chmod(mine, 0o755)
    except OSError:
        pass
    mp = os.path.splitext(exe)[0] + ".map"
    if os.path.exists(mp):
        shutil.copyfile(mp, os.path.join(wdir, os.path.basename(mp)))
    st = {}
    if cfg.get("settings_base"):
        with open(cfg["settings_base"], "r", encoding="utf-8") as f:
            st = json.load(f)
    st.update(settings or {})
    if mods:
        st["EnabledMods"] = list(mods.keys())
        for mid, fill in mods.items():
            mf = os.path.join(wdir, "mods", mid)
            os.makedirs(mf)
            with open(os.path.join(mf, "mod.json"), "w", encoding="utf-8") as f:
                json.dump({"id": mid, "kind": "files", "name": mid, "version": "1.0"}, f)
            fill(mf)
    with open(os.path.join(wdir, "settings.json"), "w", encoding="utf-8") as f:
        json.dump(st, f, indent=1)
    return wdir, mine


def _cpu_seconds(p):
    """Kernel + user CPU time of a finished child, Windows only; None elsewhere."""
    if os.name != "nt":
        return None
    try:
        import ctypes
        from ctypes import wintypes
        c, e, k, u = (wintypes.FILETIME(), wintypes.FILETIME(), wintypes.FILETIME(), wintypes.FILETIME())
        if not ctypes.windll.kernel32.GetProcessTimes(int(p._handle), ctypes.byref(c), ctypes.byref(e),
                                                        ctypes.byref(k), ctypes.byref(u)):
            return None
        t = lambda ft: ((ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 1e7
        return t(k) + t(u)
    except Exception:
        return None


def launch(cfg, name, exe, env=None, settings=None, mods=None, subdir="rows", timeout=None,
           faults_fatal=True, quiet=True):
    """One quiet game launch: minimized, never activated, muted, no console, no
    inherited SM64DS_* variables, a private folder. Only the process started
    here is ever killed (its own timeout). Returns a Row."""
    wdir, mine = prepare_row(cfg, name, exe, settings, mods, subdir)
    row = Row(name, wdir)
    row.exe = exe
    row.exe_md5 = md5(mine)
    e = {k: v for k, v in os.environ.items() if not k.startswith("SM64DS_")}
    e["TEMP"] = e["TMP"] = wdir
    if quiet:
        e.update({"SM64DS_NO_FOCUS": "1", "SM64DS_VOLUME": "0", "SM64DS_MINIMIZED": "1",
                  "SM64DS_NO_DIALOG": "1", "SM64DS_INSTANCE": "ahq_" + name[:40]})
    if cfg.get("asset_root"):
        e["SM64DS_ASSET_ROOT"] = cfg["asset_root"]
    if faults_fatal:
        e["SM64DS_FAULTS_FATAL"] = "1"
    for k, v in (env or {}).items():
        e[k] = str(v).replace("{wdir}", wdir.replace("\\", "/"))
    kw = {}
    if os.name == "nt":
        si = subprocess.STARTUPINFO()
        si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        si.wShowWindow = 7     # SW_SHOWMINNOACTIVE
        kw = dict(startupinfo=si, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    t0 = time.time()
    logp = os.path.join(wdir, "out.txt")
    with open(logp, "wb") as fh:
        p = subprocess.Popen([mine], cwd=wdir, env=e, stdout=fh, stderr=subprocess.STDOUT, **kw)
        try:
            row.rc = p.wait(timeout=timeout or cfg["timeout_s"])
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
            row.rc = "TIMEOUT"
        row.cpu_seconds = _cpu_seconds(p)
    row.seconds = time.time() - t0
    with open(logp, "r", encoding="utf-8", errors="replace") as f:
        row.text = f.read()
    print("   [launch] %-28s md5=%s rc=%s %.0fs" % (name, row.exe_md5[:8], row.rc, row.seconds), flush=True)
    return row


def run_parallel(jobs, workers):
    """jobs: list of zero-argument callables. Returns their results in order,
    running `workers` at a time."""
    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor(max(1, workers)) as ex:
        return list(ex.map(lambda j: j(), jobs))


# ---- the result table ----------------------------------------------------------------

class Results:
    """PASS / FAIL / INCONCLUSIVE / SKIP rows with a one-line detail."""
    def __init__(self, title):
        self.title = title
        self.rows = []

    def add(self, name, status, detail=""):
        assert status in ("PASS", "FAIL", "INCONCLUSIVE", "SKIP")
        self.rows.append((name, status, detail))
        print("  %-12s %s  %s" % (status, name, detail), flush=True)

    def failed(self):
        return [r for r in self.rows if r[1] == "FAIL"]

    def table(self):
        w = max([len(r[0]) for r in self.rows] + [10])
        out = ["== %s ==" % self.title]
        for n, s, d in self.rows:
            out.append("%-*s  %-12s %s" % (w, n, s, d))
        out.append("%d PASS, %d FAIL, %d INCONCLUSIVE, %d SKIP" % tuple(
            sum(1 for r in self.rows if r[1] == k) for k in ("PASS", "FAIL", "INCONCLUSIVE", "SKIP")))
        return "\n".join(out)

    def save(self, cfg, fname):
        os.makedirs(cfg["out_dir"], exist_ok=True)
        p = os.path.join(cfg["out_dir"], fname)
        with open(p, "w", encoding="utf-8") as f:
            f.write(self.table() + "\n")
        return p


def tail(text, n=25):
    return "\n".join(text.splitlines()[-n:])
