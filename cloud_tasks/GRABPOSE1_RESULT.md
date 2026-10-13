# GRABPOSE1 result

Linux VM: nothing here was built or run. Code is in `port/hal/vs_arena.cpp` only (puppet driver). No wire, relay or referee change.

## 1. Eaten by a remote Yoshi: cheek puff

ROM evidence
- `src/_ZN6Player18St_YoshiPower_MainEv.cpp` case 1: at body-anim frame 0xc of the swallow the Yoshi sets `mUseAltBodyModel` (+0x714) = 1. It stays set while the mouth is full; `St_YoshiPower_Cleanup` (`src/_ZN6Player21St_YoshiPower_CleanupEv.cpp`), `Player::Hurt` (`src/_ZN6Player4HurtERK7Vector3j5Fix12IiEjjj.cpp:63`) and the drop path (`src/actors/Player.cpp:5644`) clear it.
- `Player::GetBodyModelID` (`src/actors/Player.cpp:1241`): for character 3 (Yoshi) with +0x714 set it answers model 3 + 4, the puffed-cheek model. `Behavior` (`src/_ZN6Player8BehaviorEv.cpp:293`) indexes the same slot.
- A puppet runs no player states, so +0x714 was never set: the eater drew as if its mouth were empty.

What the puppet receives: the eater's own SYNS `held` (= seat + 1 of the player in its mouth, from mObjInMouth +0x360, `vs_arena.cpp` sampling). No new field.

Change (`pup_drive`, after the look swap/tongue block, before SetAnim): for a Yoshi puppet, +0x714 = 1 while its stream's `held` names a player seat other than itself; 0 otherwise. On the victim's console it also requires my body's +0xd0 (mEatingPlayer) to name that puppet (the referee's EATEN has applied), so a DENY never puffs. Dropped as soon as `held` clears (spit / swallow / escape / release). Every watcher derives it from the same stream. `PupDraw` gained `held`. SetAnim then installs the pose on the model GetBodyModelID now returns (the anim id differs on the new model, so the ROM call lands the cursor).
Knob: `SM64DS_PUPPET_PUFF=0` = base behaviour. Log line: `[puppet] fN seat S cheeks puffed|relaxed`.

## 2. Held by a remote Wario

ROM evidence
- `func_ov002_020dacb4` (`src/func_ov002_020dacb4.c`) is the ROM's only grab entry: a player victim (class 0xbf) goes to the same light-hold state as an item; holder anim 0x2f (`St_HoldLight_Init`) plus the upper-body carry layer table entry `data_ov002_020ff480[0xc4 + char]` (0x224 only when the carried actor has flag 0x8000; the Player profile flags are 0x2, `symbols/profile_reconstruction_registry.json`, so a player is never "heavy"). So the ROM has no separate holder pose for players; the player-specific looks are the victim's: `St_Grabbed_Init` anim 0x71 and `func_ov002_020e3f90` (`src/func_ov002_020e3f90.c`), the holder-is-0xbf branch that dangles/tilts the victim off the holder matrix.
- `func_ov002_020e4990` (`src/func_ov002_020e4990.c`) is what turns the carry layer on: layer 8 off, then on while +0x358 != 0 and the body anim is one of the carry anims (0x2f, 0x33, 0x6c...). `func_ov002_020e4bb8` calls it between the placement (`020e444c`) and the pose (`020e4768`). The puppet driver replays 444c and 4768 but never 4990, so `puppet_carry_overlay` (vs_arena_grab.cpp, commit 663f0a66) loaded the layer and nothing ever enabled it: a remote holder did not raise the carry arms, and the carried body rode a Wario that was not visibly holding it.

Change (`pup_drive`): call `func_ov002_020e4990(a)` between `020e444c` and `020e4768`, exactly the ROM order, for every puppet (it is a no-op without +0x358). The victim-side look (St_Grabbed 0x71 + the 020e3f90 player branch, +0x358/+0x35c/0x4000 links) was already driven through the ROM's own code by vs_arena_grab and is unchanged; physics and who-holds-whom untouched.
Knob: `SM64DS_PUPPET_CARRYLAYER=0` = base behaviour.

Not proven: I could not run the game, so I did not see Tango's exact "held like an item" frame. If the carry arms were already visible there, the remaining suspects are in order (a) holder puppet +0x63c (anim id << 2) stale so the 020e3f90 swing branch is chosen wrongly, (b) victim flag 0x4000 dropped (hold_probe in vs_arena_grab prints flags/anim every 10 frames with SM64DS_GRAB_LOG=1).

## Rows for a local agent (2 consoles VS, build with the change, then SM64DS_PUPPET_PUFF=0 / SM64DS_PUPPET_CARRYLAYER=0 as the must-fail base)
1. Seat A Yoshi eats seat B. A's console: ROM-native puff (unchanged). B's console: A's puppet shows the puffed-cheek model once EATEN lands, `[puppet] ... cheeks puffed`; spit / escape: `cheeks relaxed`. Third watcher the same. With PUFF=0 the puppet's cheeks stay flat (must fail).
2. A denied eat (DENY): B's console never logs `cheeks puffed`.
3. Seat A Wario stuns-then-grabs B (GRAB_LOG=1). B's console: A's puppet shows the arms-raised carry layer while holding, B hangs in St_Grabbed pose; A's console: B puppet in the dangle pose; third watcher both. With CARRYLAYER=0 the holder puppet's arms stay down (must fail). Throw / swing / break unchanged.
4. `python tools/port_refcheck.py` (no src/ file touched, expected clean) and an MSVC build of port/hal/vs_arena.cpp.
