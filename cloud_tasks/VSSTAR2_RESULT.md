# VSSTAR2 result (VS star tie)

Base origin/cloud/fold9b-pre2, branch cloud/vsstar2. Linux VM: the game was NOT built or run; the client change
(port/hal/vs_arena_ref.cpp) is unbuilt and needs the local rows below. FIELD9_REPORT.md / FIX9C.md / STATUS_FIX9C.md are
not on this base, so the FIX9C facts come from the code (referee.py STAR_POSE_MS, `star_stamp`).

## Cause
The touch started the ROM star-get first and asked the referee second:
- vs_arena_ref.cpp neutralise_foreign_star_touches (was :1898-1923): for MY touch it returned early (touch flags
  star+0x130/+0x134 left in place), so the ROM's PowerStar Behavior went to state 5 and put the body in the Yoshi
  star pose (St_NoControl, kStarPoseState) in that same frame's walk.
- grab_detect (was :2088-2118, post-walk) only then saw state 5 and sent CLAIM_GRAB; star5_arena (:2073) held the star
  over the head until the verdict, then on a loss `release_my_yoshi()` pulled the body out of the pose.
So a tie's loser posed for one round trip (up to kHoldMs = 500 ms) and was then yanked back.
Referee side was already right: one STAR_TAKEN, the loser gets DENY LOST, STAR_TAKEN goes to both, and only the winner's
seat gets `star_stamp` (referee.py `_grab`, the 1.5 s STAR_POSE_MS immunity), so that is unchanged.

## Change
- port/hal/vs_arena_ref.cpp neutralise_foreign_star_touches (:1901): my allowed touch now only sends CLAIM_GRAB and
  holds the star (Inst.pending, with the saved touch flags/id). The flags are cleared like a foreign touch, so the ROM
  star-get never starts.
- hold_step() (:1962, called from vs_ref_pre_walk :3403): on my verdict (STAR_TAKEN mine -> result 1) the saved touch is
  handed back to the star and the ROM's own star-get runs (pose / dance / fanfare / count; star5_arena result==1 path;
  lock set; FIX9C immunity is the referee's winner-only stamp). On a loss/deny/timeout nothing was started, so nothing
  is undone; a star taken by another seat is shown over that seat's head like on any other console (show_star_start).
  250 ms retry guard after a loss; a handed-back win the ROM never takes (500 ms) removes the star.
- Lag: the winner's star-get starts one claim round trip late (the loser used to start immediately). Expect the
  seat's RTT, typically 30-100 ms; worst case the 500 ms verdict timeout (touch then dropped, no pose).
- Control: SM64DS_ARENA_NOHOLDTOUCH=1 restores the old early star-get (A/B row). The cancel path was not needed.
- Referee/relay python is unchanged except tests: NO RELAY REDEPLOY NEEDED.

## Tests (Linux)
- port/tools/relay/test_referee.py: new u_star_tie (:2213): 10 trials, two GRAB claims 1-4 ms apart; exactly one
  STAR_TAKEN and one DENY LOST, count +1, loser told who took it, immunity stamp for the winner only. 50 checks.
- `python3 test_referee.py unit`: 23 tests, 540 checks, 0 failed. `test_referee.py fuzz 20000`: pass. `vectors`: pass.
- `python3 test_relay_v2.py`: 22 tests, 1121 checks, 0 failed.

## Local rows (Windows)
1. Build port with this branch; must-fail first on mpfold9's exe: 2P VS, both players run into the same star together
   10 times -> there the loser poses/counts at least once.
2. Same 10 on this build: the loser never poses, never gets the fanfare, count unchanged, sees the star go to the winner;
   the winner poses/counts normally (note the added delay) and is hit-immune ~1.5 s.
3. Solo grab (no tie): pose starts promptly (latency ~RTT), count +1 once; log shows "claim only ... held" then "is mine".
4. Hurt/NoControl body touching a star: still no claim, no pose (F9-b gate unchanged).
5. SM64DS_ARENA_NOHOLDTOUCH=1 reproduces the old behaviour.
Open: the touch hand-back relies on the ROM re-reading star+0x130/+0x134 in its next Behavior; verify the winner really
enters state 5 (log "ROM never started the star-get" would show otherwise).
