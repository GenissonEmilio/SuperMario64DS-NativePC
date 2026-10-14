# fold/arm9-dbgpc-1020 — arm9/dBgPc promotion handoff

## What landed

`dBgPc` — the non-polymorphic base `dBgPi` carries at +0x04 (a five-word
`SurfaceInfo` record). All four of its shards folded into
`src/engine/collision/dBgPc.cpp`, licensing `0x02037ee4..0x02037f44`:

- `_ZN5dBgPcD2Ev` / `D1Ev` — the destructor's sub-object variants
- `_ZN5dBgPcC1Ev` / `C2Ev` — the constructor's sub-object/base variants

The TU is two definitions: `~dBgPc()` and `dBgPc()`. mwccarm emits the D2,D1
and C1,C2 variant pairs from them; C1 and C2 are byte-identical because a
non-polymorphic base stores no vptr. The base-object ctor runs the five
surface stores the shard files each had.

## Compiler-only output

- `_ZN11SurfaceInfoD1Ev` — the destructor definition drags in `surface`'s
  implicit member destructor; trivially empty, unreferenced by the enrolled
  variants (the ROM's D2/D1 are bare 4-byte returns), no cartridge home.
  Manifest records it as `deadstrip`.
- No `D0` emits: the destructor is non-virtual and nothing deletes a
  `dBgPc`, so mwccarm never produces the deleting variant.
- No `_ZTV`/`_ZTI`/`_ZTS`: non-polymorphic, no key function. The
  cartridge's `_ZTI5dBgPc` record (where referenced) is emitted by the
  derived class's TU as base-info, not here.

## Verification

- `tubuild.py verify arm9/dBgPc`: 4/4 MATCH, objisolate clean,
  reloc-destinations clean → TEXT-VERIFIED.
- Emission-order advisory on ordinal pair (1,2) is the expected
  compiler-owned destructor-group ordering (pilot report sec 3).
- `port_refcheck`: the slice_gate8 listing moved to the promoted path.
- Boundaries: `func_02037eb0` below belongs to a different record walk;
  `_ZNK5dBgPi9GetClsnIDEv` at `0x02037f4c` begins arm9/dBgPi's own run
  (fold/arm9-dbgpi owns that class — no overlap).

## Why this shape

The four shards were already real member definitions bound one-variant-per-
file by delinks; the promoted file says the same thing once and lets the
compiler emit the ABI surface the way the original TU did.
