#!/usr/bin/env python3
"""Build a tiny synthetic sound_data.sdat for the host audio test (no ROM needed).

One music sequence NCS_BGM_TEST (loops forever), one bank NCS_BANK_TEST and one
wave archive NCS_WAVE_TEST with three waves:

  wave 0  400 Hz sine at 16000 Hz, 4000 samples, looping (root key 60)
  wave 1  800 Hz sine at 16000 Hz, 0.5 s, one shot      (root key 60)
  wave 2  300 Hz sine at 22050 Hz, 0.5 s, one shot      (root key 60)

Bank programs: 0 = wave 0, 1 = wave 1, 2 = wave 2, 3 = PSG pulse (duty 3).

The sequence plays (60 72 48 on program 0), (60 72 on program 1), (60 72 48 on
program 2), (60 72 on program 3), each note 0.25 s at 120 bpm, then jumps back.

usage: make_test_sdat.py out.sdat
"""
import math
import struct
import sys


def u16(v): return struct.pack("<H", v)
def u32(v): return struct.pack("<I", v)


def swav(samples, rate, loop_start=None):
    words = len(samples) // 2
    ls = (loop_start or 0) // 2
    data = b"".join(struct.pack("<h", s) for s in samples)
    return (bytes([1, 1 if loop_start is not None else 0]) + u16(rate)
            + u16(16756991 // rate) + u16(ls) + u32(words - ls) + data)


def sine(freq, rate, n):
    return [int(12000 * math.sin(2 * math.pi * freq * i / rate)) for i in range(n)]


def build_swar():
    waves = [
        swav(sine(400, 16000, 4000), 16000, 0),
        swav(sine(800, 16000, 8000), 16000),
        swav(sine(300, 22050, 11024), 22050),
    ]
    cnt = len(waves)
    tbl = 0x3c + 4 * cnt
    offs, body = [], b""
    for w in waves:
        offs.append(tbl + len(body))
        body += w
    data = b"DATA" + u32(0) + b"\0" * 32 + u32(cnt) + b"".join(u32(o) for o in offs) + body
    size = 0x10 + len(data)
    data = data[:4] + u32(len(data)) + data[8:]
    return b"SWAR" + u16(0xfeff) + u16(0x100) + u32(size) + u16(16) + u16(1) + data


def inst(typ, swav_i, swar_i, base, a=127, d=127, s=127, r=127, pan=64):
    return bytes([typ]) + b"\0" + b"\0" + u16(swav_i) + u16(swar_i) + bytes([base, a, d, s, r, pan]) + b"\0\0"


def build_sbnk():
    # records: type byte + u16 offset + pad byte in the table; the record itself
    # is swav u16, swar u16, base, a, d, s, r, pan (10 bytes)
    progs = [(1, 0), (1, 1), (1, 2), (2, 3)]   # (type, swav-or-duty)
    cnt = len(progs)
    tbl = 0x3c
    rec0 = tbl + 4 * cnt
    table = b""
    recs = b""
    for i, (t, sw) in enumerate(progs):
        off = rec0 + len(recs)
        table += bytes([t]) + u16(off) + b"\0"
        recs += u16(sw) + u16(0) + bytes([60, 127, 127, 127, 127, 64])
    body = b"DATA" + u32(0) + b"\0" * 32 + u32(cnt) + table + recs
    size = 0x10 + len(body)
    body = body[:4] + u32(len(body)) + body[8:]
    return b"SBNK" + u16(0xfeff) + u16(0x100) + u32(size) + u16(16) + u16(1) + body


def build_sseq():
    q = 24  # 0.25 s at 120 bpm
    s = bytearray()
    s += bytes([0xE1]) + u16(120)
    loop_at = len(s)
    for prog, keys in ((0, (60, 72, 48)), (1, (60, 72)), (2, (60, 72, 48)), (3, (60, 72))):
        s += bytes([0x81, prog])
        for k in keys:
            s += bytes([k, 100, q])
    s += bytes([0x94]) + struct.pack("<I", loop_at)[:3]
    s += bytes([0xff])
    hdr_len = 0x1c
    body = b"DATA" + u32(0) + u32(hdr_len) + bytes(s)
    size = 0x10 + len(body)
    body = body[:4] + u32(len(body)) + body[8:]
    return b"SSEQ" + u16(0xfeff) + u16(0x100) + u32(size) + u16(16) + u16(1) + body


def build_ssar():
    q = 24
    e0 = bytes([0x81, 0]) + bytes([60, 100, q]) + bytes([0xff])
    e1 = bytes([0x81, 1]) + bytes([72, 100, q, 60, 100, q]) + bytes([0xff])
    e2 = bytes([0x81, 3]) + bytes([60, 100, q, 72, 100, q, 64, 100, q]) + bytes([0xff])   # PSG pulse only
    dof = 0x20 + 12 * 3
    ents = u32(0) + u16(0) + bytes([127, 64, 0, 0]) + b"\0\0"
    ents += u32(len(e0)) + u16(0) + bytes([127, 64, 0, 0]) + b"\0\0"
    ents += u32(len(e0) + len(e1)) + u16(0) + bytes([127, 64, 0, 0]) + b"\0\0"
    seqs = e0 + e1 + e2
    body = b"DATA" + u32(0) + u32(dof) + u32(3) + ents + seqs
    size = 0x10 + len(body)
    body = body[:4] + u32(len(body)) + body[8:]
    return b"SSAR" + u16(0xfeff) + u16(0x100) + u32(size) + u16(16) + u16(1) + body


def pad(b, n=4):
    return b + b"\0" * ((-len(b)) % n)


def build():
    files = [build_sseq(), build_sbnk(), build_swar(), build_ssar()]
    names = {"seq": ["NCS_BGM_TEST"], "seqarc": ["NCS_SEQARC_TEST"], "bank": ["NCS_BANK_TEST"], "wavearc": ["NCS_WAVE_TEST"]}

    # SYMB: 8 record lists, each at some offset: count, name offsets; strings after.
    symb_hdr = 8 + 8 * 4
    lists = {}
    strings = b""
    blob = b""
    # layout: header, then the lists, then strings
    kinds = ["seq", "seqarc", "bank", "wavearc", "player", "group", "strmplr", "strm"]
    stride = {"seqarc": 8}
    list_sizes = [4 + stride.get(k, 4) * len(names.get(k, [])) for k in kinds]
    list_off = []
    p = symb_hdr
    for sz in list_sizes:
        list_off.append(p)
        p += sz
    str_off = {}
    for k in kinds:
        for n in names.get(k, []):
            str_off[(k, n)] = p + len(strings)
            strings += n.encode() + b"\0"
    for k, lo in zip(kinds, list_off):
        pass
    symb = b"SYMB" + u32(0) + b"".join(u32(lo) for lo in list_off)
    for k in kinds:
        ns = names.get(k, [])
        symb += u32(len(ns)) + b"".join(u32(str_off[(k, n)]) + (u32(0) if k == "seqarc" else b"") for n in ns)
    symb += strings
    symb = pad(symb)
    symb = symb[:4] + u32(len(symb)) + symb[8:]

    # INFO
    info_hdr = 8 + 8 * 4
    entries = {
        "seq": [u32(0) + u16(0) + bytes([127, 64, 0, 0]) + b"\0\0"],
        "seqarc": [u32(3) + u32(0)],
        "bank": [u32(1) + u16(0) + u16(0xffff) * 3],
        "wavearc": [u32(2) + u32(0)],
    }
    rec_off = []
    p = info_hdr
    sizes = []
    for k in kinds:
        es = entries.get(k, [])
        rec_off.append(p)
        p += 4 + 4 * len(es)
    ent_pos = {}
    body = b""
    for k in kinds:
        for i, e in enumerate(entries.get(k, [])):
            ent_pos[(k, i)] = p + len(body)
            body += e
    info = b"INFO" + u32(0) + b"".join(u32(o) for o in rec_off)
    for k in kinds:
        es = entries.get(k, [])
        info += u32(len(es)) + b"".join(u32(ent_pos[(k, i)]) for i in range(len(es)))
    info += body
    info = pad(info)
    info = info[:4] + u32(len(info)) + info[8:]

    # layout: header 0x40, SYMB, INFO, FAT, FILE
    hdr_size = 0x40
    fat_size = 12 + 16 * len(files)
    off_symb = hdr_size
    off_info = off_symb + len(symb)
    off_fat = off_info + len(info)
    off_file = off_fat + fat_size
    file_data = b""
    fat = b"FAT " + u32(fat_size) + u32(len(files))
    pos = off_file + 16
    for f in files:
        f4 = pad(f)
        fat += u32(pos) + u32(len(f)) + u32(0) + u32(0)
        file_data += f4
        pos += len(f4)
    file_block = b"FILE" + u32(16 + len(file_data)) + u32(len(files)) + u32(0) + file_data
    total = hdr_size + len(symb) + len(info) + len(fat) + len(file_block)
    hdr = (b"SDAT" + u16(0xfeff) + u16(0x100) + u32(total) + u16(hdr_size) + u16(4)
           + u32(off_symb) + u32(len(symb)) + u32(off_info) + u32(len(info))
           + u32(off_fat) + u32(len(fat)) + u32(off_file) + u32(len(file_block)))
    hdr = hdr + b"\0" * (hdr_size - len(hdr))
    return hdr + symb + info + fat + file_block


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "test.sdat"
    open(out, "wb").write(build())
    print("wrote", out)
