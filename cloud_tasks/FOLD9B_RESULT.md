# FOLD9B result (pre-merge for mptest9b + REVFIX9 must-fixes)

Branch cloud/fold9b-pre, base origin/port/l7-mpfold9. No build, no game (Linux VM); python tests run.

## 1. Merge log (all `--no-ff`, author Tango)
Order: port/l7-fix9a, port/l7-fix9b, port/l7-fix9c, port/l7-cooptp1, cloud/cooptp2, port/l7-pinvite1, cloud/wparty2, port/l7-pausemp1,
cloud/grabpose1, cloud/ghostdoor2, cloud/roomset1, cloud/playint1.

CONFLICTS: none. Every merge was clean (party.cpp keeps e4ab0eb11c's "stack of 3+" logic; verified `if (!third) return;` present).
cloud/pausescout1 exists but its result says "no code changed" (code-reading only), so it was NOT merged.

## 2. REVFIX9 fixes (one commit each)
- must 1, 61d3a594: `port/hal/world_party.cpp` `input_live()` + `world_party_input`: edges count only in a level with control
  (in a level, not leaving, no pause `data_0209f2c4`, no message `data_0209d660`, `data_0209fc48==0`, not `chat_typing()`).
  Raw prev state is tracked every frame, so a button held through a menu closing is not a fresh press. It now returns the pad bits of a
  press it consumed; `port/tests/walk_window.cpp` `chat_pad_gate` clears them from the pad for that frame. world_party.h signature changed.
  Other D-pad Up/Down consumers: only PAUSEMP1's plate cursor (hal/menu_recipe_pause.cpp, acts only while the pause menu is up, which
  world_party now ignores) and the ROM itself. PLAYINT1 / WPARTY2 read no D-pad (WPARTY2 only added the rebinding of the same masks).
  So the two features never act in the same state; the frame an invite/leave press is taken, the game does not also see it.
- must 2, 7a89c25d: `port/hal/coop_tp.cpp` `on_ctl` accepts the toggle only from seat 0 (`kHostSeat`, the host seat per the launcher plan);
  `on_now` drops `heard` after 10 s of silence (host repeats every 3 s) or when seat 0 is no longer present.
- must 3, f935f51b: `world_party.cpp` (founder branch ~l.470): id = founder seat (high nibble) + nonce 1..15 (low nibble); two parties founded by
  different seats cannot share an id. A modified client can still forge an id (not addressed; the invite chain was not added).
- should, e8eecb72: `party.cpp` `unstick`: the "third body" count only counts bodies with `g_solid` (those `follow()` took through gate());
  the step is skipped unless my body is `on_ground` (the step goes into the pushback word the ROM integrates through its own collision).
  `SM64DS_ROOM_COLLISION` is off only for exactly "0" (party.cpp `read_env`).
- should, 1be29663: `vs_arena_ref.cpp` `cap_tongue_watch`: the outright character swap needs a CAP_TAKEN for me within the last 600 frames
  (`g_cap_taken_me_frame`, set in `on_cap_taken`). The retry loop before frame 60 is unchanged.
- should, af91f7d6: `coop_tp.cpp` `try_tp` compares `data_0209f220[0] & 0xff`; `tools/coop_tp_proof.py` takes `CTP_EXE` / `CTP_BASE_EXE`
  as required env vars (exits with a message, no path defaults).
Not done: FIX9A talker-class ROM-table check, StartTalk patch notes, nits (need game/ROM data).

## 3. Tests (Linux, python3)
- port/tools/relay test_referee.py: 24 tests, 501 checks, 0 failed.
- port/tools/relay test_relay_v2.py: 22 tests, 1121 checks, 0 failed.
- port/tools/lobby test_units.py: 1279 passed, 2 FAILED ("the match timeout returns the room to lobby", "with a failed timeout event").
  The same 2 fail on origin/port/l7-mpfold9 (1166 passed, 2 failed) and on origin/cloud/roomset1: pre-existing, not from this merge.
- port/tools/lobby test_security.py: 48 passed, 0 failed. test_client.py needs a mode; roomset1's own result reports `selftest` 218 passed (not rerun).
- `g++ -fsyntax-only` of world_party.cpp clean; party.cpp only the pre-existing linkage warning (windows.h files cannot be checked here).

## 4. Local agent must build and run (deduplicated)
Build the port (MSVC) plus smoke_settings / smoke_settings_live (tables grew 4 rows), and `python tools/port_refcheck.py`.
- FIX9A: coop 4-console new-save castle intro (Lakitu); sign/Toad talk with a closer friend.
- FIX9B/solo: identity (Luigi/Wario at join), star-get, pause, castle intro, base-vs-merged.
- FIX9C: 2P VS star near a drop; a tongued cap within 0.5 s (note the new CAP_TAKEN gate for the outright swap: verify the swap still happens).
- COOPTP1/2: `coop_tp_proof.py hub_tp course_none tp tplocked badge tpoff tpparty refuse_me refuse_same refuse_change alone` plus `tp:base badge:base`
  (set CTP_EXE / CTP_BASE_EXE); toggle forged from a non-host seat is ignored; host leaves -> toggle falls back to own env after ~10 s.
- PINVITE1/WPARTY2: pause menu in a world room, D-pad DOWN twice must NOT leave the party; Up/Down in message box / star select / cutscene /
  while typing do nothing; the consumed press does not also act in game; two consoles UP,UP form a party; world-party stars between members
  (`[pstar] rx ... -> ok`), non-member gets nothing, after DOWN,DOWN no more; old relay drops SYNX as "route"; rebinding the four keys in
  settings.json; party-room stars unchanged (party_stars_proof.py); two separate world parties never merge.
- PAUSEMP1: online Adventure pause menu input (pausescout1 could not find a cause by reading; needs a real-focus row).
- GRABPOSE1: VS Yoshi eat puffed cheeks / Wario grab carry layer on watchers (with PUFF=0 / CARRYLAYER=0 must-fail controls); MSVC build of vs_arena.cpp.
- GHOSTDOOR2: `SM64DS_GHOST_DOOR_PROBE=1`, basement key door + star door watched from ~150 / ~600 units; must-fail on mpfold9.
- PLAYINT1: apply the section-6 hook lines of PLAYINT1_DESIGN.md, build, run `--live-rows` L1..L11.
- ROOMSET1: launcher / lobby real launch of the room settings (not run here).
- Party unstick: three friends spawned on one spot near a wall/ledge.

## 5. Redeploy list
- Relay: `port/tools/relay/relay.py` (WPARTY2 hub SYNX forwarding) and `port/tools/relay/referee.py` (FIX9C) -- on Tango's word.
- Lobby: `port/tools/lobby/app/server.py` (ROOMSET1 room settings verb).
