# COOPTP2 result (no build possible on the VM; unbuilt, unproven)

## Hub level list (sublevel ids, data_0209f2f8)
Evidence: kStage in port/hal/player_list.cpp and port_level_table in port/hal/level_boot.cpp.
- 1 castle grounds (main_castle)
- 2 castle 1F (castle_1f; ov010 "castle interior floor")
- 3 courtyard (main_garden)
- 4 basement (castle_b1)
- 5 second/top floor (castle_2f)
Not hub: 51 is "VS map 1" (ov059), 50 is the Rec Room, 41 none. There is no separate sublevel for an upper-most floor beyond castle_2f in either table.

## Changes
- port/hal/coop_tp.cpp: `in_hub()` (levels 1..5). Badge (`row_extra`) only in hub; click handler is installed only while coop + toggle + in hub (in a course the click is not consumed and goes to the game); `on_click` and `try_tp` re-check as a guard (one quiet, 5 s rate-limited chat line "TP only works in the castle" if a click races the handler drop).
- Landing: confirmed `LoadLevelNoReturn(level, 0, act, 0)` = entrance 0, the level's normal start; friend position is never read, only level+act. No other path places you near a friend.
- Coop only: `on_now()` needs `coop_world_active()` (room kind Coop); coop_tp is referenced nowhere else, so party/VS/world never reach it.
- port/tools/coop_tp_proof.py: clicker A now starts in the hub (me_warp 1@300) for tp, tplocked, tpoff, tpparty, badge, refuse_me; new rows hub_tp and course_none; refuse_same now asserts no badge/no TP in a course (the "already with" line is unreachable from a course).

## Rows a local agent must run
Build, then: `coop_tp_proof.py hub_tp course_none tp tplocked badge tpoff tpparty refuse_me refuse_same refuse_change alone`, plus `tp:base badge:base` controls. Verify the hub warp placing (me_warp "1@300") works in the harness.
