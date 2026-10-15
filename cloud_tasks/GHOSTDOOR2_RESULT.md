# GHOSTDOOR2 result (key doors / star doors on the watcher)

Linux VM: code only, syntax-checked (`g++ -fsyntax-only port/hal/ghost_door.cpp`, clean). Not built, not run.

## Cause (evidence)
GHOSTDOOR1 only knew one door actor, 353 (`g_profile_DOOR`, port/hal/ghost_door.cpp `is_door`, `kDoorActorId`).
The ROM has two door classes in ov100:
* DOOR 353 (`Door`, include/Door.h): a `ModelAnim` at 0xd4 whose cursor (0x12c) is the open animation. Its key-lock
  (key hanging on the door) is the second model at 0x138, drawn from the door's own bone transforms
  (src/_ZN4Door6RenderEv.cpp), so it follows the cursor with no extra work. The key path is
  src/func_ov100_02144cf8.c (L210: `TryTalkToKeyDoor` -> node 0x021488e4 -> 02144bf4 -> `func_ov100_021451c4` plain open).
* STAR_GATE 354 (`daStarGate_c`, include/daStarGate_c.h, src/game/actors/d_a_star_gate.cpp): the big star / key-count
  doors. It has NO Animation and NO cursor at 0x12c (the object is only 0x118 bytes). It opens by ST_OPEN_CLOSE:
  `St_OpenClose_Main` (d_a_star_gate.cpp:~345) approaches `mHorzSpeed` (0x98) to -0x8a000 and writes the leaf
  offset to 0xa4/0xa8/0xac with `Vec3_RotateYAndTranslate`; `Render` (:~165) draws the model twice at
  pos +/- that offset. All of it runs only for the local player's `TryEnterStarDoor` (kind 0xa).

On the watcher, a friend's star-door entry therefore (a) never slid open (nothing writes 0xa4..0xac there) and
(b) the sender/watcher `nearest_door` skipped 354 and instead picked/drove whatever plain Door (353) was within
300 x 220 units, which is a door in the wrong spot (and read its cursor as the "frame"). That is the reported symptom.
The 353 key doors take the same path as other plain doors (cursor, pair leaf); nothing there differs, and the
key actor 0x11a spawned in 02144a38 is a local spawn for the opener's console only.

## Change
* port/hal/ghost_door.cpp: `nearest_any` (353 or 354, same metric) is used by sender and watcher. A 354 sends
  `128 + slide/2units` (slide = -mHorzSpeed, 0..69) in the existing vy low byte; a 353 sends its cursor clamped to
  <= 127, so the byte says the class. The watcher writes the gate's 0xa4/0xa8/0xac to the same rotated offset
  (`Vec3_RotateYAndTranslate`, pure math), no state callback is called, no pair leaf, original offsets restored on
  release, yield if the local game writes them. The UNSAFE control arm still only applies to a plain door.
* port/hal/ghost_door.h: `kGateBase`, wire comment. port/hal/comms_sync.cpp: the tag-fade door-frame test skips
  bytes >= 128 (a gate slide is not the 10..15 "through the door" frame).
* Modes: sender and watcher code is shared by the adventure ghosts and the party / coop solid bodies (both go through
  `gw_send` and `watcher_apply` at comms_sync.cpp:~3352); no mode-specific code.

## Rows for a local agent (2 consoles)
Build, then with `SM64DS_GHOST_DOOR_PROBE=1` on both:
1. B opens the basement key door (353, key variant) and a big star door (354) while A watches from ~150 and ~600 units:
   door opens in place; `[gwdoorw] gate=... slide=` climbs 0..138 for the star door; `[gwdoors] ... gate` lists the gates.
2. A's own level / position / camera unchanged; solo identity unchanged; tag still fades through a plain door.
3. MUST-FAIL: the same row on origin/port/l7-mpfold9 shows the star door staying shut on A and/or a neighbouring plain
   Door moving.
Open points: the exact 353 variants of the key doors were not enumerable here (`data_ov100_02148204` is ROM data); if a key
door still looks off after this, read the `[gwdoors]` list for its param and cursor/frames.
