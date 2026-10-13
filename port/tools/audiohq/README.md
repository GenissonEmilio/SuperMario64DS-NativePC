# audiohq: checks for the 48 kHz audio and the .wav / .ogg sound mods

These scripts check, on your own PC and with no AI involved, that

1. the new "hq" sound (48000 Hz mixing, smoother resampler) sounds like the old
   "ds" sound (the DS's own 32768 Hz) in time, level and pitch, and that "ds" is
   still byte-for-byte what it was;
2. the game itself is unchanged by it (pictures, game state);
3. plain `.wav` / `.ogg` sound mods work in the real game and bad files are
   skipped without a crash.

Everything runs the game **muted, minimized and never focused**, and only ever
stops the game processes it started itself.

## One-time set-up

1. You need Python 3.8 or newer. Nothing else to install.
2. Copy `config.example.json` to `config.json` (same folder) and fill in the
   paths. That one file is the only place a path of your machine is written.
   * `exe_base`: a `walk_window.exe` built **before** this work (the shipped
     0.9.1 dev build, or any build of `lead/audio-snapshot`)
   * `asset_root`: the folder that holds `extracted/`, the same one the other
     run scripts use
   * `out_dir`: any scratch folder for the results
   * `exe_tip`: the exe built from this branch. `build.py` makes it for you
     (next section); you can leave it out of the config if you use `--build`.
3. Close big games first (the cost check is skipped while VRChat or a League
   match runs, and says so).

## Build the exe from this branch

    python build.py

It uses the staged build queue if `stage_py` is set in the config, else the
tree's own machine-wide build lock, else it builds directly. The finished exe is
copied to `<out_dir>/exes/tip/walk_window.exe` with its md5 and the line to put
in the config. Builds take a while; the first one in a fresh tree takes the
longest.

## Run everything

    python run_all.py --build

(without `--build` if you already built, and `--quick` for routes of 20 seconds
instead of 60). It prints one table of PASS / FAIL rows when it is done. The
whole thing is about 130 game launches, two at a time: plan on one to two hours
(`--quick` roughly halves it).

Or one part at a time:

| script | what it checks |
|---|---|
| `audio_check.py` | the sound: ds is identical to the old exe, hq lines up in time, level and pitch, cost |
| `mods_check.py` | .wav / .ogg mods: music, loops, fades, pause, one wave, bad files |
| `gate.py` | the game itself: pictures, trap counts, RASTER_AB, TWOD_VERIFY, heap, game state |

Each takes `--quick`, `--set key=value` (change any config key for one run) and
`--config path`.

## Reading the table

* `PASS` the check held.
* `FAIL` it did not. The detail says what was measured.
* `INCONCLUSIVE` the run could not decide (for example the route had no fade in
  it to follow). Not a failure, but it means that point is unproven.
* `SKIP` not done (the machine was busy, so no timing was taken).

## If something FAILs

`run_all.py` makes `<out_dir>/send_back.zip` (the same files are in the folder
`<out_dir>/send_back/`). Send that one file back. It holds the table, the exe
versions and the game log of every run. Nothing in it is private except the file
paths in the logs.

## What each check does, in a few lines

**audio_check**: each route (level 1 idle, level 1 walking and jumping, level 6
idle, and a pulse-wave-only route with the music silenced) runs on the old exe
twice (to see the route repeats), on the new exe in "ds" mode, in "hq" mode, in
"hq" with the `SM64DS_AUDIO_HQ=0` switch and with no setting at all. The mix is
captured with `SM64DS_NO_AUDIO=1` and `SM64DS_WAV_DUMP`.
(a) the ds dump has the same bytes as the old exe's; the kill switch gives the
ds bytes; no setting gives 48000 Hz. (b) the hq dump is 48000 Hz and the same
length; the loudness over time (192 times a second, the game's own clock) lines
up with the ds dump with the best match at a lag of exactly 0 ticks; note
starts agree within one tick; the level is within 1 dB; there is no sample jump
bigger than the ds dump has; the pulse (PSG) notes have the same pitch (FFT).
(c) the game-state hash is the same on old, ds and hq. (d) the mixer's own cost
per second of audio, and the whole process's CPU time.

**mods_check**: builds its test mods itself. A looping tune (an .ogg with loop
tags, a .wav with a loop chunk) replaces every music track, so whatever the
route plays becomes the tune; the check looks for the tune's own pitch, the loop,
the volume following the game's fades, and the hold during pause. One wave of a
wave archive is replaced and a scripted sound that uses it is played: the voice
log must show the new clip, at the same playback rate on every note. Then a mod
of bad files (cut-off, wrong format, wrong name, too big, wrong folder...) must
leave one log line per file and the game must run to the end.

**gate**: the rows of the lane gate (pictures at levels 1, 6, 8, F3-off,
renderer-on, trap counts, RASTER_AB, TWOD_VERIFY, heap lines; the game-state
hash is in `audio_check` (c)) run on the old exe
and the new exe in ds, hq and default modes; all must give the same answer. Rows
with menus run twice: with the default menus and with `OneScreenMenus` /
`HudLayout` set to false (the older references). The old reference values are
printed for information only.

## Files

| file | what it is |
|---|---|
| `config.example.json` | copy to `config.json` |
| `build.py` | builds the exe |
| `run_all.py` | runs the three checks and makes the table and the send-back zip |
| `audio_check.py` `mods_check.py` `gate.py` | the checks |
| `audiohq_common.py` `wavlib.py` `sdat_tools.py` | shared code (config, quiet launch, WAV analysis, reading the sound archive) |
| `fixtures/` | three tiny .ogg files the mod checks use; `make_fixtures.py` regenerates them |
| `dev/` | tests of these scripts themselves (not needed on a normal run), see below |

## Testing the scripts themselves (for developers)

`python dev/selftest.py` (Linux or anything with g++ and python) builds the
audio code into a small test program, wraps it in a stand-in for the game and
runs all three scripts against it with a synthetic sound archive. `--negative`
also breaks the stand-in on purpose and checks the scripts notice. This proves
the scripts work; only a run against the real exes proves the game.
`dev/fuzz_files.py` throws damaged .ogg / .wav files at the audio code under the
address sanitizer.
