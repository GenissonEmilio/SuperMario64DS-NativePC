# fold/arm9-g2x-1021 — arm9/G2x promotion handoff

## What landed

`G2x` — the all-static 2D-engine blend/affine writer set. All three of its
shards folded into `src/engine/gx/G2x.cpp`, licensing
`0x02055238..0x0205532c`:

- `_ZN3G2x18SetBlendBrightnessEPVtts` — brightness/fade registers; `amt`'s
  sign selects fade-to-black (0xc0) vs fade-to-white (0x80)
- `_ZN3G2x13SetBlendAlphaEPVttttj` — 32-bit packed write through a u16*;
  the trailing arg is genuinely `j` (ldr, not ldrh — see include/G2x.h)
- `_ZN3G2x12SetBGyAffineEPVtP9Matrix2x2iiii` — packs the matrix's 20.12
  terms into halfword pairs (>>4 is the format conversion) and resolves
  the rotate-about/rotate-to point pair into the single hardware origin

No lifecycle, no vtable, no RTTI — `G2x` has no instance state (every
member is static; r0 is the register pointer, per include/G2x.h).
`Matrix2x2` stays a file-local `int m[4]` view — the header forward-declares
it; the only field access in the class is SetBGyAffine's indexing (same
shape include/OAM.h uses).

## Verification

- `tubuild.py verify arm9/G2x`: 3/3 MATCH, objisolate clean,
  reloc-destinations clean, ROM-ascending emission → TEXT-VERIFIED.
  Nothing extra emits — a static-only class produces no variants.
- Boundaries: `func_020551f0` below and `func_0205532c` /
  `Geometry_MatrixMultiply3x3` above are unenrolled free code.
- `notes/converted-tier.md`'s evidence row repointed to the promoted path.

## Why this shape

The three shards were already real static-member definitions with the
mangled-name parameter types the header documents; the TU keeps the shard
bodies verbatim and writes them ROM-descending so deferred codegen emits
the ascending layout.
