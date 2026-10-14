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
