#!/usr/bin/env python3
"""ARENA E2E: real launchers on an arena lobby (run mp1, lane ARENAGATE1, plan A6).

Wraps the private launcher repo's tests/MpE2E/mp_e2e.py (scenario vsarena,
ARENALOBBY1): N real LobbyForm launcher windows on a private scratch desktop
drive a LOCAL lobby (this tree's port/tools/lobby/app/server.py with
ARENA_GAME_IDS set) and a LOCAL relay (this tree's REFEREE1 relay), and launch
real games the way a player's launcher does. That is the only path that proves
the pieces the other arena rungs emulate: the launcher's env block, the lobby's
go plan and digest, the launch WATCHDOG (A4: it kills a game that never prints
'[vsarena] session up' within 60 s of launch / 120 s after the seat line) and the
result the room reads.

mp_e2e.py's own vsarena verdicts are deliberately weak (what the games do depends
on the exe), so this file judges the whole run again:
  - the e2e driver's own VERDICT line is PASS (0 fails)
  - every game's playlog (the launcher-run game writes its stderr to
    <bundle>/playlog): arena_logs.judge_match, exactly as the direct rungs: one
    session up, one START, ONE byte-identical referee MATCH OVER, no two winners,
    no lockstep line, no fault
  - THE MATCH RAN PAST 60 s: its MATCH OVER frame is >= 1800 (30 fps), so the
    launcher's 60 s watchdog window was crossed with the game alive
  - the lobby never posted `failed` for the match (no_pairing / spawn_failed /
    wrong_player_count) and posted ONE result whose win= is time-up
  - every launcher's room read the result ('Match over')
  - the games' env was the arena plan's: SM64DS_VS_NETMODE=arena, the arena digest
  - must-FAIL control: the same judge on the logs with one console's referee
    verdict removed goes red

The knobs added to every game (peer runner, after the launcher built its env) turn
the rung-7 star recipe OFF (BREAKALL / STAR_TRIGGER: nobody may end the match early)
and set ARENA_TIME_S so the clock ends it past 60 s, with the scripted pad so the
bodies move.
"""
import glob
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import arena_logs as AL      # noqa: E402

NO_CONSOLE = getattr(subprocess, "CREATE_NO_WINDOW", 0)
PEER_EXE_REL = os.path.join("tests", "MpE2E", "bin", "Release", "net10.0-windows", "win-x64",
                            "SM64DSLauncher.MpE2E.exe")


def run_e2e(launcher_repo, name, n, exe, assets, extracted, rom, relay_py, server_py, out,
            game_tip, secs, timeout, scenario="vsarena"):
    """Run mp_e2e.py once at IDLE priority (League / VRChat rules; the caller holds
    the ladder lock). -> (rc, stdout text, run dir)."""
    peer = os.path.join(launcher_repo, PEER_EXE_REL)
    cmd = [sys.executable, os.path.join(launcher_repo, "tests", "MpE2E", "mp_e2e.py"),
           "--scenario", scenario, "--n", str(n), "--name", name, "--peer-exe", peer,
           "--exe", exe, "--assets", assets, "--extracted", extracted, "--rom", rom,
           "--server", server_py, "--relay", relay_py, "--out", out, "--arena-ids", "game",
           "--timeout", str(timeout), "--hold", str(timeout)]
    if game_tip:
        cmd += ["--game-tip", game_tip]
    for kv in ("SM64DS_VS_BREAKALL=0", "SM64DS_STAR_TRIGGER=0", "SM64DS_ARENA_TIME_S=%d" % secs,
               "SM64DS_ARENA_REFLOG=1", "SM64DS_PUPPET_LOG=1", "SM64DS_HUB_REPORT_S=5"):
        cmd += ["--knob", kv]
    t0 = time.time()
    logp = os.path.join(out, name + ".stdout.txt")
    os.makedirs(out, exist_ok=True)
    with open(logp, "wb") as lg:
        p = subprocess.Popen(cmd, stdout=lg, stderr=subprocess.STDOUT,
                             creationflags=NO_CONSOLE | 0x00004000)    # BELOW_NORMAL
        try:
            rc = p.wait(timeout=timeout + 300)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
            rc = -9
    with open(logp, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    return rc, text, os.path.join(out, name), time.time() - t0


def read_all(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def judge_run(text, rundir, n, min_frame=1800):
    """-> [(ok, msg)] over the e2e stdout and the run directory."""
    out = []

    def ok(c, m):
        out.append((bool(c), m))
    v = re.search(r"VERDICT (PASS|FAIL) \((\d+) fails\)", text)
    ok(v and v.group(1) == "PASS", "the e2e driver's own VERDICT: %s" % (v.group(0) if v else "none (no verdict line)"))
    fails = re.findall(r"(?m)^FAIL .*$", text)
    if fails:
        ok(False, "driver FAIL lines: %s" % " | ".join(f[:80] for f in fails[:3]))
    texts = {}
    for k in range(n):
        logs = sorted(glob.glob(os.path.join(rundir, "b%d" % k, "playlog", "play_*.log")))
        texts[k] = "\n".join(read_all(p) for p in logs)
        ok(bool(logs), "p%d has a playlog (%d file(s))" % (k, len(logs)))
    rcs = {k: 0 for k in texts}      # the launcher reads the exit; the driver judges it
    ll = read_all(os.path.join(rundir, "lobby.log"))
    failed = re.findall(r"(?m)^.*room \S+ failed match=.*$", ll)
    ok(not failed, "the lobby never failed the match (%s)" % ((failed[:1] or ["-"])[0].strip()[:100]))
    res = re.findall(r"(?m)^.*room \S+ result match=.*$", ll)
    ok(len(res) >= 1 and all("win=time-up" in x for x in res),
       "the lobby posted a time-up result (%d line(s): %s)" % (len(res), (res[:1] or ["-"])[0].strip()[:100]))
    arm = re.findall(r"(?m)^.*room \S+ arm match=.*$", ll)
    ok(bool(arm) and "netmode=arena" in arm[-1], "the lobby armed the match as arena")
    for k in range(n):
        penv = ""
        for pth in sorted(glob.glob(os.path.join(rundir, "p%d_launch*.env" % k))):
            penv += read_all(pth)
        ok("SET SM64DS_VS_NETMODE=arena" in penv and "SM64DS_SESSION_DIGEST" in penv,
           "p%d's game env was the arena plan's (NETMODE arena + digest)" % k)
        ok("SM64DS_LEVEL" not in penv.replace("CLEAR SM64DS_LEVEL", ""), "p%d's game env sets no SM64DS_LEVEL" % k)
        pl = read_all(os.path.join(rundir, "p%d.log" % k))
        ok("Match over" in pl, "p%d's room read the result" % k)
    items = AL.judge_match(texts, rcs, list(range(n)), {"reason": 1, "exit0": False}, rt=None, lane={})
    out += items
    # THE MATCH RAN PAST 60 s
    fr = []
    for k in range(n):
        m = AL.OVER_LEG.search(texts[k])
        fr.append(int(m.group(1)) if m else 0)
    ok(fr and min(fr) >= min_frame, "the match ran past 60 s on every console: MATCH OVER frames %s (>= %d at 30 fps)" % (
        fr, min_frame))
    ok(not any("no_pairing" in x or "spawn_failed" in x for x in [ll] + [read_all(os.path.join(rundir, "p%d.log" % k))
                                                                         for k in range(n)]),
       "no no_pairing / spawn_failed anywhere (the launch watchdog never fired)")
    return out, texts


def run_rung(L, run, r, kit, Checks, Skip):
    """The ladder rung body (mp_ladder.r_arenae2e): the e2e once, judged again."""
    import hashlib
    a = L.a
    exe = a.kit_exe if kit else L.exe
    assets = a.e2e_assets or os.path.join(L.asset_root, "build", "assets")
    extracted = a.e2e_extracted or os.path.join(ROOT, "extracted")
    rom = a.e2e_rom or a.rom or ""
    ck = Checks()
    if not (os.path.exists(assets) and os.path.isdir(extracted) and os.path.exists(rom)):
        raise Skip("needs --e2e-assets / --e2e-extracted / --e2e-rom (or --rom): assets %s extracted %s rom %s" % (
            os.path.exists(assets), os.path.isdir(extracted), os.path.exists(rom)))
    secs = r.opts.get("secs", 75)
    name = r.name.replace("-", "_")
    tip = ""
    try:
        tip = subprocess.run(["git", "-C", ROOT, "rev-parse", "HEAD"], capture_output=True, text=True,
                             creationflags=NO_CONSOLE).stdout.strip()[:10]
    except OSError:
        pass
    rc, text, rundir, wall = run_e2e(a.launcher_repo, name, r.n, exe, assets, extracted, rom, L.relay_py,
                                     os.path.join(ROOT, "port", "tools", "lobby", "app", "server.py"),
                                     run.dir, tip, secs, 600)
    items, texts = judge_run(text, rundir, r.n)
    for okk, m in items:
        ck.ok(okk, m)
    ck.ok(rc == 0, "mp_e2e.py exit %s" % rc)
    # must-FAIL control: one console never heard the referee's verdict
    mut = AL.mutate("deaf", texts)
    if mut:
        red = [m for okk, m in AL.judge_match(mut[1], {k: 0 for k in texts}, list(range(r.n)),
                                              {"reason": 1, "exit0": False}) if not okk]
        ck.ok(bool(red), "control (%s): the judge %s" % (mut[0], "went red as it must" if red else "STAYED GREEN"))
    ck.note("exe md5 %s" % hashlib.md5(open(exe, "rb").read()).hexdigest()[:8])
    ck.note("e2e out %s" % rundir)
    return ck, wall, {"e2e": rc}


if __name__ == "__main__":
    print(__doc__)
