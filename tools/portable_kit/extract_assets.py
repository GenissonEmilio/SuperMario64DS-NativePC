#!/usr/bin/env python3
"""Unpack the game data out of your own Super Mario 64 DS cartridge dump.

This is the same job as extract_assets.ps1, for a machine with no Windows
PowerShell (Linux, Steam Deck, or a Wine/Proton prefix). It needs only Python 3
(3.6 or newer) and the standard library. It writes exactly the same files in
exactly the same layout:

    extracted/dsd/files/<path>     every cartridge file, byte for byte
    build/assets/files.tsv         file id -> path index
    build/assets/handles.tsv       asset handle -> file id (read out of overlay 0)
    build/assets/nitrofs.tsv       where the card's own tables live in the dump
    build/assets/nitrofs_fnt.bin   the card's file name table, verbatim
    build/assets/nitrofs_fat.bin   the card's file allocation table, verbatim
    build/assets/nitrofs_ovt9.bin  the ARM9 overlay table, verbatim
    build/assets/nitrofs_ovt7.bin  the ARM7 overlay table, verbatim (if the card has one)
    build/assets/romdata.bin       code-side data tables rebuilt from the dump
    build/assets/romdata.manifest  copied from the kit

Nothing is downloaded and nothing is installed. Compressed files are left
compressed, as the cartridge has them.

Usage:
    python3 extract_assets.py                       (looks in "PLACE YOUR ROM HERE")
    python3 extract_assets.py --rom /path/to/sm64ds.nds [--destination /path/to/kit]

Exit status 0 on success, 1 on any problem (with one plain line saying why).
The "##PROGRESS" and "##REASON" lines are for a program reading the output.
"""
import argparse
import hashlib
import os
import re
import shutil
import struct
import sys


def prog(pct, phase):
    pct = max(0, min(100, int(pct)))
    print("##PROGRESS %d %s" % (pct, phase), flush=True)


def stop(text, blocked=False):
    one = re.sub(r"\s+", " ", re.sub(r"[^ -~]", " ", text)).strip()
    if len(one) > 400:
        one = one[:397] + "..."
    print("##REASON " + one)
    print()
    print(text)
    print()
    if not blocked:
        print("This kit contains no game data of its own, by design. It needs a dump")
        print("of a Super Mario 64 DS cartridge that you own. See README.txt.")
    sys.exit(1)


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def expand_blz(data):
    """Bottom-LZ decompression, the form the DS uses for ARM9 and overlays."""
    if len(data) < 8:
        return bytes(data)
    footer = u32(data, len(data) - 8)
    extra = u32(data, len(data) - 4)
    if extra == 0:
        return bytes(data)          # flagged, but stored plain
    header_len = footer >> 24
    comp_len = footer & 0xFFFFFF
    if header_len < 8 or header_len > len(data) or comp_len > len(data):
        raise ValueError("overlay 0 has an unreadable compression header")
    if comp_len >= len(data):
        comp_len = len(data)
    passthrough = len(data) - comp_len
    clen = comp_len - header_len
    out_len = len(data) + extra - passthrough
    out = bytearray(out_len)
    read = 0
    done = 0
    flags = 0
    mask = 1
    base = passthrough + clen - 1
    while done < out_len:
        if mask == 1:
            if read >= comp_len:
                raise ValueError("overlay 0 ended mid-stream")
            flags = data[base - read]
            read += 1
            mask = 0x80
        else:
            mask >>= 1
        if flags & mask:
            b1 = data[base - read]
            read += 1
            b2 = data[base - read]
            read += 1
            length = (b1 >> 4) + 3
            disp = (((b1 & 0x0F) << 8) | b2) + 3
            if disp > done:
                if done < 2:
                    raise ValueError("overlay 0 back-references data that is not there yet")
                disp = 2
            src = done - disp
            for _ in range(length):
                out[out_len - 1 - done] = out[out_len - 1 - src]
                src += 1
                done += 1
        else:
            out[out_len - 1 - done] = data[base - read]
            read += 1
            done += 1
    return bytes(data[:passthrough]) + bytes(out)


KINDS = {".bca": "animation", ".bmd": "model", ".btp": "texture-sequence",
         ".kcl": "collision", ".narc": "archive", ".sdat": "sound-archive",
         ".bin": "data"}


def kind_of(path):
    ext = os.path.splitext(path)[1].lower()
    return KINDS.get(ext, "file")


def write_bytes(path, data):
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--rom", help="path to your .nds dump")
    ap.add_argument("--destination", help="kit folder to write extracted/ and build/ into")
    args = ap.parse_args()

    dest = os.path.abspath(args.destination or os.path.dirname(os.path.abspath(__file__)))
    try:
        os.makedirs(dest, exist_ok=True)
        probe = os.path.join(dest, ".write-probe-%d" % os.getpid())
        with open(probe, "wb") as f:
            f.write(b"\0")
        os.remove(probe)
    except OSError as e:
        stop("This folder cannot be written to (%s): %s" % (dest, e.strerror or e), True)

    rom = args.rom
    if not rom:
        found = []
        for name in ("PLACE YOUR ROM HERE", "PLACE EU ROM HERE"):
            cand = os.path.join(dest, name)
            if os.path.isdir(cand):
                found = sorted(os.path.join(cand, n) for n in os.listdir(cand)
                               if n.lower().endswith(".nds"))
                if found:
                    break
        if not found:
            found = sorted(os.path.join(dest, n) for n in os.listdir(dest)
                           if n.lower().endswith(".nds") and os.path.isfile(os.path.join(dest, n)))
        if not found:
            stop("No .nds file found in '%s' or next to this script." %
                 os.path.join(dest, "PLACE YOUR ROM HERE"))
        if len(found) > 1:
            stop("More than one .nds file is sitting here. Leave only the Super Mario 64 DS "
                 "one, or name it: extract_assets.py --rom <file>")
        rom = found[0]
    if not os.path.isfile(rom):
        stop("The .nds file is not there any more: %s." % rom)

    print("Reading %s" % os.path.basename(rom))
    try:
        with open(rom, "rb") as f:
            rb = f.read()
    except OSError as e:
        stop("The .nds file could not be read: %s" % (e.strerror or e))
    if len(rb) < 0x4000:
        stop("That file is only %d bytes, too small to be a DS cartridge dump." % len(rb))

    title = rb[0x00:0x0C].rstrip(b"\0").decode("ascii", "replace")
    code = rb[0x0C:0x10].decode("ascii", "replace")
    if title != "S.MARIO64DS" or not re.match(r"^ASM.$", code):
        stop("That is not a Super Mario 64 DS dump (it says title '%s', game code '%s'). "
             "Use a dump of a Super Mario 64 DS cartridge." %
             (re.sub(r"[^ -~]", ".", title), re.sub(r"[^ -~]", ".", code)))
    regions = {"E": "North America", "P": "Europe", "J": "Japan",
               "K": "Korea", "C": "China", "U": "Australia"}
    region = regions.get(code[3], "region '%s'" % code[3])
    if code != "ASMP":
        stop("This is the %s ROM (game code %s). This port is built from the European ROM "
             "(ASMP), so its data will not come out of a %s cartridge." % (region, code, region))

    fnt_off, fnt_size = u32(rb, 0x40), u32(rb, 0x44)
    fat_off, fat_size = u32(rb, 0x48), u32(rb, 0x4C)
    ovt_off, ovt_size = u32(rb, 0x50), u32(rb, 0x54)
    ovt7_off, ovt7_size = u32(rb, 0x58), u32(rb, 0x5C)
    used = u32(rb, 0x80)
    for what, off, size in (("file name table", fnt_off, fnt_size),
                            ("file allocation table", fat_off, fat_size),
                            ("overlay table", ovt_off, ovt_size)):
        if off == 0 or off + size > len(rb):
            stop("This dump is truncated: its %s runs past the end of the file. "
                 "Re-dump the cartridge." % what)
    if not (ovt7_off == 0 and ovt7_size == 0):
        if ovt7_off == 0 or ovt7_size == 0 or ovt7_off + ovt7_size > len(rb):
            stop("This dump is truncated: its ARM7 overlay table runs past the end of the "
                 "file. Re-dump the cartridge.")
    if used > len(rb):
        stop("This dump is incomplete: the header says the cartridge holds %d bytes but the "
             "file is only %d. Re-dump the cartridge." % (used, len(rb)))
    print("Super Mario 64 DS (%s, %s), %d MB" % (code, region, round(len(rb) / 1048576.0)))
    prog(2, "Reading the file tables")

    fat_count = fat_size // 8
    fat_start = [u32(rb, fat_off + i * 8) for i in range(fat_count)]
    fat_end = [u32(rb, fat_off + i * 8 + 4) for i in range(fat_count)]

    # Walk the file name table breadth first, as the cartridge's own tools do.
    paths = {}
    queue = [(0, "")]
    visited = {0}
    qi = 0
    while qi < len(queue):
        dir_id, base = queue[qi]
        qi += 1
        entry = fnt_off + dir_id * 8
        if entry + 8 > len(rb):
            stop("This dump's file name table is damaged. Re-dump the cartridge.")
        o = fnt_off + u32(rb, entry)
        file_id = u16(rb, entry + 4)
        while True:
            if o >= len(rb):
                stop("This dump's file name table is damaged. Re-dump the cartridge.")
            t = rb[o]
            o += 1
            if t == 0:
                break
            ln = t & 0x7F
            need = ln + (2 if t & 0x80 else 0)
            if o + need > len(rb):
                stop("This dump's file name table is damaged. Re-dump the cartridge.")
            name = rb[o:o + ln].decode("ascii", "replace")
            o += ln
            full = base + "/" + name if base else name
            if t & 0x80:
                sub = u16(rb, o) & 0x0FFF
                o += 2
                if sub not in visited:
                    visited.add(sub)
                    queue.append((sub, full))
            else:
                paths[file_id] = full
                file_id += 1
    if not paths:
        stop("This dump has no named files in it. Re-dump the cartridge.")

    need_bytes = 0
    for fid in sorted(paths):
        if fid < 0 or fid >= fat_count:
            stop("This dump is damaged: its file name table names a file (%s) that its file "
                 "allocation table does not have. Re-dump the cartridge." % paths[fid])
        ln = fat_end[fid] - fat_start[fid]
        if ln < 0 or fat_start[fid] + ln > len(rb):
            stop("This dump is damaged: its file table points at data past the end of the "
                 "file (file %d, '%s'). Re-dump the cartridge." % (fid, paths[fid]))
        need_bytes += ln
    try:
        free = shutil.disk_usage(dest).free
        want = need_bytes + 16 * 1048576
        if free < want:
            stop("There is not enough free space to unpack the game data. About %d MB is "
                 "needed and %d MB is free. Free up some space and try again." %
                 (round(want / 1048576.0), round(free / 1048576.0)), True)
    except OSError:
        print("    (could not measure free disk space here; carrying on)")

    files_root = os.path.join(dest, "extracted", "dsd", "files")
    print("Writing %d files to %s" % (len(paths), files_root))
    total = len(paths)
    written = 0
    nbytes = 0
    for fid in sorted(paths):
        target = os.path.join(files_root, *paths[fid].split("/"))
        s, e = fat_start[fid], fat_end[fid]
        write_bytes(target, rb[s:e])
        written += 1
        nbytes += e - s
        if written % 500 == 0:
            print("    %d of %d" % (written, total))
            prog(2 + int(68 * written / total), "Unpacking file %d of %d" % (written, total))
    print("    %d files, %.1f MB" % (written, nbytes / 1048576.0))
    prog(70, "Unpacked %d files" % written)

    # Overlay 0 carries the asset-handle table.
    if ovt_size < 32:
        stop("This dump has no ARM9 overlays. Re-dump the cartridge.")
    ov0_ram = u32(rb, ovt_off + 4)
    ov0_fid = u32(rb, ovt_off + 24)
    ov0_flags = u32(rb, ovt_off + 28) >> 24
    if ov0_fid >= fat_count:
        stop("This dump is damaged: its overlay table points at a file its file allocation "
             "table does not have. Re-dump the cartridge.")
    ov0 = rb[fat_start[ov0_fid]:fat_end[ov0_fid]]
    prog(71, "Decompressing overlay 0")
    if ov0_flags & 1:
        try:
            ov0 = expand_blz(ov0)
        except ValueError as ex:
            stop("Could not read overlay 0 of this dump (%s). Re-dump the cartridge." % ex)
    handle_addr = 0x020BD4B8
    handle_count = 0x80A
    table_off = handle_addr - ov0_ram
    if table_off < 0 or table_off + handle_count * 4 > len(ov0):
        stop("This dump is Super Mario 64 DS but not a revision this build knows: its "
             "overlay 0 has no asset-handle table where one is expected.")
    id_by_path = {p: i for i, p in paths.items()}
    print("Reading the asset-handle table out of overlay 0")
    prog(72, "Reading the asset-handle table")
    hrows = ["handle\thex_handle\tfile_id\thex_file_id\tpath\tkind\tsize\n"]
    for h in range(handle_count):
        if h % 400 == 0 and h > 0:
            prog(72 + int(8 * h / handle_count), "Cataloguing asset handle %d of %d" % (h, handle_count))
        ptr = u32(ov0, table_off + h * 4)
        so = ptr - ov0_ram
        if so < 0 or so >= len(ov0):
            stop("This dump is Super Mario 64 DS but not a revision this build knows: asset "
                 "handle %d points outside overlay 0." % h)
        end = ov0.find(b"\0", so)
        if end < 0:
            end = len(ov0)
        apath = ov0[so:end].decode("ascii", "replace")
        if apath not in id_by_path:
            stop("This dump is Super Mario 64 DS but not a revision this build knows: asset "
                 "handle %d names '%s', which its filesystem does not have." % (h, apath))
        fid = id_by_path[apath]
        size = fat_end[fid] - fat_start[fid]
        hrows.append("%d\t0x%04x\t%d\t0x%04x\t%s\t%s\t%d\n" %
                     (h, h, fid, fid, apath, kind_of(apath), size))
    frows = ["file_id\thex_id\tpath\tkind\tsize\n"]
    for fid in sorted(paths):
        size = fat_end[fid] - fat_start[fid]
        frows.append("%d\t0x%04x\t%s\t%s\t%d\n" % (fid, fid, paths[fid], kind_of(paths[fid]), size))

    assets = os.path.join(dest, "build", "assets")
    os.makedirs(assets, exist_ok=True)

    def write_text(name, text):
        with open(os.path.join(assets, name), "w", encoding="utf-8", newline="") as f:
            f.write(text)

    write_text("files.tsv", "".join(frows))
    write_text("handles.tsv", "".join(hrows))
    print("    %d files and %d handles catalogued" % (len(paths), handle_count))

    for what, size in (("file name table", fnt_size), ("file allocation table", fat_size)):
        if size == 0:
            stop("This dump has an empty %s, so the game could not look up any of its "
                 "files. Re-dump the cartridge." % what)
    write_bytes(os.path.join(assets, "nitrofs_fnt.bin"), rb[fnt_off:fnt_off + fnt_size])
    write_bytes(os.path.join(assets, "nitrofs_fat.bin"), rb[fat_off:fat_off + fat_size])
    for name, off, size in (("nitrofs_ovt9.bin", ovt_off, ovt_size),
                            ("nitrofs_ovt7.bin", ovt7_off, ovt7_size)):
        if size:
            write_bytes(os.path.join(assets, name), rb[off:off + size])
    write_text("nitrofs.tsv",
               "key\tvalue\nfnt_offset\t%d\nfnt_size\t%d\nfat_offset\t%d\nfat_size\t%d\n"
               "ovt9_offset\t%d\novt9_size\t%d\novt7_offset\t%d\novt7_size\t%d\n" %
               (fnt_off, fnt_size, fat_off, fat_size, ovt_off, ovt_size, ovt7_off, ovt7_size))
    print("    name table %d bytes, allocation table %d bytes, overlay table %d bytes" %
          (fnt_size, fat_size, ovt_size))

    # romdata.bin, rebuilt from the recipe that ships with the kit.
    prog(80, "Rebuilding romdata.bin")
    recipe = os.path.join(dest, "romdata.recipe.tsv")
    if not os.path.isfile(recipe):
        recipe = os.path.join(assets, "romdata.recipe.tsv")
    if not os.path.isfile(recipe):
        stop("This kit is incomplete: romdata.recipe.tsv is missing, so the game data cannot "
             "be rebuilt. Download the kit again and unpack the whole zip into one folder "
             "(keep the files together).", True)
    with open(recipe, "r", encoding="utf-8") as f:
        lines = f.read().splitlines()
    m = re.match(r"^# romdata-recipe v1 ([0-9a-f]{64}) (\d+)$", lines[0]) if lines else None
    if len(lines) < 2 or not m:
        stop("This kit's romdata.recipe.tsv is damaged, so the game data cannot be rebuilt. "
             "Download the kit again.", True)
    want_sha = m.group(1)
    blob_total = int(m.group(2))
    rows = []
    for line in lines:
        if line.startswith("#") or not line.strip():
            continue
        f = line.split("\t")
        if len(f) < 4:
            stop("This kit's romdata.recipe.tsv is damaged (a line is incomplete), so the "
                 "game data cannot be rebuilt. Download the kit again.", True)
        rows.append(f)
    needed = []
    for f in rows:
        if f[2] not in needed:
            needed.append(f[2])
    ov_by_name = {}
    for i in range(ovt_size // 32):
        e = ovt_off + i * 32
        ov_by_name["ov%03d" % u32(rb, e)] = (u32(rb, e + 24), u32(rb, e + 28) >> 24)
    print("Rebuilding romdata.bin (%d images to decompress, please wait)" % len(needed))
    images = {}
    for n, src in enumerate(needed):
        prog(80 + int(17 * n / len(needed)), "Decompressing %s (%d of %d)" % (src, n + 1, len(needed)))
        if src == "arm9":
            a_off, a_size = u32(rb, 0x20), u32(rb, 0x2C)
            try:
                images[src] = expand_blz(rb[a_off:a_off + a_size])
            except ValueError as ex:
                stop("Could not read this dump's program data (%s). Re-dump the cartridge." % ex)
        else:
            if src not in ov_by_name:
                stop("This dump is Super Mario 64 DS but not a revision this build knows: it "
                     "has no overlay '%s'." % src)
            fid, flags = ov_by_name[src]
            if fid >= fat_count:
                stop("This dump is damaged: its overlay table points at a file its file "
                     "allocation table does not have (overlay '%s'). Re-dump the cartridge." % src)
            ln = fat_end[fid] - fat_start[fid]
            if ln < 0 or fat_start[fid] + ln > len(rb):
                stop("This dump is damaged: overlay '%s' runs past the end of the file. "
                     "Re-dump the cartridge." % src)
            raw = rb[fat_start[fid]:fat_start[fid] + ln]
            if flags & 1:
                try:
                    raw = expand_blz(raw)
                except ValueError as ex:
                    stop("Could not read overlay '%s' of this dump (%s). Re-dump the cartridge." % (src, ex))
            images[src] = bytes(raw)
        print("    %s (%d KB)" % (src, round(len(images[src]) / 1024.0)))
    prog(98, "Assembling and checking romdata.bin")
    blob = bytearray(blob_total)
    for f in rows:
        off, size, src_off = int(f[0]), int(f[1]), int(f[3])
        img = images[f[2]]
        if off < 0 or size < 0 or src_off < 0 or off + size > blob_total:
            stop("This kit's romdata.recipe.tsv is damaged (a line is out of range), so the "
                 "game data cannot be rebuilt. Download the kit again.", True)
        have = min(size, max(0, len(img) - src_off))
        if have > 0:
            blob[off:off + have] = img[src_off:src_off + have]
    if hashlib.sha256(bytes(blob)).hexdigest() != want_sha:
        stop("This is a European dump (game code %s), but its contents do not match the "
             "cartridge this build was made from. That usually means the file has been "
             "modified (a patch, a translation or a trimmed dump) or the dump is faulty. "
             "Use an untouched dump of your own retail cartridge, then try again." % code)
    write_bytes(os.path.join(assets, "romdata.bin"), bytes(blob))
    print("    romdata.bin verified (%d bytes, checksum OK)" % blob_total)

    manifest = os.path.join(dest, "romdata.manifest")
    if os.path.isfile(manifest):
        shutil.copyfile(manifest, os.path.join(assets, "romdata.manifest"))
    elif not os.path.isfile(os.path.join(assets, "romdata.manifest")):
        stop("This kit is incomplete: romdata.manifest is missing, so the game would refuse "
             "to start. Download the kit again and unpack the whole zip into one folder "
             "(keep the files together).", True)
    prog(100, "Done")
    print()
    print("Done. The game data is ready in %s" % dest)
    return 0


if __name__ == "__main__":
    sys.exit(main())
