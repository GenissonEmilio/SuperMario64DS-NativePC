"""hostgen_portsync3 -- hostgen patch rows added at the 2026-10-14 main sync
(lane PORTSYNC3), on top of hostgen_portsync2.py. Same format: ROWS maps
(TABLE, stem) to the whole row. ASCII only."""

ROWS = {}

# func_02009e70 (the camera follow state). Main declares `Vector3 sp70 =
# line.GetClsnPos();` in the middle of the body and jumps past it with
# `goto L_A534` (twice). mwccarm allows it; MSVC refuses it (C2362: the jump
# skips an initialisation). sp70 is never read, so the declaration is scoped
# to its own block; the GetClsnPos() call stays.
ROWS[("MWCC_SPELLING", "func_02009e70")] = [
    ("    Vector3 sp70 = line.GetClsnPos();\n",
     "    { Vector3 sp70 = line.GetClsnPos(); (void)sp70; }\n"),
]

# ---- PARK_SYMBOL: folded bodies the port line never compiled ---------------
# Main's folds pulled these functions into TUs the port compiles wholesale.
# The 0.9.2 line (port/h8-rel092b) compiled none of them from src/ (checked
# against its own slice enumeration, tools/inferred_stub_guard.build_tu_set),
# and each one reaches DS-only runtime that the host never defines (OS mutex
# and interrupt helpers, the DS sound driver's hardware tables, ROM data in
# overlays the port does not mount). Parking restores exactly what ran before.
# src/ApproachAngle.cpp keeps ApproachAngle and UpdateAngle behind `#ifndef
# _MSC_VER` because they call __aeabi_idiv and "the host smoke build has no
# divider". The window targets call them (Eyerok, dMgJump3DMario, d_a_pg_rcer),
# so the guard is lifted for the generated copy and the divide goes through the
# port's own ds_idiv (hal/cstd_div.c), the DS divider with its x/0 = x rule.
ROWS[("MWCC_SPELLING", "ApproachAngle")] = [
    ("#ifndef _MSC_VER\n/* The host smoke build has no divider. These two call __aeabi_idiv. */\nextern \"C\" int __aeabi_idiv(int a, int b);",
     "/* PORTSYNC3: guard lifted for the host; the divide is ds_idiv (hal/cstd_div.c). */\nextern \"C\" int ds_idiv(int a, int b);\n#define __aeabi_idiv ds_idiv"),
    ("        angle = angle + step;\n    }\n}\n#endif\n",
     "        angle = angle + step;\n    }\n}\n"),
]
ROWS[("PARK_SYMBOL", "cstd4")] = ["func_0206dac4", "func_0206dbf8", "func_0206de14", "func_0206dc4c", "func_0206df14", "func_0206e06c"]
ROWS[("PARK_SYMBOL", "cstd6")] = ["func_0207037c"]
ROWS[("PARK_SYMBOL", "SoundEngine")] = ["func_020506fc", "func_020503a4", "func_020500a4",
                                        "func_0204f194", "func_020507e0"]
ROWS[("PARK_SYMBOL", "Sound")] = ["func_020133bc", "func_020134c8"]
# dExtShadowModel_c::InitCuboid / InitCylinder are NOT parked: the old
# per-function files (src/_ZN11ShadowModel10InitCuboidEv.cpp) were compiled, and
# their ov000 data is bound by linkalias.py's same-address rule.
# daFRing_c / daObjFl_UkiKi_c InitResources were parked on the premise that the old
# build did not compile them; it did (as BowserShockwaves / LavaPlank), so they run.

# daDoor_c::InitResources and daTree_c::Behavior: PORTSYNC2 parked main's bodies
# on the premise of a flat host copy that does not exist in the tree (only
# daTor_c's, port/unmatched/Tornado_HostSites.cpp, does). The hostgen repairs the
# old per-function stems carried are keyed on the TU stem now, so main's member
# IS the repaired body: it is un-parked, and the two ledger rows are R faces
# (flat name -> member) instead of F faces (member -> flat).
ROWS[("PARK_SYMBOL", "daDoor_c")] = None
ROWS[("PARK_SYMBOL", "d_a_tree")] = None

# G3i.cpp (main's fold of the G3i matrix writers). LookAt_ binds a local
# `mtx` to the MTX_MULT_3x3 port and writes the view matrix through it; the
# fold put PerspectiveW_ in the same TU, whose `int *mtx` OUTPUT parameter is
# tested (`if (mtx)`) and indexed. hostgen's pointer rewrite works on names
# per TU text and gives up on a name with any non-deref use, so the whole
# `mtx` family was left plain: the title's LookAt_ latched into mapped memory
# and the geometry engine never got the view matrix (title bottom screen drew
# 8.0 units off, the buttons and wrench fell out of the picture). The row
# gives the register-bound local a name of its own.
ROWS[("MWCC_SPELLING", "G3i")] = [
    ("    volatile int *mtx;\n",
     "    volatile int *lookat_port;\n"),
    ("        mtx = (volatile int *)0x400045c;\n"
     "        *mtx = right.x;\n        *mtx = up2.x;\n        *mtx = forward.x;\n"
     "        *mtx = right.y;\n        *mtx = up2.y;\n        *mtx = forward.y;\n"
     "        *mtx = right.z;\n        *mtx = up2.z;\n        *mtx = forward.z;\n",
     "        lookat_port = (volatile int *)0x400045c;\n"
     "        *lookat_port = right.x;\n        *lookat_port = up2.x;\n        *lookat_port = forward.x;\n"
     "        *lookat_port = right.y;\n        *lookat_port = up2.y;\n        *lookat_port = forward.y;\n"
     "        *lookat_port = right.z;\n        *lookat_port = up2.z;\n        *lookat_port = forward.z;\n"),
    ("        *mtx = tx;\n        *mtx = ty;\n        *mtx = tz;\n",
     "        *lookat_port = tx;\n        *lookat_port = ty;\n        *lookat_port = tz;\n"),
]
