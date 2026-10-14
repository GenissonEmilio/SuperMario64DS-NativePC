"""hostgen_portsync2 -- hostgen patch rows re-derived after the decomp main merge.

Main folded one-function files into production TUs and rewrote many bodies, so
the anchors of several hostgen.py rows (written against the old one-function
files) no longer matched. Each row below was re-derived against the merged
src/ text: re-anchored where the defect still exists, deleted (None) where
main's text no longer has it. hostgen.py loads this file at import
(_load_rederived) and its rows replace the entries of the same (TABLE, stem).
The comment above each row says what changed and why.
"""

# ======================================================================
# part_00
# PORTSYNC2 re-derived hostgen rows, part_00.
# Each entry REPLACES the whole (TABLE, stem) row. ASCII only.
ROWS_00 = {}

# ---- dPathLiftActor_c ------------------------------------------------------
# DS_DIV: the path-follow tick's two angle-step divisions by the per-segment
# frame count (0 when a segment is shorter than one frame of travel; ROM idiv
# answers x/0 = 0, x86 faults). Main renamed dv -> segFrames, ad1/ad2 ->
# pitchDiff/yawDiff and folded StatePath into the TU; same two divisions.
ROWS_00[("DS_DIV", "dPathLiftActor_c")] = [
    ("pitchDiff / segFrames", "ds_idiv(pitchDiff, segFrames)"),
    ("yawDiff / segFrames", "ds_idiv(yawDiff, segFrames)"),
]

# ---- dMgTrmpln2Mario_c -----------------------------------------------------
# MEMBER_REDECL: delete out-of-class member redeclarations (mwcc-tolerated,
# C2761 to MSVC). Two old stems folded here, so the row arrived doubled; one
# replace() of the System::New line removes both copies in the TU (lines in
# both folded bodies). DROPPED the Animation::Advance item: main replaced it
# with a local `struct dExtFrameCtrl_c { void Advance(); ... }` whose method is
# declared only in-class -- no out-of-class redeclaration is left.
ROWS_00[("MEMBER_REDECL", "dMgTrmpln2Mario_c")] = [
    ("System* System::New(unsigned, unsigned, Fix12, Fix12, Fix12, const Vector3_16f*, Callback*);\n", ""),
]

# ---- dScMgPachinko_c -------------------------------------------------------
# MG_PMF_CALL: the three open-coded member-pointer dispatches (tables
# 02142624 / 02142694 / 02142644, arity 1) call a DS code word raw; route each
# through port_mg_pachinko_call1. Three old stems folded here with the same
# call line, so the merged row carried one anchor three times (the first
# replace() takes all three and the later items then fail). Main also renamed
# 020fc7d0's record `e` -> `rec` (and the row pointer r5 -> row), so the code
# word there is rec->a. Each anchor now carries its body's own tail so it is
# unique and names its own record variable.
ROWS_00[("MG_PMF_CALL", "dScMgPachinko_c")] = [
    # func_ov006_020fc7d0 (table 02142624, 2 rows of 0x1c)
    ("      ((void(*)(void*,int))fn)(obj,i);\n    }\n    i++;\n    row+=0x1c;\n  }while(i<2);",
     "      port_mg_pachinko_call1(obj, (unsigned)rec->a, adj, i);\n    }\n    i++;\n    row+=0x1c;\n  }while(i<2);"),
    # func_ov006_020fda7c (table 02142694, 30 rows of 0x38)
    ("      ((void(*)(void*,int))fn)(obj,i);\n    }\n    i++;\n    r5+=0x38;\n  }while(i<30);",
     "      port_mg_pachinko_call1(obj, (unsigned)e->a, adj, i);\n    }\n    i++;\n    r5+=0x38;\n  }while(i<30);"),
    # func_ov006_020fe248 (table 02142644, 48 rows of 0x38)
    ("      ((void(*)(void*,int))fn)(obj,i);\n    }\n    i++;\n    r5+=0x38;\n  }while(i<48);",
     "      port_mg_pachinko_call1(obj, (unsigned)e->a, adj, i);\n    }\n    i++;\n    r5+=0x38;\n  }while(i<48);"),
]

# ---- dMgJump3DMario_c ------------------------------------------------------
# DATA_C_LINKAGE: DROPPED. The row gave C linkage to nine mState records and
# argument pairs that the old TU declared at BLOCK scope (as PmfRecord/Pair)
# with no linkage-specification. Main moved every one of them (0213b020..
# 0213b078 as Jump3DState, plus the rest of the TU's data) into the TU's own
# file-scope `extern "C" { ... }` block above the first function, and no
# block-scope extern is left after it; the anchor `struct Pair` is gone with
# them. The names already come out undecorated.
ROWS_00[("DATA_C_LINKAGE", "dMgJump3DMario_c")] = None

# ---- dMgState_c ------------------------------------------------------------
# VIRTUAL_CALL: DROPPED. The row (folded from func_ov004_020b6b40 and
# func_ov004_020b6c9c) retyped a raw C call through vtable word 0x13 as a
# __fastcall(this, 0, int) so the host thunk mb_v19's `ret 4` was not cleaned
# twice by a cdecl caller. Main replaced the raw dispatch in both bodies with a
# real virtual call, `g->OnTurnIntoEgg(mState)`, through the TU's `struct
# Base` view (OnTurnIntoEgg(int) declared as slot 19). MSVC emits that as a
# __thiscall: this in ecx, one stack argument, callee cleans -- the exact
# contract of mb_v19's __fastcall (ecx, edx, int) ret 4. No raw call remains.
ROWS_00[("VIRTUAL_CALL", "dMgState_c")] = None

# ---- dBgActor_c ------------------------------------------------------------
# VIRTUAL_CALL (folded from func_ov002_020eeeb8 / func_ov002_020ef070, so the
# declaration item arrived twice). The row did two things:
#  (1) convention: retyped the raw cdecl calls through vtable words 24
#      (OnKicked, +0x60) and 23 (OnAttacked2, +0x5c) as __fastcall(a, 0,
#      actor) to match the seated ret-4 veneers. Main now spells both as real
#      virtual calls, `a->OnKicked(*(dActor_c *)actor)` and
#      `a->OnAttacked2(*(dActor_c *)actor)`, which MSVC emits as __thiscall
#      (ecx receiver, one stack argument, callee cleans) -- the veneer's own
#      contract, the same spelling this TU's unpatched OnAttacked1 /
#      OnGroundPounded calls already use. That half is DROPPED.
#  (2) host seam: the quarantine net's receiver latch
#      port_actor_interaction_begin/end around the call, so a fault inside
#      the receiver freezes the receiver, not the player. Not part of the
#      convention fix and not something main's text provides, so it is
#      RE-ANCHORED around the two real virtual calls (each unique in the TU);
#      the declaration anchor `extern short data_02082214[];` is still unique
#      and still inside the TU's extern "C" block (the seam is extern "C" in
#      port/unmatched/func_02043fdc_hostcopy.cpp), now listed once.
ROWS_00[("VIRTUAL_CALL", "dBgActor_c")] = [
    ("extern short data_02082214[];",
     "extern short data_02082214[];\n"
     "/* hostgen VIRTUAL_CALL host seam: the quarantine net's receiver\n"
     "   latch (port/unmatched/func_02043fdc_hostcopy.cpp). */\n"
     "void *port_actor_interaction_begin(void *receiver);\n"
     "void port_actor_interaction_end(void *prev);"),
    ("                    a->OnKicked(*(dActor_c *)actor);",
     "                    /* hostgen VIRTUAL_CALL host seam: receiver latch\n"
     "                       around Actor::OnKicked (slot 24). See the table. */\n"
     "                    void *port_prev_recv =\n"
     "                        port_actor_interaction_begin(a);\n"
     "                    a->OnKicked(*(dActor_c *)actor);\n"
     "                    port_actor_interaction_end(port_prev_recv);"),
    ("                    a->OnAttacked2(*(dActor_c *)actor);",
     "                    /* hostgen VIRTUAL_CALL host seam: receiver latch\n"
     "                       around Actor::OnAttacked2 (slot 23). See the table. */\n"
     "                    void *port_prev_recv =\n"
     "                        port_actor_interaction_begin(a);\n"
     "                    a->OnAttacked2(*(dActor_c *)actor);\n"
     "                    port_actor_interaction_end(port_prev_recv);"),
]

# ---- dScMgCup_c ------------------------------------------------------------
# HOST_COPY_PARK: dScMgCup_c_classInit is still host-copied by
# port/unmatched/MgCup_Factory.cpp (still on port/CMakeLists.txt's link line),
# so the TU's own definition must stay parked or it is a duplicate symbol.
# Main rewrote the body onto named members (scene->mSysTracker / mModel / mFx /
# mCup instead of scene + 0x53e8 ...), so the closing anchor moved; the opening
# anchor is unchanged. Note for whoever owns the host copy: main's body now
# passes the object to _ZN11dScMgBase_cC2Ev(scene) explicitly, which was the
# PORT_HOST_ABI ruling the host copy exists for -- it may be retirable, but
# that is a port/CMakeLists.txt decision, not this row's.
ROWS_00[("HOST_COPY_PARK", "dScMgCup_c")] = [
    ('extern "C" void *dScMgCup_c_classInit()\n{\n',
     "/* hostgen HOST_COPY_PARK: port/unmatched/MgCup_Factory.cpp defines\n"
     "   this factory under a standing PORT_HOST_ABI ruling (the base\n"
     "   constructor call with no argument), so a second definition here is\n"
     "   a duplicate symbol. Parked, not deleted. See the HOST_COPY_PARK\n"
     "   table in tools/hostgen.py. */\n"
     "#if 0  /* hostgen HOST_COPY_PARK: dScMgCup_c_classInit */\n"
     'extern "C" void *dScMgCup_c_classInit()\n{\n'),
    ("        __cxa_vec_ctor(scene->mCup, 3, 8,\n"
     "                      (void *)func_0203d738,\n"
     "                      (void *)NullDestructor_0203d47c);\n"
     "    }\n    return scene;\n}\n",
     "        __cxa_vec_ctor(scene->mCup, 3, 8,\n"
     "                      (void *)func_0203d738,\n"
     "                      (void *)NullDestructor_0203d47c);\n"
     "    }\n    return scene;\n}\n"
     "#endif  /* hostgen HOST_COPY_PARK: dScMgCup_c_classInit */\n"),
]
# UNINIT_LOCAL: DROPPED. The row initialised func_ov006_020def80's `int cup`
# to `i` because two of the three paths reached the shared epilogue without
# assigning it (mwcc's r1 happened to hold i). Main's body now assigns
# `cup = i;` on all three paths before `goto epilogue` / the label, so the
# local is never read uninitialised; the anchor `char *row = c + i;` is gone
# too (the parameter is `raw` now).
ROWS_00[("UNINIT_LOCAL", "dScMgCup_c")] = None

# ---- dScMgD3DBase_c --------------------------------------------------------
# LEDGER_PARK: port/faces_sync.txt still carries the F row for
# ?OnKicked@dScMgD3DBase_c@@UAEHXZ (the face DEFINES the member and forwards
# to the flat _ZN14dScMgD3DBase_c8OnKickedEv, supplied by
# port/unmatched/MgD3DBase_DeclConflict.cpp), so the TU's own member body is
# still a duplicate symbol and stays parked. Main renamed the body's local
# `self` -> `raw` and replaced the "ROM ordinal 11" banner after it with
# `// @symbol _ZN14dScMgD3DBase_c8OnPushedEv`; both anchors re-spelled on
# that text (each unique: one definition, one @symbol line).
ROWS_00[("LEDGER_PARK", "dScMgD3DBase_c")] = [
    ("int dScMgD3DBase_c::OnKicked()\n{\n    char *raw = (char *)this;\n",
     "/* hostgen LEDGER_PARK: port/faces_sync.txt already defines this\n"
     "   member as a face forwarding to the flat ROM name, so a\n"
     "   second definition here is a duplicate symbol. Parked, not deleted.\n"
     "   See the LEDGER_PARK table in tools/hostgen.py. */\n"
     "#if 0\n"
     "int dScMgD3DBase_c::OnKicked()\n{\n    char *raw = (char *)this;\n"),
    ("    return 1;\n}\n\n// @symbol _ZN14dScMgD3DBase_c8OnPushedEv\n",
     "    return 1;\n}\n"
     "#endif  /* hostgen LEDGER_PARK: dScMgD3DBase_c::OnKicked */\n"
     "\n// @symbol _ZN14dScMgD3DBase_c8OnPushedEv\n"),
]

# ---- dScMgSound_c ----------------------------------------------------------
# HOST_COPY_PARK: DROPPED. The row bracketed the TU's flat-named definition
# `void _ZN12dScMgSound_c19func_ov006_0211ba88Ei(char *base, int idx)` because
# port/unmatched/MgSound_ShadowSlot35.cpp defines that same flat name (LNK2005).
# Main rewrote the body as a real member, `void
# dScMgSound_c::func_ov006_0211ba88(int slot)`, which MSVC emits under its own
# decorated name (?func_ov006_0211ba88@dScMgSound_c@@...), and no
# port/faces_sync.txt row defines or bridges that member -- so the two
# definitions no longer collide, and port/unmatched/MgSound_StateDispatch.cpp
# still seats the host copy's flat name in the 02142df8 slot-4 entry, so the
# ruling's "the host copy is the body that runs" is unchanged. (Main's text
# also no longer has the four defects the host copy lists: Virtual8C is a
# real slot-35 virtual, the reduction is a do/while, the n == 0 test is
# outside the slot >= limit arm, and stores go through typed mNotes fields.)
ROWS_00[("HOST_COPY_PARK", "dScMgSound_c")] = None

# ---- daBombking_c ----------------------------------------------------------
# HOST_COPY_PARK: DROPPED. The row bracketed the TU's flat
# `extern "C" int func_ov078_02123f1c(CView* c)` because
# port/unmatched/KingBobOmb_HeldHealthCheck.cpp host-copies that ROM body
# (GetHealth's receiver rode r0). Main rewrote it as a real member,
# `int daBombking_c::func_ov078_02123f1c()`, emitted under MSVC's decorated
# member name; the host copy defines the flat _ZN12daBombking_c19func_ov078_
# 02123f1cEv, port/hal/actor_overlays.cpp's pmfb7_ov078_02123f1c dispatches
# to that flat name, and port/faces_sync.txt has no row for the member, so
# nothing collides and the host copy is still the body that runs. (The
# receiver-less `_ZN6Player9GetHealthEv()` call is still in main's member body;
# it is simply not the body the port dispatches to.)
ROWS_00[("HOST_COPY_PARK", "daBombking_c")] = None
# HEADER_SHADOW (folded from _ZN12daBombking_c19func_ov078_02124cf4Ev): the
# row hides decl_common.h's declarations of names the TU declares
# incompatibly. Its two old names are no longer a conflict in main's text:
# decl_common.h no longer declares _ZN12daBombking_c19func_ov078_02125c24Ei
# (the TU calls it as a member now), and its `func_0200fa8c(void*, int)`
# agrees with the TU's `(void *c, int a)`. Main's text carries one new
# collision of the same kind across that include instead:
# include/daBombking_c.h:177 declares `extern int _ZTV12daBombking_c[];` with
# C++ linkage and decl_common.h:319/369 redeclare it inside its extern "C"
# block -- "different language linkage", a hard error to MSVC/clang-cl. The
# TU itself never names the vtable, so hiding decl_common.h's copy changes
# nothing it references. RE-DERIVED to shadow that name only.
ROWS_00[("HEADER_SHADOW", "daBombking_c")] = ("decl_common.h", ("_ZTV12daBombking_c",))

# ---- dScMgPanel_c ----------------------------------------------------------
# WHAT CHANGED IN MAIN, and why every Panel row below changes mechanism rather
# than just anchor text. At the 2026-09-13 sync the TU's dispatchers were
# extern "C" functions over BLOCK-SCOPE `extern` declarations of the state
# tables (021427bc, 021427ec, 02142820, 02142840, 02142860, 02142888), which
# bound to the C-named ov006 .data mount: the words were the cartridge's DS
# addresses (021427bc / 02142860 seated with host bodies by
# port_mg_panel_states_seat), hence the DS-address seams. Main now makes
# every dispatcher a real `dScMgPanel_c::` member and DEFINES all six tables
# in the TU (`PanelStateFn data_ov006_02142888[8] = {
# &dScMgPanel_c::func_ov006_021071d4, ... }`, file scope, C++ linkage, under
# `typedef void (dScMgPanel_c::*PanelStateFn)()` / `PanelStateFnI` over the
# complete class). In the host build those definitions are this TU's own
# decorated symbols holding MSVC member pointers to this TU's own host
# members (8 bytes {code, adj} under /vmg /vmm), and every block-scope
# extern in the TU is C++-linkage too, so it binds to them, not to the mount.
# The code word every site reads is therefore a HOST address. Routing it
# through port_mg_panel_call0/1 would now be the regression this table warns
# about: panel_try_0/1 are keyed on DS addresses and port_mg_call0/1's
# mg_is_ds_code refuses a host word ("nothing was called").
#
# REVIEW FLAG: this rests on the TU's table definitions being the ones the
# generated copy uses (they are: no DATA_C_LINKAGE / ALTNAME / --extern-data
# reaches an initialised definition). If the port instead decides to bind
# these tables back to the ov006 mount, all three rows must go back to the
# DS-seam form.
#
# PMF_SEAM: DROPPED as routing -- the five typed `->*` dispatches (02104c60
# over 021427ec, 021057f0 over 02142820, 02106eb8 / 02106f44 over 02142840,
# 02106fdc / 0210709c over 02142840) now call a host member pointer to a host
# member, which MSVC's own ->* sequence does correctly. Kept as an EMPTY list
# (not None) so pmf_seam_residue still guards the TU: with no patch the
# declaration block is not prepended (apply_patches returns early), and every
# `->*` left must be listed in PMF_SEAM_ALLOW below.
ROWS_00[("PMF_SEAM", "dScMgPanel_c")] = []
# PMF_SEAM_DECL: the include/extern block only rode in with a PMF_SEAM patch;
# no patch, no use. DROPPED.
ROWS_00[("PMF_SEAM_DECL", "dScMgPanel_c")] = None
# PMF_SEAM_ALLOW: every `->*` the generated copy keeps, each reading one of
# the tables this TU itself defines with &dScMgPanel_c:: initialisers (host
# member pointers). The old two seated-table lines were respelled by main
# (`(c->*data_ov006_021427bc[j].pmf[0])(0)` -> `(((dScMgPanel_c *)c)->*
# data_ov006_021427bc[j])(0)`, and the 02142860 one likewise). The 02142840
# `[index]` line with 8-space indent occurs three times after MG_PMF_CALL
# (02106eb8, 02106f44, and the rewritten 02106bc0 call); the `scene->*` line
# twice (02106fdc, 0210709c); the 02142888 line is the rewritten Behavior.
ROWS_00[("PMF_SEAM_ALLOW", "dScMgPanel_c")] = [
    "    (((dScMgPanel_c *)c)->*data_ov006_021427bc[j])(0);",
    "        (cc->*data_ov006_021427ec[*(u8 *)(scene + 0x4000 + 0x686)])();",
    "    (((dScMgPanel_c *)c)->*data_ov006_02142860[c->idx])();",
    "    (((dScMgPanel_c *)c)->*data_ov006_02142820[c->idx])();",
    "            (((dScMgPanel_c *)scene)->*data_ov006_02142840[*p])(i);",
    "        (((dScMgPanel_c *)scene)->*data_ov006_02142840[index])(i);",
    "        (scene->*data_ov006_02142840[index])(i);",
    "    (((dScMgPanel_c *)c)->*data_ov006_02142888[idx])();",
]
# MG_PMF_CALL: the two OPEN-CODED decodes are still wrong on the host, but for
# a different reason now. Behavior (table 02142888, arity 0) and
# func_ov006_02106bc0 (table 02142840, arity 1) reinterpret the table entry
# as two ints and call the code word through a plain cdecl pointer
# (`((void (*)(void *))fn)(obj)`, `fn(thisp, i)`). The word is a host
# __thiscall member now, so a cdecl call pushes the receiver instead of
# passing it in ecx (and leaves the arity-1 argument for the callee's `ret 4`
# to clean twice); the seam would refuse the host word. RE-ANCHORED on the
# same two call lines, replaced by the typed member-pointer call over the
# same table and index the decode read, so MSVC applies its own adjustment
# and convention. (The decode's locals stay; they are just unused.)
# DROPPED: the 02106ca4 pair (retype `extern PanelPmf_ca4
# data_ov006_02142840[]` to two ints, and route its `->*` call). Its reason was
# a member pointer to an INCOMPLETE class striding the table at the wrong
# width; main declares it `PanelStateFnI` over the complete dScMgPanel_c, /vmg
# /vmm fixes the width at 8 in any case, and the table is the TU's own host
# table, so its plain ->* call is correct (listed in PMF_SEAM_ALLOW above).
ROWS_00[("MG_PMF_CALL", "dScMgPanel_c")] = [
    ("    ((void (*)(void *))fn)(obj);",
     "    /* hostgen MG_PMF_CALL: this TU defines data_ov006_02142888 with\n"
     "       host member pointers; call it typed so MSVC passes the receiver\n"
     "       in ecx. See the table. */\n"
     "    (((dScMgPanel_c *)c)->*data_ov006_02142888[idx])();"),
    ("        fn(thisp, i);",
     "        /* hostgen MG_PMF_CALL: this TU defines data_ov006_02142840 with\n"
     "           host member pointers; call it typed. See the table. */\n"
     "        (((dScMgPanel_c *)scene)->*data_ov006_02142840[index])(i);"),
]

# ---- Player ----------------------------------------------------------------
# ASM_EXCISION (folded from _ZN6Player13InitResourcesEv): Player::
# InitResources zeroes the fresh 0x14-byte mHeldObjQueue allocation with a
# codegen-only mwccarm construct the host compiler cannot consume. It was a
# hand-written CLEAR asm block (5 x 4 strb post-inc); main replaced the block
# with mwccarm's `__builtin__clear((void*)mHeldObjQueue, 0x14);` intrinsic --
# the same inline CLEAR, per the TU's own comment -- which MSVC/clang-cl do not
# know (nothing in port/ or include/ defines it). Same fix, re-anchored on the
# intrinsic call: a plain 20-byte zero of the same block. (mHeldObjQueue is an
# s32 holding the pointer, hence the cast.) The TU has no asm block left, so
# the post-emit "asm still present" check is trivially satisfied.
ROWS_00[("ASM_EXCISION", "Player")] = [
    ("    __builtin__clear((void*)mHeldObjQueue, 0x14);",
     "    /* hostgen ASM_EXCISION: mwccarm's __builtin__clear intrinsic (its\n"
     "       inline CLEAR, 5 x 4 strb post-inc) has no host spelling. The\n"
     "       state it reaches is a 20-byte zero of the fresh block. */\n"
     "    { char *zq_ = (char *)mHeldObjQueue;\n"
     "      for (int zi_ = 0; zi_ < 0x14; ++zi_) zq_[zi_] = 0; }"),
]
# NARROW_RETURN: DROPPED. The row wrapped the flat extern "C" body
# `unsigned char _ZN6Player19func_ov002_020c84b0Ev(...)` because the TU's own
# St_NoControl caller declared it `extern int` and read the u8 return wide
# (three bytes of junk in EAX). Main made it a real member,
# `unsigned char Player::func_ov002_020c84b0()` (declared so in
# include/Player.h:873), and its only caller (St_NoControl's case 26) now calls
# it as a member through that declaration, reading it narrow. No flat
# definition is left for a "flat" row to wrap, and nothing in port/, src/ or
# include/ references the flat name any more (cxx_aliases.cpp's row for it is
# already retired).
ROWS_00[("NARROW_RETURN", "Player")] = None

# ======================================================================
# part_01
# PORTSYNC2 re-derived hostgen rows, part_01.
# Keys are (TABLE, TU stem); the value replaces the whole entry, None deletes it.

_FOR = "    return 0;  /* hostgen FALLS_OFF_RETURN: see the table's note */\n"

ROWS_01 = {
    # ---- dBgW collision-callback address taken as a flat-Itanium DATUM -------
    # The old rows gave C linkage to `extern <T> _ZN4dBgW16UpdatePosAndAngs...;`
    # (or ...22UpdatePosWithTransform...) so MSVC would ask for the flat name
    # instead of a decorated data spelling. Main's InitResources no longer
    # declares any datum: each TU now passes `&dBgW::UpdatePosAndAngs` /
    # `&dBgW::UpdatePosWithTransform`, the real static members declared in
    # include/dBgW.h:124/130, which MSVC references as
    # ?UpdatePosAndAngs@dBgW@@SAX... -- the host's own definition (the very
    # name port/hal/cxx_aliases.cpp:2251/2271 bridges the flat name onto).
    # The defect is gone with the datum; there is nothing left to re-link. DROP.
    ("DATA_C_LINKAGE", "daDpLift_c"): None,          # main :192 &dBgW::UpdatePosWithTransform
    ("DATA_C_LINKAGE", "daDsnBase_c"): None,         # main :399 &dBgW::UpdatePosAndAngs
    ("DATA_C_LINKAGE", "daObjCtKaitendai_c"): None,  # main :150 &dBgW::UpdatePosAndAngs
    ("DATA_C_LINKAGE", "daObjCtMecha10_c"): None,    # main :149 &dBgW::UpdatePosAndAngs
    ("DATA_C_LINKAGE", "daObjDpBrock_c"): None,      # main :109 &dBgW::UpdatePosWithTransform
    ("DATA_C_LINKAGE", "daObjEwbIce_c"): None,       # main :404 &dBgW::UpdatePosWithTransform

    # ---- daGmch_c::ApplySlopeToVertSpeed falls off the end of an int body ----
    # Still true in main (daGmch_c.cpp:343-364, "Declared `int` and it falls off
    # the end"). Main now spells the wall read as a member call
    # `((dBgPi *)...->GetWallResult())->surface.CopyNormalTo(*(Vector3 *)wallNormal);`
    # instead of the flat _ZNK10dBgCh_Actr13GetWallResultEv(clsn)+4 call.
    # RE-ANCHOR on the wall branch's CopyNormalTo (unique in the TU).
    ("FALLS_OFF_RETURN", "daGmch_c"): [
        ("surface.CopyNormalTo(*(Vector3 *)wallNormal);\n    }\n}",
         "surface.CopyNormalTo(*(Vector3 *)wallNormal);\n    }\n" + _FOR + "}"),
    ],
}

# ---- daGmch_c: the factory no longer names its vtable --------------------------
# The row gave `_ZTV8daGmch_c` C linkage because daGmch_c_classInit stored it
# by name (`p[0] = (int)&_ZTV8daGmch_c[2];`). Main rewrote the factory as
# `return new daGmch_c();` (daGmch_c.cpp:939-942) and the TU is the key-function
# TU that emits its own vtable; the only mention left is the unused
# include/daGmch_c.h:175 declaration, so no reference to the symbol is emitted.
# Same reason ZTV_C_LINKAGE already dropped d_a_ey_bm / d_a_mc_flag / daPkn_c. DROP.
ROWS_01[("ZTV_C_LINKAGE", "daGmch_c")] = None

# ---- daKuriKing_c (Goomboss) InitResources: level overlay objects by address --
# Main renamed the two ambiguous level-overlay addresses by address:
# func_021123f4 -> data_ov010_021123f4 and func_021124ac -> data_ov010_021124ac
# (declared `extern BMA_File` / `extern BTA_File` at daKuriKing_c.cpp:193-194).
# The defect is unchanged: the symbol table resolves the address to one
# arbitrary level overlay (now ov010, still `ambiguous`) while Goomboss's level
# is 45 / ov053, and the host mounts overlays whole with no C name for an
# interior address. RE-ANCHOR on the new spelling, still via port_ov053_at().
# Three sites, InitResources lines 1758, 1768, 1769.
ROWS_01[("OVERLAY_OBJECT_AT", "daKuriKing_c")] = [
    ("*(BMA_File *)&data_ov010_021123f4", "*(BMA_File *)port_ov053_at(0x021123f4)"),
    ("&data_ov010_021123f4, 0x40000000", "port_ov053_at(0x021123f4), 0x40000000"),
    ("&data_ov010_021124ac, 0, 0x1000", "port_ov053_at(0x021124ac), 0, 0x1000"),
]
# DECL unchanged (re-stated so the pair reads together).
ROWS_01[("OVERLAY_OBJECT_AT_DECL", "daKuriKing_c")] = (
    '/* hostgen OVERLAY_OBJECT_AT: see the table in tools/hostgen.py */\n'
    'extern "C" void *port_ov053_at(unsigned ds);\n')

# ---- daKuriKing_c: ROM data externs declared at file scope in C++ ------------
# The old anchor `extern void *data_ov002_0210da30;` sat inside the per-function
# file. Main's TU declares, at file scope and outside any extern "C" region
# (daKuriKing_c.cpp:204-212):
#     extern SharedFilePtr data_ov002_0210da30;   extern SharedFilePtr data_ov084_02130cf8;
#     extern SharedFilePtr *data_ov074_0212292c[]; extern SharedFilePtr *data_ov074_02122948[];
# RE-ANCHOR before line 204 with main's own types (a redeclaration must agree).
# Narrowed: data_ov074_02122e4c/02122e5c/02122e04/02122dfc are now declared
# extern "C" by main itself (lines 147,160,163,190; 2006-2007), and
# data_ov074_02123000/02123040/02123030 are now DEFINED in this TU (lines
# 1982-1991, the folded static-init) -- C linkage on a definition would move
# the TU's own object onto the name the ov074 mount already defines.
# data_ov084_0213089c / 02130cc8 (lines 206-207, from the folded vanish
# variant) are already bridged decorated->C in port/hal/actor_classes_ov074.cpp
# :339-340, so they are left out as before.
ROWS_01[("DATA_C_LINKAGE", "daKuriKing_c")] = ("extern SharedFilePtr data_ov002_0210da30;\n", [
    ("extern SharedFilePtr data_ov002_0210da30;", "data_ov002_0210da30"),
    ("extern SharedFilePtr data_ov084_02130cf8;", "data_ov084_02130cf8"),
    ("extern SharedFilePtr *data_ov074_0212292c[];", "data_ov074_0212292c"),
    ("extern SharedFilePtr *data_ov074_02122948[];", "data_ov074_02122948"),
], "before")

# ---- daMip_c: local-type externs / local class with undefined virtuals -------
# (a) `struct G { int w[2]; }; extern struct G data_ov085_021305c0;` is gone:
#     every member now declares `extern DaMipAnimationFilePtr data_ov085_021305c0;`
#     (a file-scope type; lines 342, 486, 1009, 1098, 1232, 1353, 1535). DROP.
# (b) The four `struct Mtx43 { s32 m[12]; };` locals are still there
#     (lines 1125, 1150, 1180, 1296) and two of them still declare
#     `extern struct Mtx43 data_020a0e68;` (1126, 1191). KEEP unchanged.
# (c) Render's `struct VObj` with six never-defined virtuals is gone: main's
#     Render (line 1252) no longer declares it. DROP.
ROWS_01[("REDECL_CONFLICT", "daMip_c")] = [
    ("    struct Mtx43 { s32 m[12]; };\n", ""),
]
ROWS_01[("REDECL_CONFLICT_DECL", "daMip_c")] = """\
/* hostgen REDECL_CONFLICT: the file-scope half of the repair below. Four
   members define an identical `struct Mtx43 { s32 m[12]; }` for themselves and
   two of them then declare an extern of that type, which C++ refuses because a
   local class has no linkage; one definition up here serves all four and every
   `struct Mtx43` spelling in the file still reads. */
struct Mtx43 { int m[12]; };

"""
# daMip_c_classInit is `return new daMip_c();` now (line 1692); the TU no longer
# names _ZTV7daMip_c (old line 1883 `p[0] = (int)(_ZTV7daMip_c + 2);`). DROP.
ROWS_01[("ZTV_C_LINKAGE", "daMip_c")] = None

# ---- daMky_c: block-scope redeclarations differing only in return type -------
# Rows 0-17 (eighteen block-scope `extern` copies of func_0201267c,
# dActor_c::ClosestPlayer, dCc_c::Clear and four daMky_c members) are gone from
# main: the members now call this->ClosestPlayer(), mdCc.Clear() and their own
# member functions directly, and func_0201267c has the single file-scope
# declaration at line 71. Row 18 (`struct dActor_c;` shadowing the real class)
# is gone too. DROP those. The four local types used in externs survive
# unchanged (M4x3 at 427/432, S at 520-521, G at 714-715, Item16): KEEP.
ROWS_01[("REDECL_CONFLICT", "daMky_c")] = [
    ("    typedef struct M4x3 { int w[12]; } M4x3;\n",
     "    typedef daMky_M4x3 M4x3;\n"),
    ("    struct S { int w[2]; };\n"
     "    extern struct S data_ov030_02115d18;\n",
     "    extern daMky_S data_ov030_02115d18;\n"),
    ("    struct G { void *a; void *b; };\n"
     "    extern struct G data_ov030_02115d18;\n",
     "    extern daMky_G data_ov030_02115d18;\n"),
    ("    typedef struct { int a, b, c, d; } Item16;\n",
     "    typedef daMky_Item16 Item16;\n"),
]

# ---- daObjBkKillerdai_c: out-of-class member redeclarations (C2761) ----------
# All five (`int dBgActor_c_helper::UpdateKillByMegaChar(...)`,
# `Actor_s::FindWithID/ClosestPlayer/Spawn`, `dBgActor_c_helper::IsClsnInRange`)
# are gone together with the helper structs: main calls
# dActor_c::FindWithID / ClosestPlayer() / dActor_c::Spawn as real members
# (lines 137-150) and the two dBgActor_c helpers through flat extern
# declarations at file scope (lines 59-60). Nothing left to delete. DROP.
ROWS_01[("MEMBER_REDECL", "daObjBkKillerdai_c")] = None

# ======================================================================
# part_02
# PORTSYNC2 re-derived hostgen rows, part_02.
# Each value REPLACES the whole table entry for that stem; None deletes it.

ROWS_02 = {
    # DATA_C_LINKAGE daObjFl_Seesaw_c (was _ZN10LavaSeesaw16CleanupResourcesEv).
    # Fixed: the old per-function TU declared `extern int data_ov022_021145a0[];`
    # at plain C++ file scope, so MSVC asked for the decorated datum.
    # DROP: main's TU declares the KCL handle inside its own `extern "C" {`
    # block (`extern SeesawCollisionFilePtr data_ov022_021145a0;`, line ~51)
    # and its later file-scope definition inherits that C linkage. The
    # anchor (#include "decl_common.h") and the int[] spelling are both gone;
    # re-inserting an `extern "C" int data_ov022_021145a0[]` would now
    # conflict with the TU's own type.
    ("DATA_C_LINKAGE", "daObjFl_Seesaw_c"): None,

    # DATA_C_LINKAGE daObjFloatBoard_c / daObjKaitendai_c /
    # daObjRotateUpdownLift_c (the dBgW::UpdatePosAndAngs callback) and
    # daObjSeesaw_c / daOnms_c (dBgW::UpdatePosWithTransform).
    # Fixed: the old TUs took the callback's address through a flat Itanium
    # name declared as a DATUM (`extern int/char/void*/int[] _ZN4dBgW...`),
    # which MSVC decorated as data.
    # DROP: main's text no longer declares any _ZN4dBgW... datum; each TU now
    # writes `&dBgW::UpdatePosAndAngs` / `&dBgW::UpdatePosWithTransform`, the
    # real static member (include/dBgW.h:124/130), which MSVC references by
    # its ordinary decorated function name (?UpdatePosAndAngs@dBgW@@SAX...),
    # the name port/hal/cxx_aliases.cpp already bridges. The datum this row
    # inserted (and the anchor it keyed on) is not in any of these TUs.
    ("DATA_C_LINKAGE", "daObjFloatBoard_c"): None,
    ("DATA_C_LINKAGE", "daObjKaitendai_c"): None,
    ("DATA_C_LINKAGE", "daObjRotateUpdownLift_c"): None,
    ("DATA_C_LINKAGE", "daObjSeesaw_c"): None,
    ("DATA_C_LINKAGE", "daOnms_c"): None,

    # FALLS_OFF_RETURN daObjYajirusi_c (was ArrowSignRight::OnAttacked1).
    # Fixes C2561: a bare `return;` inside the int-typed OnAttacked1 slot.
    # RE-ANCHORED: main's body is the same defect, only reformatted -- the
    # early exit is now split over two lines (`if (r == 0)\n        return;`).
    # Same rewrite: the early exit becomes the absence of the Kill() call.
    # Anchor is unique in the TU.
    ("FALLS_OFF_RETURN", "daObjYajirusi_c"): [
        ("    if (r == 0)\n        return;\n    Kill();\n}",
         "    if (r != 0) {\n"
         "        Kill();\n"
         "    }\n"
         "}"),
    ],

    # VIRTUAL_CALL daObj_Mip_Key_c (was RabbitKey::Render).
    # Fixed: the old TU dispatched Model vtable word 5 (Render) through a
    # local shadow `void (*fn14)(void *, int)` cdecl pointer while the host
    # seats a __fastcall thunk there.
    # DROP: main's Render now calls `mModel.Render(0);` -- a real C++ call on
    # the Model member (include/Model.h:73, virtual Render(const Vector3*)).
    # MSVC binds it __thiscall to ?Render@Model@@UAEXPBUVector3@@@Z, which
    # the host defines (port/hal/cxx_aliases.cpp:2336; the unpatched
    # mModel.Render(...) sites in many other src/actors TUs rely on it). No
    # raw vtable word is read any more.
    ("VIRTUAL_CALL", "daObj_Mip_Key_c"): None,

    # LEDGER_PARK daPkn_c (TrackClosestPlayer, ex func_ov084_0212f204).
    # Fixed: the old TU defined this as an extern "C" flat function
    # `_ZN7daPkn_c18TrackClosestPlayerEv(char *r4)` that called ClosestPlayer()
    # with no argument (receiver riding r0); the host copy in
    # port/unmatched/Actor_ClosestPlayer_OverlayReaders.cpp defines the same
    # C name with the receiver passed, so the TU's body had to be parked to
    # avoid a duplicate symbol.
    # DROP: main's text is a real member `void daPkn_c::TrackClosestPlayer()`
    # that calls `ClosestPlayer()` as a member (this passed __thiscall; the
    # decorated ?ClosestPlayer@dActor_c@@QAEPAUPlayer@@XZ is a faces_sync R
    # row). Its symbol is MSVC-decorated, so it no longer collides with the
    # host copy's C name; and the TU's own call `TrackClosestPlayer();` in
    # Behavior references the decorated member, which nothing else defines
    # (no face row), so parking it would now leave an unresolved external.
    # The host copy's _ZN7daPkn_c18TrackClosestPlayerEv body becomes dead
    # and should be retired with the slice change (outside this file).
    ("LEDGER_PARK", "daPkn_c"): None,

    # LEDGER_PARK daSanbo_c (func_ov096_02135e2c).
    # Fixed: the old TU defined func_ov096_02135e2c as an extern "C" int body
    # that fell off the end (C4716) and collided with the C-linkage host copy
    # in port/unmatched/Pokey_HostSites.cpp; the banner-to-banner park hid it.
    # DROP: main's text is a real member `void daSanbo_c::func_ov096_02135e2c(
    # dBgCh_Actr *clsn)` -- void, so no C4716 -- whose decorated symbol does
    # not collide with the host copy's C name. Its one caller (line ~402,
    # `func_ov096_02135e2c(&mWithMeshClsn)`) is a member call needing the
    # decorated symbol, which nothing in port/ defines or aliases, so parking
    # would now leave an unresolved external. The "/* ROM ordinal N -- ... */"
    # banners the row anchored on are gone (main uses `// @symbol` lines).
    # Pokey_HostSites.cpp's copy becomes dead (retire it outside this file).
    ("LEDGER_PARK", "daSanbo_c"): None,

    # REG_RIDE_ARG ModelComponents (was _ZN15ModelComponents6RenderEP9Matrix4x3P7Vector3).
    # Fixes: Render calls func_02044b30 with only the object, relying on the
    # material index already sitting in ARM r1; under cdecl the callee reads
    # `idx` from an unwritten stack slot.
    # RE-ANCHORED: main folded func_02044b30 into the same TU and declares it
    # correctly at file scope (`void func_02044b30(char *obj, int idx);`), so
    # the declaration half of the row is no longer needed. The defect moved to
    # the call sites, which now spell the one-arg call through a function-
    # pointer cast `((void (*)(void *))func_02044b30)(this);` (two sites, both
    # with the loop's `u8 idx` live and named). Patch both to pass idx.
    ("REG_RIDE_ARG", "ModelComponents"): [
        ("((void (*)(void *))func_02044b30)(this);",
         "func_02044b30((char *)this, idx);"
         "  /* hostgen REG_RIDE_ARG: the material index rides r1, see the "
         "table */"),
    ],
}

# ======================================================================
# part_03
ROWS_03 = {
    # dExtShadowModel_c: DATA_C_LINKAGE gave the live-shadow list head
    # data_0209cef4 C linkage. DROP: main's TU now declares it inside its own
    # extern "C" { ... } block (`extern dExtShadowModel_c *data_0209cef4;`),
    # so it already gets the undecorated name; the defect is gone.
    ("DATA_C_LINKAGE", "dExtShadowModel_c"): None,
    # SaveData (folded from CollectStar): ARG_WIDTH retyped CollectStar's first
    # parameter to the s8 its caller (old src/CollectStarInLevel.c) declared.
    # DROP: main folded caller and callee into this one TU; CollectStarInLevel
    # now calls CollectStar through the definition's own `(int a, int b)`
    # prototype, so the caller sign-extends to a full int itself. No other
    # declaration of CollectStar with an s8 parameter remains in src/,
    # include/ or port/.
    ("ARG_WIDTH", "SaveData"): None,
}

# dCamera_c (folded from func_0200ca50 + func_0200cae4): CALL_STATE_FN routes
# the camera state objects' mwcc member-pointer dispatch through
# hal_call_camera_state_fn on the DS code word instead of calling it raw.
# RE-ANCHOR: main still dispatches raw `(x->**pp)()` in both bodies; in
# func_0200ca50 the receiver is now the CamPmfHost view `ch` (same pointer as
# `self`), so the anchor reads `(ch->**pp)()`. func_0200cae4's text
# `return (c->**p)();` is unchanged. Order kept as merged (0200ca50 first).
ROWS_03[("CALL_STATE_FN", "dCamera_c")] = [
    ("            r5 = (ch->**pp)();",
     "            r5 = hal_call_camera_state_fn(self, *(unsigned *)pp);"),
    ("  return (c->**p)();",
     "  return hal_call_camera_state_fn(c, *(unsigned *)p);"),
]

# d_a_obj_ki_fune (folded from _ZN14daObjKi_Fune_c13InitResourcesEv) and
# d_a_obj_wc_obj04: DATA_C_LINKAGE gave a flat-Itanium DATUM spelling of
# dBgW::UpdatePosWithTransform C linkage so the type-disagreeing data externs
# collapsed onto the one flat name. DROP: main no longer declares that datum
# at all -- both TUs now take the address of the real static member
# `&dBgW::UpdatePosWithTransform` declared in include/dBgW.h (ki_fune line 84,
# wc_obj04 line 105), the one spelling every consumer of the header shares, so
# there is no per-TU decorated datum left to bridge.
ROWS_03[("DATA_C_LINKAGE", "d_a_obj_ki_fune")] = None
ROWS_03[("DATA_C_LINKAGE", "d_a_obj_wc_obj04")] = None

# d_a_pg_rcer (folded from func_ov019_0211140c): FALLS_OFF_RETURN adds the
# missing return to RacingPenguin's normal helper, an `int` body that falls
# off its end (C4716; all callers discard the value). RE-ANCHOR: main still
# falls off; the wall branch is now spelled as a member call
# `((dBgPi *)...GetWallResultEv(clsn))->surface.CopyNormalTo(...wallNormal)`.
# The floor branch writes floorNormal and does not match this text.
ROWS_03[("FALLS_OFF_RETURN", "d_a_pg_rcer")] = [
    ("        ((dBgPi *)_ZNK10dBgCh_Actr13GetWallResultEv(clsn))->surface."
     "CopyNormalTo(*(Vector3 *)wallNormal);\n    }\n}",
     "        ((dBgPi *)_ZNK10dBgCh_Actr13GetWallResultEv(clsn))->surface."
     "CopyNormalTo(*(Vector3 *)wallNormal);\n    }\n"
     "    return 1;  /* hostgen FALLS_OFF_RETURN: see the table's note */\n"
     "}"),
]

# d_a_propeller_heyho: EXTERN_C_DATA gives FlyGuy's seven SharedFilePtr views
# and the wander state record C linkage, so the body binds to the port's
# per-symbol ov070 mount (C names) instead of asking for decorated names.
# RE-ANCHOR: main retyped the seven views to the TU-local wrappers
# HeyhoModelFile / HeyhoAnimationFilePtr and moved data_ov070_0212359c below a
# comment block, and there is no `extern "C" {` directly after them any more,
# so instead of moving an opener this wraps each run in its own
# extern "C" { } (balanced). The 0212359c anchor carries the following
# comment's first line because that declaration is repeated (as a
# redeclaration, which inherits this linkage) five more times further down.
# CAVEAT (not fixed by this row): main now also DEFINES all eight names (plus
# the five other state records) at the end of the TU, the fold of
# __sinit_ov070_02122afc; with this row they become C-linkage definitions of
# names ovdata.py's ov070 mount also defines. See the report.
ROWS_03[("EXTERN_C_DATA", "d_a_propeller_heyho")] = [
    ("extern HeyhoModelFile data_ov070_02123530;\n"
     "extern HeyhoAnimationFilePtr data_ov070_02123520;\n"
     "extern HeyhoAnimationFilePtr data_ov070_02123518;\n"
     "extern HeyhoAnimationFilePtr data_ov070_02123510;\n"
     "extern HeyhoAnimationFilePtr data_ov070_02123528;\n"
     "extern HeyhoAnimationFilePtr data_ov070_02123508;\n"
     "extern HeyhoAnimationFilePtr data_ov070_02123500;\n",
     'extern "C" {  /* hostgen EXTERN_C_DATA */\n'
     "extern HeyhoModelFile data_ov070_02123530;\n"
     "extern HeyhoAnimationFilePtr data_ov070_02123520;\n"
     "extern HeyhoAnimationFilePtr data_ov070_02123518;\n"
     "extern HeyhoAnimationFilePtr data_ov070_02123510;\n"
     "extern HeyhoAnimationFilePtr data_ov070_02123528;\n"
     "extern HeyhoAnimationFilePtr data_ov070_02123508;\n"
     "extern HeyhoAnimationFilePtr data_ov070_02123500;\n"
     "}  /* hostgen EXTERN_C_DATA */\n"),
    ("extern daPropeller_Heyho_c::State data_ov070_0212359c;\n"
     "/* The ROM PMF constants the six state records copy from",
     'extern "C" {  /* hostgen EXTERN_C_DATA */\n'
     "extern daPropeller_Heyho_c::State data_ov070_0212359c;\n"
     "}  /* hostgen EXTERN_C_DATA */\n"
     "/* The ROM PMF constants the six state records copy from"),
]

# d_a_wanwan: LEDGER_PARK brackets daWanwan_c_classInit out because
# port/unmatched/ChainChomp_Spawn_hostcopy.cpp defines the same factory (a
# second definition is a duplicate symbol). RE-ANCHOR: main's factory is now
# `extern "C" daWanwan_c *daWanwan_c_classInit()` with `{` on its own line and
# member-named __cxa_vec_ctor calls; the bracket still opens on the
# definition line (below the @symbol marker and comment) and closes on the
# body's own last brace. The host copy still exists, so the park stays.
ROWS_03[("LEDGER_PARK", "d_a_wanwan")] = [
    ('extern "C" daWanwan_c *daWanwan_c_classInit()\n{\n',
     "#if 0  /* hostgen LEDGER_PARK: daWanwan_c_classInit */\n"
     'extern "C" daWanwan_c *daWanwan_c_classInit()\n{\n'),
    # PORTSYNC3 re-anchor: main's factory is `return new daWanwan_c();` now.
    ("    return new daWanwan_c();\n}\n",
     "    return new daWanwan_c();\n}\n"
     "#endif  /* hostgen LEDGER_PARK: daWanwan_c_classInit */\n"),
]

# d_s_mg_roulette: LEDGER_PARK brackets dScMgRoulette_c_classInit out because
# port/unmatched/MgRoulette_Factory.cpp hosts the same factory. RE-ANCHOR: the
# opening line is unchanged; the body's tail now reads
# `_ZN5ModelC1Ev(models + 0x60);   /* mModel2 */ ... return scene;`.
# The host copy still exists, so the park stays.
ROWS_03[("LEDGER_PARK", "d_s_mg_roulette")] = [
    ('extern "C" void* dScMgRoulette_c_classInit(void){\n',
     "#if 0  /* hostgen LEDGER_PARK: dScMgRoulette_c_classInit -- "
     "port/unmatched/MgRoulette_Factory.cpp hosts this factory for the\n"
     "          dropped-receiver _ZN11dScMgBase_cC2Ev call */\n"
     'extern "C" void* dScMgRoulette_c_classInit(void){\n'),
    ("    _ZN5ModelC1Ev(models + 0x60);   /* mModel2 */\n  }\n  return scene;\n}\n",
     "    _ZN5ModelC1Ev(models + 0x60);   /* mModel2 */\n  }\n  return scene;\n}\n"
     "#endif  /* hostgen LEDGER_PARK: dScMgRoulette_c_classInit */\n"),
]

# ======================================================================
# part_04
# PORTSYNC2 re-derived hostgen rows, part_04: hostgen'd TUs that main's
# folds left uncompilable on the host (fix lane B). Each value REPLACES the
# whole table entry for that stem; None deletes it.
ROWS_04 = {}

# ---- _ZN10dScStage_c16CleanupResourcesEv -----------------------------------
# PARK_SYMBOL -> HOST_COPY_PARK. The PARK_SYMBOL row parks from the
# `// @symbol` marker to the first brace pair after it, which works for a
# folded body whose marker sits on its definition. In this one-function file
# the marker heads the FILE: it is followed by the includes and the TU's
# `extern "C" { ... }` declaration block, so the park swallowed the includes
# and the declarations and left the definition itself live (MSVC C2653,
# 'dScStage_c' is not a class or namespace name). The reason for the park is
# unchanged -- port/unmatched/hostarm/_ZN5Stage16CleanupResourcesEv.cpp carries
# this member with the port line's arm and port/slice_gate213.txt compiles it
# -- so the bracket now opens on the definition line and closes on the body's
# own last brace, the HOST_COPY_PARK shape. The declarations stay live and
# unused.
ROWS_04[("PARK_SYMBOL", "_ZN10dScStage_c16CleanupResourcesEv")] = None
ROWS_04[("HOST_COPY_PARK", "_ZN10dScStage_c16CleanupResourcesEv")] = [
    ("int dScStage_c::CleanupResources()\n{\n",
     "#if 0  /* hostgen HOST_COPY_PARK: port/unmatched/hostarm/\n"
     "          _ZN5Stage16CleanupResourcesEv.cpp defines this member */\n"
     "int dScStage_c::CleanupResources()\n{\n"),
    ("            _ZN5Sound6Player19SetPlayableSeqCountEii(3, 1);\n"
     "        }\n    }\n    return 1;\n}\n",
     "            _ZN5Sound6Player19SetPlayableSeqCountEii(3, 1);\n"
     "        }\n    }\n    return 1;\n}\n"
     "#endif  /* hostgen HOST_COPY_PARK: dScStage_c::CleanupResources */\n"),
]

# ---- dActor_c --------------------------------------------------------------
# PARK_SYMBOL -> HOST_COPY_PARK, the dScStage_c::CleanupResources shape.
# Main's `// @symbol _ZN8dActor_c10EarthquakeERK7Vector35Fix12IiE` marker is
# followed by the body's own `extern "C" { ... }` declaration block, then a
# SECOND `extern "C" {` holding the definition, so the marker-to-first-brace
# park took the declarations (func_0200d8c8, data_0209f318) and left the
# definition live and undeclared (C2065). The park's reason stands
# (port/unmatched/hostarm/ carries this body with the port line's arm), so
# the bracket now holds the definition itself; the declarations stay live.
ROWS_04[("PARK_SYMBOL", "dActor_c")] = None
ROWS_04[("HOST_COPY_PARK", "dActor_c")] = [
    ("void _ZN8dActor_c10EarthquakeERK7Vector35Fix12IiE(void* v, int f) {\n"
     "    func_0200d8c8(data_0209f318, f);\n}\n",
     "#if 0  /* hostgen HOST_COPY_PARK: port/unmatched/hostarm/ carries this body */\n"
     "void _ZN8dActor_c10EarthquakeERK7Vector35Fix12IiE(void* v, int f) {\n"
     "    func_0200d8c8(data_0209f318, f);\n}\n"
     "#endif  /* hostgen HOST_COPY_PARK: _ZN8dActor_c10EarthquakeERK7Vector35Fix12IiE */\n"),
]

# ---- daObjBk_Lift_c (was _ZN14daObjBk_Lift_c13InitResourcesEv) --------------
# DATA_C_LINKAGE: DROPPED, the daObjFl_Seesaw_c case (ROWS_02). The row put
# `extern SharedFilePtr data_ov015_02114a64/_02114a5c` and `extern CLPS_Block
# data_ov015_02113594` in an extern "C" block after `#include "dBgCh_Gnd.h"`.
# Main now declares all three inside the TU's own `extern "C" {` block, and the
# two file handles with the TU-local types BkLiftModelFilePtr /
# BkLiftCollisionFilePtr, so the row's SharedFilePtr spelling became a
# conflicting redeclaration (C2371). The names already come out undecorated.
ROWS_04[("DATA_C_LINKAGE", "daObjBk_Lift_c")] = None
# HOST_COPY_PARK (new): main also DEFINES the two handles at the end of the TU
# (`BkLiftModelFilePtr data_ov015_02114a64(1433);` and its 1434 sibling, the
# source of __sinit_daObjBk_Lift_c.cpp, ex __sinit_ov015_02113264). Under the
# extern "C" declaration above they are C-linkage definitions of names the
# port's per-symbol ov015 mount (port/ov015_syms.txt) already defines, so the
# link would see the storage twice. The port line's TU only declared them;
# the storage stays the mount's, and the bracket holds the two definitions.
ROWS_04[("HOST_COPY_PARK", "daObjBk_Lift_c")] = None   # PORTSYNC3: the TU defines and constructs the handles now (the mount rows are name-only)

# ---- daObjYajirusi_c -------------------------------------------------------
# VIRTUAL_CALL (new; the call-spelling table -- these three are non-virtual,
# but the repair is the same kind: respell a call so the host binds it to the
# body the ROM calls). Main's InitResources and OnHitByMegaChar now call the
# INHERITED dBgActor_c members UpdateModelPosAndRotY(), UpdateClsnPosAndRot()
# and KillByMegaChar(player) unqualified. Under SM64DS_PLATFORM_PC +
# _MSC_VER, include/daObjYajirusi_c.h gives the host a FLAT view (no
# dBgActor_c base: MSVC will not reuse dBgActor_c's tail padding, so real
# inheritance moves every derived field), and that view declares only the
# two inherited members its old Kill() needed. So the three names are
# undeclared (C3861). The flat view pins dBgActor_c's span at the ROM
# offsets (the header's static_asserts; sizeof(dBgActor_c) is 0x320 on the
# host too), so each call is spelled on the same object viewed as its real
# base; it binds to dBgActor_c's own host definition, __thiscall, exactly the
# call the ROM makes. The header-side fix (declare the three in the flat
# view, as it already does DisappearPoofDustAt) is an include/ change.
ROWS_04[("VIRTUAL_CALL", "daObjYajirusi_c")] = [
    ('#include "daObjYajirusi_c.h"\n',
     '#include "daObjYajirusi_c.h"\n'
     '#include "dBgActor_c.h"  /* hostgen VIRTUAL_CALL: the flat view\'s base */\n'),
    ("    UpdateModelPosAndRotY();\n    UpdateClsnPosAndRot();\n",
     "    /* hostgen VIRTUAL_CALL: inherited dBgActor_c members the host's flat\n"
     "       view does not declare -- see the table. */\n"
     "    reinterpret_cast<dBgActor_c *>(this)->UpdateModelPosAndRotY();\n"
     "    reinterpret_cast<dBgActor_c *>(this)->UpdateClsnPosAndRot();\n"),
    ("    KillByMegaChar(player);\n",
     "    reinterpret_cast<dBgActor_c *>(this)->KillByMegaChar(player);"
     "  /* hostgen VIRTUAL_CALL */\n"),
    # PORTSYNC3: main's factories are `new daObjYajirusi_c` now. On the host the class is
    # the FLAT view (no base, no vptr), so `new` leaves the vptr and the dBgActor_c
    # base unbuilt (measured: level 6 faulted on the first signpost spawn, vptr 0).
    # The port builds the object the way the ROM's factory does.
    ("int *daObjYajirusi_c_classInit_YAJIRUSI_L(void)\n{\n    return (int *)new daObjYajirusi_c;\n}",
     "extern \"C\" int *port_new_daObjYajirusi_c(void);  /* hal/portsync3_manual.cpp */\n"
     "int *daObjYajirusi_c_classInit_YAJIRUSI_L(void)\n{\n    return port_new_daObjYajirusi_c();  /* hostgen VIRTUAL_CALL */\n}"),
    ("int *daObjYajirusi_c_classInit_YAJIRUSI_R(void)\n{\n    return (int *)new daObjYajirusi_c;\n}",
     "int *daObjYajirusi_c_classInit_YAJIRUSI_R(void)\n{\n    return port_new_daObjYajirusi_c();  /* hostgen VIRTUAL_CALL */\n}"),
]

# ---- dMeter_c ----------------------------------------------------------------
# VIRTUAL_CALL (PORTSYNC3). The port's two HUD moves (the adventure stars and lives to the
# top screen, the VS coin row) used to ride vtable slot 9: hal/sub_actors.cpp seated
# hud_render in the hand-filled _ZTV8dMeter_c the ROM factory stored. Main's factory is
# `new dMeter_c` now, so the object carries the COMPILER's vtable and that seat is never
# dispatched (measured: lives and stars stayed on the bottom screen). The two moves are
# called from the top of the real Render instead; the hook answers 1 when it drew the HUD
# itself, which is exactly what hud_render returned.
ROWS_04[("VIRTUAL_CALL", "dMeter_c")] = [
    ("int dMeter_c::Render()\n{\n    int b = (data_0209f2d8 == 1);\n",
     "extern \"C\" int port_hud_render_hook(void *self);  /* hal/sub_actors.cpp */\n"
     "int dMeter_c::Render()\n{\n    if (port_hud_render_hook(this))  /* hostgen VIRTUAL_CALL */\n        return 1;\n"
     "    int b = (data_0209f2d8 == 1);\n"),
]

# ---- daOnms_c --------------------------------------------------------------
# EXTERN_C_DATA (new). Main's TU writes the arm9 sin/cos table as
# `s16 data_02082214[];` with no `extern`, inside its `extern "C" {` block.
# A declaration inside a braced linkage-specification is still a DEFINITION
# unless it says extern, and an array definition of unknown size is C2133 to
# MSVC (mwcc read it as the declaration it meant). The table is arm9 data the
# port already defines with C linkage; the word `extern` is what was meant.
# This TU is not on the --extern-data list (that rewrite would do the same
# thing to every data_ definition in the file), so the one line is spelled
# here.
ROWS_04[("EXTERN_C_DATA", "daOnms_c")] = [
    ("\ns16 data_02082214[];\n", "\nextern s16 data_02082214[];  /* hostgen EXTERN_C_DATA */\n"),
]

# ---- daSanbo_c -------------------------------------------------------------
# HEADER_SHADOW (new). include/daSanbo_c.h:145 now declares
# `extern int _ZTV9daSanbo_c[];` at plain C++ file scope (the key-function
# note), and include/decl_common.h:529 and :603 declare the same name inside
# its extern "C" block; the TU includes both. mwcc reads one declaration;
# MSVC refuses a second declaration with a different language linkage
# (C2732). The TU itself never names the vtable (`return new daSanbo_c()`
# stores it), so decl_common.h's two copies are hidden across its include
# and the class header's declaration stands alone. The host table is
# port/hal/actor_classes_ov096.cpp's _ZTV5Pokey either way.
ROWS_04[("HEADER_SHADOW", "daSanbo_c")] = ("decl_common.h", ("_ZTV9daSanbo_c",))

# ---- dExtFrameCtrl_c -------------------------------------------------------
# ARG_WIDTH (new; the return value, the same "spell the width the callers
# declare" repair). Main's TU defines `bool dExtFrameCtrl_c::WillHitFrame(int)
# const` while include/dExtFrameCtrl_c.h:77 declares it `int`. Itanium
# mangling carries no return type, so mwcc links one function; MSVC refuses an
# out-of-line definition whose return type differs from the in-class
# declaration (C2556/C2371). Every caller (the header, daDoor_c's
# `!= 0` tests, decl_Animation.h, dMgTrmpln2Mario_c's flat declaration) reads
# an int; on x86 an int return is the whole EAX, which is what the ARM
# callers read in r0 too, so the definition is spelled `int`. The body's
# value is unchanged (its comparisons give 0 or 1).
ROWS_04[("ARG_WIDTH", "dExtFrameCtrl_c")] = [
    ("bool dExtFrameCtrl_c::WillHitFrame(int frame) const\n",
     "int dExtFrameCtrl_c::WillHitFrame(int frame) const  /* hostgen ARG_WIDTH: header says int */\n"),
]

# ---- d_a_btn ---------------------------------------------------------------
# REDECL_CONFLICT (new). Main's TU declares func_ov079_02123d4c twice with C
# linkage: `void (int *out, daBtn_c *self)` in its extern "C" block (line 86,
# the definition's own spelling) and, for OnAimedAtWithEggReturnVec,
# `namespace vecret { extern "C" Vector3 func_ov079_02123d4c(void *); }` --
# the hidden-return-pointer view of the same (out, this) pair. Namespaces do
# not separate extern "C" names, so to MSVC that is a second C-linkage
# function of one name with another type (C2733). The view becomes an inline
# C++ forwarder of the same name in the same namespace: it calls the real
# (out, this) function with a local Vector3 and returns it, which is exactly
# what the hidden-pointer call did, with no ABI assumption about how MSVC
# returns a 12-byte struct. The one call site (`vecret::...(this)`) reads
# unchanged.
ROWS_04[("REDECL_CONFLICT", "d_a_btn")] = [
    ('namespace vecret { extern "C" Vector3 func_ov079_02123d4c(void *thiz); }\n',
     "namespace vecret {  /* hostgen REDECL_CONFLICT: a forwarder, not a second\n"
     "                       extern \"C\" declaration -- see the table */\n"
     "static inline Vector3 func_ov079_02123d4c(void *thiz)\n"
     "{\n"
     "    Vector3 v;\n"
     "    ::func_ov079_02123d4c((int *)&v, (daBtn_c *)thiz);\n"
     "    return v;\n"
     "}\n"
     "}\n"),
]

# ---- daWanwan2_c -----------------------------------------------------------
# PARK_SYMBOL -> HOST_COPY_PARK, the dScStage_c::CleanupResources shape. Main's
# `// @symbol daWanwan2_c_classInit` marker is followed by a `local extern`
# comment and the factory's own `extern "C" { ... }` declaration block (the
# constructor chain's mangled names), so the marker-to-first-brace park took
# the declarations and left the factory live with every callee undeclared
# (21 x C3861). The park's reason stands (port/unmatched/UnchainedChomp_
# Behavior.cpp defines daWanwan2_c_classInit), so the bracket now opens on
# the definition line and closes on the body's own last brace.
ROWS_04[("PARK_SYMBOL", "daWanwan2_c")] = None
ROWS_04[("HOST_COPY_PARK", "daWanwan2_c")] = [
    ('extern "C" daWanwan2_c *daWanwan2_c_classInit()\n{\n',
     "#if 0  /* hostgen HOST_COPY_PARK: port/unmatched/UnchainedChomp_Behavior.cpp\n"
     "          defines this factory */\n"
     'extern "C" daWanwan2_c *daWanwan2_c_classInit()\n{\n'),
    # PORTSYNC3 re-anchor: main's factory is `return new daWanwan2_c();` now.
    ("    return new daWanwan2_c();\n}\n",
     "    return new daWanwan2_c();\n}\n"
     "#endif  /* hostgen HOST_COPY_PARK: daWanwan2_c_classInit */\n"),
]

# ---- d_s_mg_base -----------------------------------------------------------
# HEADER_SHADOW (new). Main's TU keeps a host arm (`#ifdef _MSC_VER`) that
# defines the flat base-object destructor as
# `extern "C" dScMgBase_c *_ZN11dScMgBase_cD2Ev(dScMgBase_c *)` -- the ROM's
# D2 returns `this` -- while include/decl_common.h:1803 declares it
# `void (void *)` inside its extern "C" block. One register on the ROM, a C2733
# to MSVC. The TU calls the name nowhere else, so decl_common.h's declaration
# is hidden across its include and the arm's definition stands alone; the
# HAL callers that spell it `void (void *)` reach the same C symbol.
# (This TU's other ~145 errors are the per-function namespaces' disagreeing
# C-linkage externs: NS_C_ALIAS in tools/hostgen.py.)
ROWS_04[("HEADER_SHADOW", "d_s_mg_base")] = ("decl_common.h", ("_ZN11dScMgBase_cD2Ev",))

# ---- unit020cd744 ----------------------------------------------------------
# PARK_SYMBOL -> HOST_COPY_PARK, the dScStage_c::CleanupResources shape. The
# `// @symbol func_ov006_020ce988` marker sits at the top of the function's
# namespace, above its `extern "C" { ... }` declaration block, so the
# marker-to-first-brace park took the five declarations
# (Matrix4x3_FromQuaternion .. ModelBase::ApplyOpacity) and left the body live
# and its callees undeclared (5 x C3861). The park's reason stands
# (port/unmatched/hostarm/ carries this body with the port line's arm), so
# the bracket now holds the definition itself.
# (This TU's ~60 other errors are the per-function namespaces' disagreeing
# C-linkage externs: NS_C_ALIAS in tools/hostgen.py.)
ROWS_04[("PARK_SYMBOL", "unit020cd744")] = None
ROWS_04[("HOST_COPY_PARK", "unit020cd744")] = [
    ('extern "C" void func_ov006_020ce988(char* c){\n',
     "#if 0  /* hostgen HOST_COPY_PARK: port/unmatched/hostarm/ carries this body */\n"
     'extern "C" void func_ov006_020ce988(char* c){\n'),
    ("        vt->f(o, c+0x68);\n    }\n}\n",
     "        vt->f(o, c+0x68);\n    }\n}\n"
     "#endif  /* hostgen HOST_COPY_PARK: func_ov006_020ce988 */\n"),
]


# ======================================================================
# part_05

ROWS_05 = {}

# dMg3DHeyhoObjAdapter_c: main's fold declares `extern GObj *data_0209f5c0;`
# at global scope with C++ linkage (the old one-function file had it inside
# its extern "C" block), so MSVC would ask for ?data_0209f5c0@@3PAUGObj@@A
# where the arm9 mount defines the C name. Give the declaration C linkage.
# (The TU itself is on NS_C_ALIAS / NS_C_ALIAS_TIGHT in hostgen.py.)
ROWS_05[("EXTERN_C_DATA", "dMg3DHeyhoObjAdapter_c")] = [
    ("struct GObj { int w[8]; };\nextern GObj *data_0209f5c0;\n",
     'struct GObj { int w[8]; };\nextern "C" GObj *data_0209f5c0;  /* hostgen EXTERN_C_DATA */\n'),
]


# daObjFl_Seesaw_c: the TU includes math/Matrix.h first, so mwccarm sees
# Matrix4x3 as {r, t}; the port force-includes dBgActor_c_TailPad_HostAbi.h,
# which reaches include/common.h's flat {s32 m[12]} spelling first, and the
# guard keeps that one. Same 0x30 bytes with the translation at m[9..11], so
# spell the three stores against m[] on the host.
ROWS_05[("MWCC_SPELLING", "daObjFl_Seesaw_c")] = [
    ("    mModel.mat4x3.t.x = mPosX >> 3;\n"
     "    mModel.mat4x3.t.y = mPosY >> 3;\n"
     "    mModel.mat4x3.t.z = mPosZ >> 3;\n",
     "    mModel.mat4x3.m[9] = mPosX >> 3;   /* hostgen: flat Matrix4x3 under /FI */\n"
     "    mModel.mat4x3.m[10] = mPosY >> 3;\n"
     "    mModel.mat4x3.m[11] = mPosZ >> 3;\n"),
]


# dScMgHanachan_c: port/unmatched/dScMgHanachan_c.cpp is re-taken from main's
# merged TU (plus the one argument-order hunk its header describes), so two of
# the five Wiggler dispatch rows re-anchor on main's spelling: the open-coded
# decode now reads the record through `w->state` (struct C's +0x00 V2, main's
# name for the old *(int *)c / *(int *)(c + 4)), and Behavior's +0x4660 word is
# the member mPhaseFn. The other two PMF_SEAM sites match as they were. The
# defect is unchanged at all five: nothing seats these fields on the host, so
# the code word is a DS address and goes through port_mg_wiggler_call0.
ROWS_05[("MG_PMF_CALL", "dScMgHanachan_c")] = [
    ("    if (w->state.x != 0) {\n"
     "        int off = w->state.y;\n"
     "        char *base = c + (off >> 1);\n"
     "        void (*fn)(char *);\n"
     "        if (off & 1)\n"
     "            fn = *(void (**)(char *))(*(char **)base + w->state.x);\n"
     "        else\n"
     "            fn = *(void (**)(char *))c;\n"
     "        fn(base);\n"
     "    }\n",
     "    if (w->state.x != 0) {\n"
     "        /* hostgen MG_PMF_CALL: the record is two words at +0x00, a\n"
     "           code word and an adjustment. The code word is a DS address;\n"
     "           the class's own switch decodes it, guards it and calls it. */\n"
     "        port_mg_wiggler_call0(c, (unsigned)w->state.x, w->state.y);\n"
     "    }\n"),
]
ROWS_05[("PMF_SEAM", "dScMgHanachan_c")] = [
    ("    (o->*(o->pmf))();",
     "    PORT_PMF_CALL0(port_mg_wiggler_call0, o, o->pmf);"),
    ("        (((Self *)self)->*(((Self *)self)->cb))();",
     "        PORT_PMF_CALL0(port_mg_wiggler_call0, self,\n"
     "                       ((Self *)self)->cb);"),
    ("    (((CB *)this)->*(*(PMF *)&mPhaseFn))();",
     "    PORT_PMF_CALL0(port_mg_wiggler_call0, this,\n"
     "                   *(PMF *)&mPhaseFn);"),
]


# d_s_mg_base: the five screen-gap routers. On the port line each was its own
# one-function file compiled with two -D renames (port/CMakeLists.txt, the
# screen-gap block): <router>=<router>__rom, so hal/screen_gap.cpp's wrapper
# owns the real name and calls the ROM body under the private one, and the
# TU's OAM::Render overload -> port_oam_render_routed_*, so THAT body's calls
# reach the shim. Main folded all five into this TU, where a -D would reach
# every other OAM::Render call of the scene framework too (and three routers
# now call OAM::Render as a member, which no -D can reach). So the two renames
# are made here, per body, and the CMake -D rows are retired.
ROWS_05[("HOST_COPY_PARK", "d_s_mg_base")] = [
    ('void func_ov004_020afdd0(void* a0, int a1, int a2, int a3, int a4){\n  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;\n  if(scene == 0) return;\n  if(scene->mMenuOpen == 0){\n    if(scene->OnHitByCannonBlastedChar() == 2){\n      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1){\n        _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiES3_ii(0, a0, a1, a2 + 0xc0 + data_ov004_020beb6c, a3, a4, 0x1000, 0x1000, 0, -1);\n        return;\n      }\n      _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiES3_ii(0, a0, a1, a2, a3, a4, 0x1000, 0x1000, 0, -1);\n      return;\n    }\n  }\n  _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiES3_ii(0, a0, a1, a2 + 0xc0 + data_ov004_020beb6c, a3, a4, 0x1000, 0x1000, 0, -1);\n  OAM::RenderSub((OamAttr*)a0, a1, a2, a3, a4);\n}\n',
     'void *port_oam_render_routed_10(int, void *, int, int, int, int, int, int, int, int);  /* hostgen: hal/screen_gap.cpp shim */\nvoid func_ov004_020afdd0__rom(void* a0, int a1, int a2, int a3, int a4){\n  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;\n  if(scene == 0) return;\n  if(scene->mMenuOpen == 0){\n    if(scene->OnHitByCannonBlastedChar() == 2){\n      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1){\n        port_oam_render_routed_10(0, a0, a1, a2 + 0xc0 + data_ov004_020beb6c, a3, a4, 0x1000, 0x1000, 0, -1);\n        return;\n      }\n      port_oam_render_routed_10(0, a0, a1, a2, a3, a4, 0x1000, 0x1000, 0, -1);\n      return;\n    }\n  }\n  port_oam_render_routed_10(0, a0, a1, a2 + 0xc0 + data_ov004_020beb6c, a3, a4, 0x1000, 0x1000, 0, -1);\n  OAM::RenderSub((OamAttr*)a0, a1, a2, a3, a4);\n}\n'),
    ('extern "C" int func_ov004_020aff38(struct OamAttr* a0, int a1, int a2, int a3, int a4, Fix12i a5, int a6)\n{\n    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;\n    if (scene == 0) return 0;\n    if (scene->mMenuOpen == 0 && scene->OnHitByCannonBlastedChar() == 2) {\n        if (*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1) {\n            int hp = data_ov004_020beb6c;\n            if (a2 >= -0x100 - hp && a2 < -hp) {\n                return _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(0, a0, a1, a2 + 0xc0 + hp, a3, a4, a5, a6);\n            }\n            if (a2 >= -0x40 && a2 < 0xc0) {\n                return _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(0, a0, a1, a2, a3, a4, a5, a6);\n            }\n        }\n    } else {\n        int hp = data_ov004_020beb6c;\n        if (a2 >= -0x100 - hp && a2 < -hp) {\n            return _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(0, a0, a1, a2 + 0xc0 + hp, a3, a4, a5, a6);\n        }\n        if (a2 >= -0x40 && a2 < 0xc0) {\n            return _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(1, a0, a1, a2, a3, a4, a5, a6);\n        }\n    }\n    return 0;\n}\n',
     'extern "C" void *port_oam_render_routed_8(int, void *, int, int, int, int, int, int);  /* hostgen: hal/screen_gap.cpp shim */\nextern "C" int func_ov004_020aff38__rom(struct OamAttr* a0, int a1, int a2, int a3, int a4, Fix12i a5, int a6)\n{\n    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;\n    if (scene == 0) return 0;\n    if (scene->mMenuOpen == 0 && scene->OnHitByCannonBlastedChar() == 2) {\n        if (*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1) {\n            int hp = data_ov004_020beb6c;\n            if (a2 >= -0x100 - hp && a2 < -hp) {\n                return (int)(size_t)port_oam_render_routed_8(0, a0, a1, a2 + 0xc0 + hp, a3, a4, a5, a6);\n            }\n            if (a2 >= -0x40 && a2 < 0xc0) {\n                return (int)(size_t)port_oam_render_routed_8(0, a0, a1, a2, a3, a4, a5, a6);\n            }\n        }\n    } else {\n        int hp = data_ov004_020beb6c;\n        if (a2 >= -0x100 - hp && a2 < -hp) {\n            return (int)(size_t)port_oam_render_routed_8(0, a0, a1, a2 + 0xc0 + hp, a3, a4, a5, a6);\n        }\n        if (a2 >= -0x40 && a2 < 0xc0) {\n            return (int)(size_t)port_oam_render_routed_8(1, a0, a1, a2, a3, a4, a5, a6);\n        }\n    }\n    return 0;\n}\n'),
    ('void RenderOamBothScreens(void* attr, int x, int y, int palette, int priority, void* mtx){\n  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;\n  if(scene == 0) return;\n  if(scene->mMenuOpen == 0){\n    if(scene->OnHitByCannonBlastedChar() == 2){\n      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1){\n        OAM::Render(false, (OamAttr*)attr, x, y + 0xc0 + data_ov004_020beb6c, palette, priority, (Matrix2x2*)mtx);\n        return;\n      }\n      OAM::Render(false, (OamAttr*)attr, x, y, palette, priority, (Matrix2x2*)mtx);\n      return;\n    }\n  }\n  OAM::Render(false, (OamAttr*)attr, x, y + 0xc0 + data_ov004_020beb6c, palette, priority, (Matrix2x2*)mtx);\n  OAM::Render(true, (OamAttr*)attr, x, y, palette, priority, (Matrix2x2*)mtx);\n}\n',
     'void *port_oam_render_routed_mtx(int, void *, int, int, int, int, void *);  /* hostgen: hal/screen_gap.cpp shim */\nvoid RenderOamBothScreens__rom(void* attr, int x, int y, int palette, int priority, void* mtx){\n  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;\n  if(scene == 0) return;\n  if(scene->mMenuOpen == 0){\n    if(scene->OnHitByCannonBlastedChar() == 2){\n      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1){\n        port_oam_render_routed_mtx(false, (OamAttr*)attr, x, y + 0xc0 + data_ov004_020beb6c, palette, priority, (Matrix2x2*)mtx);\n        return;\n      }\n      port_oam_render_routed_mtx(false, (OamAttr*)attr, x, y, palette, priority, (Matrix2x2*)mtx);\n      return;\n    }\n  }\n  port_oam_render_routed_mtx(false, (OamAttr*)attr, x, y + 0xc0 + data_ov004_020beb6c, palette, priority, (Matrix2x2*)mtx);\n  port_oam_render_routed_mtx(true, (OamAttr*)attr, x, y, palette, priority, (Matrix2x2*)mtx);\n}\n'),
    ('void func_ov004_020b023c(void* a0, int a1, int a2, int a3, void* a4){\n  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;\n  if(scene == 0) return;\n  if(scene->mMenuOpen == 0){\n    if(scene->OnHitByCannonBlastedChar() == 2){\n      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1){\n        OAM::Render(0, (OamAttr*)a0, a1, a2 + 0xc0 + data_ov004_020beb6c, a3, -1, (Matrix2x2*)a4);\n        return;\n      }\n      OAM::Render(0, (OamAttr*)a0, a1, a2, a3, -1, (Matrix2x2*)a4);\n      return;\n    }\n  }\n  OAM::Render(0, (OamAttr*)a0, a1, a2 + 0xc0 + data_ov004_020beb6c, a3, -1, (Matrix2x2*)a4);\n  OAM::Render(1, (OamAttr*)a0, a1, a2, a3, -1, (Matrix2x2*)a4);\n}\n',
     'void *port_oam_render_routed_mtx(int, void *, int, int, int, int, void *);  /* hostgen: hal/screen_gap.cpp shim */\nvoid func_ov004_020b023c__rom(void* a0, int a1, int a2, int a3, void* a4){\n  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;\n  if(scene == 0) return;\n  if(scene->mMenuOpen == 0){\n    if(scene->OnHitByCannonBlastedChar() == 2){\n      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1){\n        port_oam_render_routed_mtx(0, (OamAttr*)a0, a1, a2 + 0xc0 + data_ov004_020beb6c, a3, -1, (Matrix2x2*)a4);\n        return;\n      }\n      port_oam_render_routed_mtx(0, (OamAttr*)a0, a1, a2, a3, -1, (Matrix2x2*)a4);\n      return;\n    }\n  }\n  port_oam_render_routed_mtx(0, (OamAttr*)a0, a1, a2 + 0xc0 + data_ov004_020beb6c, a3, -1, (Matrix2x2*)a4);\n  port_oam_render_routed_mtx(1, (OamAttr*)a0, a1, a2, a3, -1, (Matrix2x2*)a4);\n}\n'),
    ('void func_ov004_020b0380(void* a0, int a1, int a2, void* a3){\n  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;\n  if(scene == 0) return;\n  if(scene->mMenuOpen == 0){\n    if(scene->OnHitByCannonBlastedChar() == 2){\n      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1){\n        OAM::Render(0, (OamAttr*)a0, a1, a2 + 0xc0 + data_ov004_020beb6c, -1, -1, (Matrix2x2*)a3);\n        return;\n      }\n      OAM::Render(0, (OamAttr*)a0, a1, a2, -1, -1, (Matrix2x2*)a3);\n      return;\n    }\n  }\n  OAM::Render(0, (OamAttr*)a0, a1, a2 + 0xc0 + data_ov004_020beb6c, -1, -1, (Matrix2x2*)a3);\n  OAM::Render(1, (OamAttr*)a0, a1, a2, -1, -1, (Matrix2x2*)a3);\n}\n',
     'void *port_oam_render_routed_mtx(int, void *, int, int, int, int, void *);  /* hostgen: hal/screen_gap.cpp shim */\nvoid func_ov004_020b0380__rom(void* a0, int a1, int a2, void* a3){\n  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;\n  if(scene == 0) return;\n  if(scene->mMenuOpen == 0){\n    if(scene->OnHitByCannonBlastedChar() == 2){\n      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1){\n        port_oam_render_routed_mtx(0, (OamAttr*)a0, a1, a2 + 0xc0 + data_ov004_020beb6c, -1, -1, (Matrix2x2*)a3);\n        return;\n      }\n      port_oam_render_routed_mtx(0, (OamAttr*)a0, a1, a2, -1, -1, (Matrix2x2*)a3);\n      return;\n    }\n  }\n  port_oam_render_routed_mtx(0, (OamAttr*)a0, a1, a2 + 0xc0 + data_ov004_020beb6c, -1, -1, (Matrix2x2*)a3);\n  port_oam_render_routed_mtx(1, (OamAttr*)a0, a1, a2, -1, -1, (Matrix2x2*)a3);\n}\n'),
]


# Three members main now defines in its class TU while port/faces_sync.txt
# keeps a FORWARD face for them (the decorated name, forwarding to the port's
# flat host copy, which carries the port's own repair): daTor_c::InitResources
# (port/unmatched/Tornado_HostSites.cpp), daDoor_c::InitResources and
# daTree_c::Behavior. Two definitions of the decorated name otherwise; the
# host copy is what ran on the port line, so main's body is parked.
ROWS_05[("PARK_SYMBOL", "daTor_c")] = ["_ZN7daTor_c13InitResourcesEv"]
ROWS_05[("PARK_SYMBOL", "daDoor_c")] = ["_ZN8daDoor_c13InitResourcesEv"]
ROWS_05[("PARK_SYMBOL", "d_a_tree")] = ["_ZN8daTree_c8BehaviorEv"]


ROWS = {}
for _part in (ROWS_00, ROWS_01, ROWS_02, ROWS_03, ROWS_04, ROWS_05):
    for _k, _v in _part.items():
        assert _k not in ROWS, "row %r re-derived twice" % (_k,)
        ROWS[_k] = _v
