# FOLD9B2 result (cloud review's must-fixes on the pre-merge)

Branch cloud/fold9b-pre2, base origin/cloud/fold9b-pre. Linux VM: no game build; python suites and `g++ -fsyntax-only` where windows.h is not needed.
One commit per item (hooks are small `hook:` commits), author Tango.

## 1. Changes (file:line)
1. GHOSTDOOR2 must-fix 1+2, `270dfdbd`, `port/hal/ghost_door.cpp`:
   - externs `data_020a0ebc` (the matrix) and `data_ov100_02148948[3]` (:30-31).
   - `gate_offset` (:125-129) passes the ROM matrix and rotates `{-slide, data_ov100_02148948.y, .z}` like `St_OpenClose_Main`
     (the sign of x is the existing `mHorzSpeed` convention: `slide = -mHorzSpeed`).
   - `[gwdoors] pick` probe (`probe_pick`, :311, called from `watcher_apply` :372 when `SM64DS_GHOST_DOOR_PROBE` is set): for every door a friend
     opens it logs the friend's position, the picked actor (pointer, class id at +0x0c, param / key-door variant at +8, position), then every
     door-like actor (353 or 354) within 600 units, tagged `PICKED` / `other`, with its dx,dz to the friend. One local row (basement key door,
     probe on) decides FIELD9 item 14: if the key door's class/variant is not the one marked PICKED, the pick is the bug. The existing
     `[gwdoors]` level listing and the sender `[gwdoor]` lines are unchanged. Item 14 stays open until that row.
   - `g++ -std=c++17 -fsyntax-only` clean.
2. PLAYINT1 note 1, `ec88a2a7`, `port/hal/party_hits.cpp:~324`: `port_party_hits_frame` returns until `comms_hub_active()` and
   `comms_hub_state() == kHubLive`, and only then latches `g.on` / registers handlers (like party_stars' `hub_live()`).
3. PLAYINT1 note 2 + hooks (one commit each):
   - hook 3 `35ea5eb9`: `party_hits.cpp` registers its SYNB sub-type with `coop_ctl_register` (coop room only) with an adapter `coop_sub`
     (:306). The real dispatcher (coop_world.cpp) passes the PAYLOAD after the 12-byte head, not the datagram, so `on_coop_ctl` now takes the
     payload (the design's `case 0x40:` line could not be used as written). No edit of the dispatcher.
   - hooks 1+2 `43b2eca4`: `port/tests/walk_window.cpp:633` (declaration), `:18260` (call right after `port_party_stars_frame();`).
   - hook 5 `4099f009`: `port/CMakeLists.txt:11630` `hal/party_hits.cpp` in `SLICE_COMMS_SOURCES` (same list as party_stars.cpp and vs_arena.cpp, so
     `vs_arena_state_index` links).
   - hook 4 `b1caf4ff`: `port/hal/comms_sync.cpp:~3503` after `party::follow`, party bodies only: writes +0x84 only while a squash runs and puts 0x1000 back once
     when it ends (a per-body flag), so a normal body's scale is never touched. NOT syntax-checked (windows.h).
   - hook 6 NOT applied: the launcher that exports `SM64DS_ROOM_INTERACT` / `SM64DS_ROOM_DAMAGE` from the plan's `settings` is not in this repo
     (only the lobby, which already sends them). Local agent / launcher owner. Unset means interact 1, damage 0 (a launcher that does not export
     it turns interactions ON in party/coop by default, see REVCLOUD1 open question).
   - SYNB COLLISION FOUND AND FIXED: COOPTP's host toggle uses SYNB sub-type **0x40** (`coop_tp.cpp:36 kSubToggle`, registered via
     `coop_ctl_register(0x40,0x40)`). `coop_ctl_register` refuses an overlapping range, so PLAYINT1 on 0x40 would have failed to register in a
     coop room (coop_tp registers at static init) and coop would never receive claims. PLAYINT1 is moved to **0x41** (`party_hits.h kCoopCtlSubtype`,
     design doc and proof text updated). Every SYNB user found by grep of all `coop_send_ctl` / `coop_ctl_register` / `hub_coop_encode(kHubTagCoopCtl`:
     0x01-0x05 world core, 0x06-0x0b coop_actors, 0x0c-0x11 coop_switch, 0x12-0x17 coop_spawn, 0x1e star_popup, 0x20-0x2f coop_save,
     0x30-0x34 (0x30-0x3f) coop_boot, 0x40 coop_tp, now 0x41 party_hits. 0x41 is free. (SYNW 0x40-0x4f in coop_spawn.h is another tag.)

## 2. D-pad Up / Down: who reads it, in which state
No other reader was found (grep of the port for D-pad masks / MENU_PAD_UP / DOWN: world_party, menu_recipe_pause, host_settings/settings_live
only; PLAYINT1 (party_hits) and COOPTP read no D-pad; chat and the player list use the keyboard/mouse).
| state | D-pad UP | D-pad DOWN |
|---|---|---|
| World room, walking in a level with control (in a level, not leaving, no pause `data_0209f2c4`, no message `data_0209d660`, `data_0209fc48==0`, not typing) | world_party INVITE / ACCEPT (default pad mask 0x0001, or the PadPartyInvite bind); the pad bit is consumed (masked from the game that frame) | world_party LEAVE (0x0002 / PadPartyLeave; press twice) ; consumed |
| Pause menu up, ROM pause, OneScreenMenus off (`menu_pause_rom_pad`, needs `data_0209f2c4`, state 1/3/5/0xd) | plate cursor up (PAUSEMP1); world_party sees `data_0209f2c4` and takes nothing | plate cursor down; world_party takes nothing |
| Pause menu, tracker view (`menu_pause_hook_pad`) | swallowed (not sent to the ROM cursor) | swallowed |
| Message box / star select / cutscene / transition / typing in chat | ROM only (world_party blocked by `input_live`) | ROM only |
| Party room, any state | ROM only (world_party needs a WORLD room) | ROM only |
| Coop room | ROM only (world_party world rooms only) | ROM only |
| PLAYINT1 / COOPTP / ROOMSET | not a reader | not a reader |
The two features that read the pad (world_party, PAUSEMP1 cursor) are gated on mutually exclusive states (pause up vs. not). settings_live
`pad_reserved` lets the d-pad be bound only for the two party rows. A user who rebinds the party pad to another button changes nothing above.

## 3. Tests (Linux, python3)
- relay `test_referee.py`: 24 tests, 501 checks, 0 failed.
- relay `test_relay_v2.py`: 22 tests, 1121 checks, 0 failed.
- lobby `test_units.py`: 1279 passed, 2 FAILED ("the match timeout returns the room to lobby", "with a failed timeout event"): the same 2 as on
  mpfold9 / fold9b-pre, not from this lane.
- lobby `test_security.py`: 48 passed, 0 failed.
- `port/tools/party_hits_proof.py`: 87 rows, 0 failed (it models the wire, not the sub-type constant).
- `g++ -fsyntax-only`: ghost_door.cpp and party_hits.cpp clean; comms_sync.cpp / walk_window.cpp need windows.h (not checked).

## 4. Local rows (FOLD9B's list + these, deduped)
Build the port (MSVC) plus smoke_settings / smoke_settings_live (tables grew 4 rows), `python tools/port_refcheck.py`; confirm party_hits.cpp links.
- FIX9A: coop 4-console new-save castle intro (Lakitu); sign/Toad talk with a closer friend.
- FIX9B/solo: identity (Luigi/Wario at join), star-get, pause, castle intro, base-vs-merged.
- FIX9C: 2P VS star near a drop; a tongued cap within 0.5 s (the CAP_TAKEN gate: the outright swap still happens).
- COOPTP1/2: `coop_tp_proof.py hub_tp course_none tp tplocked badge tpoff tpparty refuse_me refuse_same refuse_change alone` + `tp:base badge:base`
  (CTP_EXE / CTP_BASE_EXE); toggle forged from a non-host seat ignored; host leaves -> falls back to own env after ~10 s. NEW: toggle still works
  with party_hits built in (0x40 / 0x41 both registered; `[phit] coop SYNB 0x41 registered` in the log).
- PINVITE1/WPARTY2: pause menu in a world room, D-pad DOWN twice must NOT leave the party; Up/Down in message box / star select / cutscene / typing do
  nothing; a consumed press does not also act in the game; two consoles UP,UP form a party; world-party stars between members (`[pstar] rx ... -> ok`),
  non-member gets nothing; old relay drops SYNX as "route"; rebinding the four keys; party-room stars unchanged (party_stars_proof.py); two parties never merge.
- PAUSEMP1: online Adventure pause menu input with a real-focus window.
- GRABPOSE1: VS Yoshi eat puffed cheeks / Wario grab carry layer on watchers (PUFF=0 / CARRYLAYER=0 must-fail controls); MSVC build of vs_arena.cpp.
- GHOSTDOOR2: `SM64DS_GHOST_DOOR_PROBE=1`, basement key door + a star door watched from ~150 / ~600 units: read the `[gwdoors] pick` rows (PICKED vs other,
  class, variant) and `[gwdoorw] gate=... slide=` (the star door opens in place with the real y/z); must-fail on mpfold9. Decides FIELD9 item 14.
- PLAYINT1: build; `party_hits_proof.py --live-rows` L1..L11 (L10 coop over SYNB 0x41); interactions come on when the hub goes live (log `[phit] on`),
  not only if the hub was live on the very first frame; stomp squash visible on a party body and restored; verify launcher exports
  SM64DS_ROOM_INTERACT / DAMAGE (hook 6).
- ROOMSET1: launcher / lobby real launch of the room settings.
- Party unstick: three friends spawned on one spot near a wall/ledge.

## 5. Redeploy list
- Relay: `port/tools/relay/relay.py` (SYNX in KIND_HUB sessions, WPARTY2) and `port/tools/relay/referee.py` (FIX9C), on Tango's word.
- Lobby: `port/tools/lobby/app/server.py` (ROOMSET1 room-settings verb, v6 settings).
- No relay change for PLAYINT1 (party SYNH already routed; coop rides SYNB 0x41).
