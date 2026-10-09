#!/usr/bin/env python3
"""List the members of a Nintendo SDAT archive by the names a mod's sound/ folder uses.

Usage: python sdat_list.py path/to/sound_data.sdat

Each output line is: <file name to use in sound/>  <type>  <record>  <file id>  <size in bytes>
The name is the archive's own symbol (SYMB block) plus the type's extension, or
<type>_<NNNN>.<ext> when the archive has no name for it. <record> is the NNNN of that
numbered form, which also works for named members (sseq_0003.sseq).
"""
import struct, sys

TYPES = [(0, "sseq", ".sseq"), (1, "ssar", ".ssar"), (2, "sbnk", ".sbnk"),
         (3, "swar", ".swar"), (7, "strm", ".strm")]
# INFO record kinds: 0 SEQ, 1 SEQARC, 2 BANK, 3 WAVEARC, 4 PLAYER, 5 GROUP, 6 STRMPLAYER, 7 STRM


def members(buf):
    """Returns a list of (kind, ext, record index, name or None, fat id)."""
    nblk = struct.unpack_from("<H", buf, 0x0e)[0]
    blk = {}
    for i in range(nblk):
        off = struct.unpack_from("<I", buf, 0x10 + i * 8)[0]
        if off and off + 4 <= len(buf):
            blk[buf[off:off + 4]] = off
    info, symb = blk.get(b"INFO"), blk.get(b"SYMB")
    out = []
    for kind, ext, suffix in TYPES:
        lst = struct.unpack_from("<I", buf, info + 8 + kind * 4)[0]
        if not lst:
            continue
        lst += info
        n = struct.unpack_from("<I", buf, lst)[0]
        names = {}
        if symb:
            sl = struct.unpack_from("<I", buf, symb + 8 + kind * 4)[0]
            if sl:
                sl += symb
                sn = struct.unpack_from("<I", buf, sl)[0]
                # SEQARC (kind 1) name entries are 8 bytes (name offset, sub-table offset);
                # SEQ, BANK, WAVEARC and STRM entries are 4 bytes (name offset)
                stride = 8 if kind == 1 else 4
                for i in range(min(n, sn)):
                    so = struct.unpack_from("<I", buf, sl + 4 + i * stride)[0]
                    if so:
                        e = buf.index(b"\0", symb + so)
                        nm = buf[symb + so:e].decode("ascii", "replace")
                        if nm:
                            names[i] = nm
        for i in range(n):
            ro = struct.unpack_from("<I", buf, lst + 4 + i * 4)[0]
            if not ro:
                continue  # unused slot
            fid = struct.unpack_from("<I", buf, info + ro)[0] & 0xffffff
            out.append((ext, suffix, i, names.get(i), fid))
    return out


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    buf = open(sys.argv[1], "rb").read()
    if buf[:4] != b"SDAT":
        sys.exit("not an SDAT")
    nblk = struct.unpack_from("<H", buf, 0x0e)[0]
    fo = [struct.unpack_from("<I", buf, 0x10 + i * 8)[0] for i in range(nblk)]
    fat = next(o for o in fo if buf[o:o + 4] == b"FAT ")
    for ext, suffix, i, name, fid in members(buf):
        size = struct.unpack_from("<I", buf, fat + 0xc + fid * 16 + 4)[0]
        fn = (name or "%s_%04d" % (ext, i)) + suffix
        print("%-48s %-5s %6d %6d %9d" % (fn, ext, i, fid, size))


if __name__ == "__main__":
    main()
