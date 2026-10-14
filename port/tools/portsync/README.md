# portsync: check a new port build against the old one

These scripts build the port, then check that the new game behaves exactly like
the old one. They run on their own (no AI needed). Every game window they open
is minimized, muted and never takes your keyboard. If you are playing the game
(walk_window.exe or SM64DSLauncher.exe is open), they wait until you close it.

**One-time setup** (in a terminal, in the repo folder):

1. `python port\tools\portsync\run_all.py --write-config`
2. Open `port\tools\portsync\portsync_config.json` in Notepad and replace every
   value in `<angle brackets>`:
   `work_dir` (a folder for logs and copies, on a big drive), `old_exe` (the
   old walk_window.exe you trust; its .map must sit next to it), and
   `build.lock_path` (the build lock file every build on this PC uses).
3. Check the scripts themselves: `python port\tools\portsync\run_all.py --selftest`
   (should end with `PORTSYNC SELFTEST: PASS`).

**Every time:** `python port\tools\portsync\run_all.py`
(add `--quick` to skip the slow opening / ending checks; `--new-exe PATH` to
test an exe you already built instead of building).

**Reading the result.** At the end you get one table:

| STEP    | what it checks |
|---------|----------------|
| build   | the port builds (through the PC's shared build queue) |
| linkage | nothing that used to be in the game went missing |
| gate    | the standard test pictures and numbers still match, with the menu settings off AND on |
| compare | the old and new game play the same routes frame by frame |

`PASS` = fine. `WARN` = fine but worth a look. `FAIL` = something changed:
the WHAT column says the first thing that differs (for compare: the first
frame). The run stops at the first FAIL and marks the rest `NOT RUN`.
The table is saved in `work_dir\reports\`; full logs are in `work_dir\logs\`.
Send those two files to whoever is fixing the port.

The single steps can also be run alone: `build.py`, `linkage.py`, `gate.py`,
`compare.py` (each takes `--help`).
