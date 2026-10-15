#!/usr/bin/env python3
"""Turn the ROM-CLEAN manifest into an extraction recipe the portable kit ships.

ROM-CLEAN moved every ROM byte out of walk_window.exe and into
build/assets/romdata.bin, which the dev build generates from the repo's
extracted/ images. A player has no extracted/ -- only their own cartridge dump.
The portable kit's extract_assets.ps1 therefore needs to know, for every byte
range of romdata.bin, WHERE in the (decompressed) ROM images that range lives.
That mapping is this file's output: romdata.recipe.tsv, shippable because it
holds only names, offsets and hashes -- zero Nintendo bytes.

    python tools/romblob_recipe.py <manifest> <recipe_out>

Every manifest `sym` row resolves to (source image, source offset):

    tag romdata     -> extracted/arm9_dec.bin, offset = addr - 0x02004000
    tag ovNNN_syms  -> overlay NNN, offset = addr - base_address(NNN)
    tag ovNNN       -> port_ovNNN_image is the whole image at offset 0;
                       any other name resolves like ovNNN_syms (ov002's
                       from-list part carries the tag ov002)

Addresses come from config/arm9/symbols.txt / config/arm9/overlays/*/
symbols.txt, falling back to the _02xxxxxx hex embedded in generated names.
This script does NOT trust its own resolution: every recipe row is sliced out
of the local extracted images and hashed against the manifest's per-sym sha,
the rows must tile the blob exactly, and the re-concatenation must hash to the
manifest's whole-file sha. A mismatch names the symbol and exits non-zero, so
a wrong recipe cannot be produced silently.
"""

import hashlib
import pathlib
import re
import sys

ARM9_BASE = 0x02004000

ADDR_RE = re.compile(r"addr:0x([0-9a-f]{8})")
NAME_HEX_RE = re.compile(r"_(02[0-9a-f]{6})$")


def load_symbols(path):
    table = {}
    for line in path.read_text().splitlines():
        parts = line.split()
        if not parts:
            continue
        m = ADDR_RE.search(line)
        if m:
            table.setdefault(parts[0], int(m.group(1), 16))
    return table


def main():
    root = pathlib.Path(__file__).resolve().parents[2]
    manifest_path = pathlib.Path(sys.argv[1])
    recipe_path = pathlib.Path(sys.argv[2])
    manifest = manifest_path.read_text().splitlines()

    head = manifest[0].split()
    whole_sha, total = head[3], int(head[4])

    arm9 = (root / "extracted/arm9_dec.bin").read_bytes()

    ytxt = (root / "extracted/dsd/arm9_overlays/overlays.yaml").read_text()
    ov_base = {}
    cur = None
    for line in ytxt.splitlines():
        m = re.match(r"\s*-?\s*id:\s*(\d+)", line)
        if m:
            cur = int(m.group(1))
        m = re.match(r"\s*base_address:\s*(\d+)", line)
        if m and cur is not None:
            ov_base.setdefault(cur, int(m.group(1)))

    ov_img = {}

    def overlay(ovid):
        if ovid not in ov_img:
            p = root / f"extracted/overlays/overlay_{ovid:04d}.bin"
            if not p.exists():
                p = root / f"extracted/dsd/arm9_overlays/ov{ovid:03d}.bin"
            ov_img[ovid] = p.read_bytes()
        return ov_img[ovid]

    arm9_syms = None
    ov_syms = {}

    def addr_of(tag, name, ovid):
        nonlocal arm9_syms
        if ovid is None:
            if arm9_syms is None:
                arm9_syms = load_symbols(root / "config/arm9/symbols.txt")
            table = arm9_syms
        else:
            if ovid not in ov_syms:
                ov_syms[ovid] = load_symbols(
                    root / f"config/arm9/overlays/ov{ovid:03d}/symbols.txt")
            table = ov_syms[ovid]
        if name in table:
            return table[name]
        # a name the port itself pins (port/<tag>.txt "name@0xADDR"), for a symbol config renamed or dropped
        side = root / "port" / f"{tag}.txt"
        if side.exists():
            mm = re.search(r"(?m)^" + re.escape(name) + r"@0x([0-9a-fA-F]+)", side.read_text())
            if mm:
                return int(mm.group(1), 16)
        m = NAME_HEX_RE.search(name)
        if m:
            return int(m.group(1), 16)
        sys.exit(f"romblob_recipe: cannot resolve {name} ({tag}) to an address")

    rows = []           # (blob_off, size, src, src_off, sha)
    bad = 0
    for line in manifest:
        if not line.startswith("sym\t"):
            continue
        _, tag, name, off, size, ssha = line.split("\t")
        off, size = int(off), int(size)

        if tag == "romdata":
            src, img = "arm9", arm9
            src_off = addr_of(tag, name, None) - ARM9_BASE
        else:
            ovid = int(re.match(r"ov(\d+)", tag).group(1))
            src, img = f"ov{ovid:03d}", overlay(ovid)
            if name == f"port_ov{ovid:03d}_image":
                src_off = 0
            else:
                src_off = addr_of(tag, name, ovid) - ov_base[ovid]

        # bss symbols sit past the end of the overlay image (the image is the
        # file-backed bytes only); the emitter reads them as zero. Recipe
        # semantics are therefore copy-what-exists, zero-fill-the-tail, and
        # extract_assets.ps1 must do the same.
        got = img[src_off:src_off + size]
        got = got + b"\x00" * (size - len(got))
        if hashlib.sha256(got).hexdigest() != ssha:
            print(f"romblob_recipe: {name} ({tag}) does not match its source "
                  f"slice {src}+{src_off:#x}")
            bad += 1
            continue
        rows.append((off, size, src, src_off, ssha))

    if bad:
        sys.exit(f"romblob_recipe: {bad} symbol(s) failed to resolve -- "
                 "recipe NOT written")

    # The rows must tile [0, total) with no gap or overlap, and slicing the
    # sources by the recipe alone must rebuild the exact blob. This is the
    # same reconstruction the .ps1 will do, so passing here proves the recipe
    # is sufficient on its own.
    rows.sort(key=lambda r: r[0])
    pos = 0
    blob = bytearray()
    srcs = {"arm9": arm9}
    for off, size, src, src_off, _ in rows:
        if off != pos:
            sys.exit(f"romblob_recipe: gap/overlap at blob offset {pos} "
                     f"(next row starts at {off})")
        if src not in srcs:
            srcs[src] = overlay(int(src[2:]))
        piece = srcs[src][src_off:src_off + size]
        blob += piece + b"\x00" * (size - len(piece))
        pos += size
    if pos != total or hashlib.sha256(bytes(blob)).hexdigest() != whole_sha:
        sys.exit("romblob_recipe: reconstruction does not hash to the "
                 "manifest whole-file sha -- recipe NOT written")

    out = [f"# romdata-recipe v1 {whole_sha} {total}"]
    for off, size, src, src_off, ssha in rows:
        out.append(f"{off}\t{size}\t{src}\t{src_off}\t{ssha}")
    recipe_path.write_text("\n".join(out) + "\n", encoding="ascii")
    needed = sorted(s for s in srcs if s != "arm9")
    print(f"romblob_recipe: {len(rows)} rows, sources arm9 + "
          f"{len(needed)} overlays -> {recipe_path}")


if __name__ == "__main__":
    main()
