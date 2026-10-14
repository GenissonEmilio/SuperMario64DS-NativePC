# fold/arm9-clpsblockref-1020 — arm9/CLPS_BlockRef promotion handoff

## What landed

`CLPS_BlockRef` — the one-word value handle `dBgW_Kc` carries (a raw
`CLPS_Block` pointer with value semantics). All three of its shards folded
into `src/engine/collision/CLPS_BlockRef.cpp`, licensing
`0x0203821c..0x02038234`:

- `_ZN13CLPS_BlockRefaSER10CLPS_Block` — copy assignment onto the pointer
- `_ZN13CLPS_BlockRefD1Ev` — trivial destructor variant
- `_ZN13CLPS_BlockRefC1Ev` — constructor (`ptr(0)`)

The TU is the three member definitions. mwccarm emits the constructor's
C1,C2 pair and the destructor's D2,D1 pair; source order is constructor,
destructor, operator= so deferred codegen's reverse-source emission lands
the ROM's operator=, D1, C1 layout.

## Compiler-only output

- `_ZN13CLPS_BlockRefC2Ev`, `_ZN13CLPS_BlockRefD2Ev` — the base-subobject
  variants the member definitions emit beside C1/D1; nothing constructs or
  destroys a `CLPS_BlockRef` as a base subobject, so both deadstrip
  unenrolled.
- No D0 (non-virtual dtor, nothing deletes one) and no `_ZTV`/`_ZTI`/`_ZTS`
  (no virtuals, no key function).

## Verification

- `tubuild.py verify arm9/CLPS_BlockRef`: 3/3 MATCH, objisolate clean,
  reloc-destinations clean, ROM-ascending emission → TEXT-VERIFIED.
- `port_refcheck`: the slice_gate8 listing moved to the promoted path.
- Boundaries: `func_020381cc` below is the free table-walking helper the
  arm9/dBgPi manifest attributes to this neighbourhood (not a member);
  `func_02038234` above is likewise unenrolled free code.
