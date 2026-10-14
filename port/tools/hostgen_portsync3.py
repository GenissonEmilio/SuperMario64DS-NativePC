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
ROWS[("PARK_SYMBOL", "cstd4")] = ["func_0206dac4", "func_0206dbf8", "func_0206de14", "func_0206dc4c"]
ROWS[("PARK_SYMBOL", "cstd6")] = ["func_0207037c"]
ROWS[("PARK_SYMBOL", "SoundEngine")] = ["func_020506fc", "func_020503a4", "func_020500a4",
                                        "func_0204fed0", "func_0204f194"]
ROWS[("PARK_SYMBOL", "Sound")] = ["func_020133bc"]
# dExtShadowModel_c::InitCuboid / InitCylinder are NOT parked: the old
# per-function files (src/_ZN11ShadowModel10InitCuboidEv.cpp) were compiled, and
# their ov000 data is bound by linkalias.py's same-address rule.
ROWS[("PARK_SYMBOL", "daFRing_c")] = ["_ZN9daFRing_c13InitResourcesEv"]
ROWS[("PARK_SYMBOL", "daObjFl_UkiKi_c")] = ["_ZN15daObjFl_UkiKi_c13InitResourcesEv"]
