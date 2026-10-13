"""A small read-only SDAT reader for the audiohq checks.

It answers one question the game logs cannot: which sounds use which wave? It
walks the sequence archive entries (the sound effects), follows each note
through the bank to the wave it plays, and returns the (wave archive, wave
number) pairs with the sequence entries and keys that play them. The checks use
it to pick a wave that a scripted sound really plays, so "the replacement is
used" can be proved from the game's own voice log.

The format knowledge is the same the game's loader has (hal/sdat/sdat.cpp,
hal/sdat/sseq.cpp); this is a second, independent reading of it.
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "mods_api"))
import sdat_list  # noqa: E402

REC_SEQ, REC_SEQARC, REC_BANK, REC_WAVEARC = 0, 1, 2, 3


def u16(b, o): return struct.unpack_from("<H", b, o)[0]
def u32(b, o): return struct.unpack_from("<I", b, o)[0]


class Sdat:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.buf = f.read()
        b = self.buf
        if b[:4] != b"SDAT":
            raise ValueError("%s is not an SDAT" % path)
        self.blk = {}
        for i in range(u16(b, 0x0e)):
            off = u32(b, 0x10 + i * 8)
            if off and off + 4 <= len(b):
                self.blk[b[off:off + 4]] = off
        self.info = self.blk[b"INFO"]
        self.fat = self.blk[b"FAT "]
        self.members = sdat_list.members(b)

    def file(self, fid):
        off, size = struct.unpack_from("<II", self.buf, self.fat + 0xc + fid * 16)
        return self.buf[off:off + size]

    def info_entry(self, rec, idx):
        lo = u32(self.buf, self.info + 8 + rec * 4)
        if not lo:
            return None
        base = self.info + lo
        if idx >= u32(self.buf, base):
            return None
        v = u32(self.buf, base + 4 + idx * 4)
        return self.info + v if v else None

    def info_count(self, rec):
        lo = u32(self.buf, self.info + 8 + rec * 4)
        return u32(self.buf, self.info + lo) if lo else 0

    def name_of(self, ext, rec):
        for e, suffix, i, name, fid in self.members:
            if e == ext and i == rec:
                return name or "%s_%04d" % (ext, i)
        return None

    def names(self, ext):
        """[(record, name, file id)] for one kind: 'sseq', 'ssar', 'sbnk', 'swar', 'strm'."""
        out = []
        for e, suffix, i, name, fid in self.members:
            if e == ext:
                out.append((i, name or "%s_%04d" % (ext, i), fid))
        return out

    def find(self, ext, name):
        for i, n, fid in self.names(ext):
            if n.lower() == name.lower():
                return i, fid
        return None

    def bgm_names(self):
        """Names of every music sequence (NCS_BGM_*)."""
        return [n for i, n, fid in self.names("sseq") if n.upper().startswith("NCS_BGM_")]

    # ---- banks ---------------------------------------------------------------

    def bank_waves(self, bank_rec):
        """The four wave archive file ids a bank links, or None for an unused slot."""
        e = self.info_entry(REC_BANK, bank_rec)
        if e is None:
            return None, None
        fid = u32(self.buf, e) & 0xffffff
        out = []
        for k in range(4):
            wa = u16(self.buf, e + 4 + k * 2)
            if wa == 0xffff:
                out.append(None)
                continue
            we = self.info_entry(REC_WAVEARC, wa)
            out.append(u32(self.buf, we) & 0xffffff if we else None)
        return fid, out

    def bank_note(self, bank_fid, prog, key):
        """(swar slot, wave number, type) the bank plays for this program and key,
        or None. Walks single, drum set and key split records."""
        f = self.file(bank_fid)
        n = u32(f, 0x38)
        if prog < 0 or prog >= n:
            return None
        t = f[0x3c + prog * 4]
        off = u16(f, 0x3d + prog * 4)
        if not t or not off:
            return None
        if 1 <= t <= 5:
            return (u16(f, off + 2), u16(f, off), t)
        if t == 0x10:
            lo, hi = f[off], f[off + 1]
            if key < lo or key > hi:
                return None
            reg = off + 2 + (key - lo) * 12
            return (u16(f, reg + 4), u16(f, reg + 2), f[reg])
        if t == 0x11:
            region = -1
            for i in range(8):
                if f[off + i] == 0:
                    break
                if key <= f[off + i]:
                    region = i
                    break
            if region < 0:
                return None
            reg = off + 8 + region * 12
            return (u16(f, reg + 4), u16(f, reg + 2), f[reg])
        return None

    # ---- sequence archives -----------------------------------------------------

    def seqarc_entries(self, arc_rec):
        """[(entry index, offset into the file of its bytecode, bank record)]."""
        e = self.info_entry(REC_SEQARC, arc_rec)
        if e is None:
            return []
        fid = u32(self.buf, e) & 0xffffff
        f = self.file(fid)
        dof, cnt = u32(f, 0x18), u32(f, 0x1c)
        out = []
        for i in range(cnt):
            so, bank = u32(f, 0x20 + i * 12), u16(f, 0x24 + i * 12)
            if so == 0xffffffff:
                continue
            out.append((i, dof + so, bank))
        return out

    def seqarc_file(self, arc_rec):
        e = self.info_entry(REC_SEQARC, arc_rec)
        return self.file(u32(self.buf, e) & 0xffffff) if e is not None else b""


_ARGS = {0xc0: 1, 0xc1: 1, 0xc2: 1, 0xc3: 1, 0xc4: 1, 0xc5: 1, 0xc6: 1, 0xc7: 1, 0xc8: 1, 0xc9: 1,
         0xca: 1, 0xcb: 1, 0xcc: 1, 0xcd: 1, 0xce: 1, 0xcf: 1, 0xd0: 1, 0xd1: 1, 0xd2: 1, 0xd3: 1,
         0xd4: 1, 0xd5: 1, 0xd6: 1, 0xe0: 2, 0xe1: 2, 0xe3: 2, 0xfc: 0, 0xfd: 0, 0xfe: 2, 0xff: 0,
         0x95: 3, 0x94: 3, 0x93: 4}


def _varlen(f, p):
    v = 0
    for _ in range(4):
        b = f[p]
        p += 1
        v = (v << 7) | (b & 0x7f)
        if not b & 0x80:
            break
    return v, p


def notes_of(f, start):
    """Linear scan of one sequence entry (a file image f, bytecode at `start`)
    for (program, key) pairs of its notes, following no jumps but opening the
    tracks a 0x93 names. Returns a set of (program, transposed key). The scan is
    a reading aid, not a player: random / variable operands are skipped over."""
    todo = [start]
    done_starts = set()
    out = set()
    while todo and len(done_starts) < 16:
        pc = todo.pop()
        if pc in done_starts:
            continue
        done_starts.add(pc)
        prog, transpose = 0, 0
        prefix_rand = prefix_var = False
        for _ in range(4096):
            if pc >= len(f):
                break
            op = f[pc]
            pc += 1
            if op in (0xa0, 0xa1, 0xa2):
                prefix_rand = prefix_rand or op == 0xa0
                prefix_var = prefix_var or op == 0xa1
                continue
            if op < 0x80:
                pc += 1                                   # velocity
                if prefix_rand:
                    pc += 4
                elif prefix_var:
                    pc += 1
                else:
                    _, pc = _varlen(f, pc)
                k = max(0, min(127, op + transpose))
                out.add((prog, k))
            elif op == 0x80:
                if prefix_rand:
                    pc += 4
                elif prefix_var:
                    pc += 1
                else:
                    _, pc = _varlen(f, pc)
            elif op == 0x81:
                if prefix_rand:
                    pc += 4
                elif prefix_var:
                    pc += 1
                else:
                    prog, pc = _varlen(f, pc)
            elif op == 0xc3:
                v = f[pc]
                pc += 1
                transpose = v - 256 if v > 127 else v
            elif op == 0x93:
                tgt = f[pc + 1] | (f[pc + 2] << 8) | (f[pc + 3] << 16)
                pc += 4
                todo.append(NOTES_DATA_BASE[0] + tgt)
            elif op in (0xff, 0xfd):
                break
            elif 0xb0 <= op <= 0xbd:
                pc += 3
            elif op in _ARGS:
                pc += _ARGS[op]
            else:
                break
            prefix_rand = prefix_var = False
    return out


NOTES_DATA_BASE = [0]


def wave_users(sd, arc_rec=2):
    """{(swar file id, wave number): set((entry, key))} for one sequence archive
    (default: record 2, the system and menu sounds the game can play on demand)."""
    f = sd.seqarc_file(arc_rec)
    if not f:
        return {}
    NOTES_DATA_BASE[0] = u32(f, 0x18)
    users = {}
    for entry, off, bank_rec in sd.seqarc_entries(arc_rec):
        bank_fid, links = sd.bank_waves(bank_rec)
        if bank_fid is None:
            continue
        try:
            notes = notes_of(f, off)
        except IndexError:
            continue
        for prog, key in notes:
            r = sd.bank_note(bank_fid, prog, key)
            if not r:
                continue
            slot, wav, typ = r
            if typ in (2, 3) or slot > 3 or links[slot] is None:
                continue                                  # PSG notes have no wave
            users.setdefault((links[slot], wav), set()).add((entry, key))
    return users


def pick_wave(sd, arc_rec=2, want_keys=2):
    """The (swar file id, wave number) played by the most distinct keys, with
    its sequence entries; the entries are what a scripted sound probe can play."""
    users = wave_users(sd, arc_rec)
    best = None
    for (swar, wav), uses in users.items():
        keys = {k for e, k in uses}
        score = (len(keys) >= want_keys, len(keys), len(uses))
        if best is None or score > best[0]:
            best = (score, swar, wav, sorted(uses))
    return best


def swar_wave_info(sd, swar_fid, index):
    """(format, loops, rate, samples) of one wave of a wave archive, or None."""
    f = sd.file(swar_fid)
    n = u32(f, 0x38)
    if index >= n:
        return None
    wo = u32(f, 0x3c + index * 4)
    if not wo:
        return None
    fmt, loop = f[wo], f[wo + 1]
    rate = u16(f, wo + 2)
    loop_words, tail = u16(f, wo + 6), u32(f, wo + 8)
    nbytes = (loop_words + tail) * 4
    samples = nbytes if fmt == 0 else nbytes // 2 if fmt == 1 else max(0, (nbytes - 4) * 2)
    return fmt, bool(loop), rate, samples


def swar_name(sd, fid):
    for i, n, f in sd.names("swar"):
        if f == fid:
            return n
    return None


def psg_entries(sd, arc_rec=2):
    """Entries of a sequence archive whose notes are all PSG (pulse or noise):
    sounds with no wave in them, the way to hear the mixer's stepped voices."""
    f = sd.seqarc_file(arc_rec)
    if not f:
        return []
    NOTES_DATA_BASE[0] = u32(f, 0x18)
    out = []
    for entry, off, bank_rec in sd.seqarc_entries(arc_rec):
        bank_fid, links = sd.bank_waves(bank_rec)
        if bank_fid is None:
            continue
        try:
            notes = notes_of(f, off)
        except IndexError:
            continue
        types = [sd.bank_note(bank_fid, p, k) for p, k in notes]
        if types and all(t and t[2] in (2, 3) for t in types):
            out.append(entry)
    return out
