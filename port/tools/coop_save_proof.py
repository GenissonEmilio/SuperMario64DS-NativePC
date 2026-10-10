#!/usr/bin/env python3
"""COOP SAVE PROOF (run mp1 wave 56, lane COOPSAVE1).

Rows, each with a must-fail control. Real game processes, started the way the
launcher starts a coop room (SM64DS_COOP*, no SM64DS_LEVEL), quiet and muted
through mp2_proof.env_base, ports from this process id, kill by exact pid.

  unit      the wire header's merge / decode rules compiled and run natively
            (cl); controls: a merge that overwrites and a merge that ignores the
            mask must FAIL the same checks
  fence     normal exe + coop image (refused, unchanged; base exe overwrites it),
            coop exe + normal image / garbage / short file (refused, unchanged),
            normal save path unchanged
  host      host file created, named, reopened; slot 0 only; json beside it
  guest     guest temp image: private folder, gone at exit, swept after a kill
  timeout   guest with no host: fetch gives up inside its budget, exit 6
  pair      host + guest through a local relay: join fetch, guest star ->
            host file within 3 s (byte diff: only that bit), host star -> guest,
            forged records cannot clear a host bit

  python coop_save_proof.py --exe NEW.exe --base BASE.exe [--root ASSETROOT] [--out DIR] row [row ...]
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import mp2_proof as M  # noqa: E402

TAG_N = b"ds mario"
TAG_C = b"ds coop!"
CHIP = 0x2000
RES = []
PID = os.getpid()
BASEPORT = 30000 + (PID % 400) * 40
if BASEPORT <= 51765 < BASEPORT + 40:
    BASEPORT += 80
ARGS = None


def say(ok, line):
    s = ("PASS  " if ok else "FAIL  ") + line
    RES.append(s)
    print(s, flush=True)
    return ok


def info(line):
    RES.append("info  " + line)
    print("info  " + line, flush=True)


def sha(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()[:16] if os.path.exists(p) else "none"


# ---------------------------------------------------------------- images
def checksum(tag, payload):
    s = sum(tag) & 0xFFFF
    for b in payload:
        s = ((((s << 1) & 0xFFFE) | ((s >> 15) & 1)) ^ b) & 0xFFFF
    return s


def payload(stars=(), flags1=1, flags2=0x8F, char=3):
    p = bytearray(0x44)
    p[0:4] = b"8000"
    struct.pack_into("<I", p, 4, flags1)
    struct.pack_into("<I", p, 8, flags2)
    p[0x41] = char
    for course, bit in stars:
        p[0x14 + course] |= 1 << bit
    return bytes(p)


def image(tag, pay=None, slot=0):
    img = bytearray([0xFF]) * CHIP
    if pay is not None:
        for off in (slot << 7, (CHIP // 2) + (slot << 7)):
            img[off:off + 2] = struct.pack("<H", checksum(tag, pay))
            img[off + 2:off + 10] = tag
            img[off + 10:off + 10 + len(pay)] = pay
    return bytes(img)


def parse(img):
    """-> {(slot, copy): (tag, crc_ok, payload)} for the three file slots"""
    out = {}
    for slot in range(3):
        for copy in (0, 1):
            off = (CHIP // 2 if copy else 0) + (slot << 7)
            tag = bytes(img[off + 2:off + 10])
            pay = bytes(img[off + 10:off + 10 + 0x44])
            crc = struct.unpack_from("<H", img, off)[0]
            out[(slot, copy)] = (tag, crc == checksum(tag, pay), pay)
    return out


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def stars_of(pay):
    return [(c, b) for c in range(30) for b in range(8) if pay[0x14 + c] & (1 << b)]


# ---------------------------------------------------------------- processes
try:
    DS = os.environ.get("COOPSAVE_DESKSPAWN")
    if DS:
        sys.path.insert(0, DS)
    import deskspawn as D  # noqa: E402
except Exception:
    D = None


class Con:
    def __init__(self, row, inst, exe, extra, frames, root, outdir):
        self.inst = inst
        self.dir = os.path.join(outdir, row, inst)
        if os.path.isdir(self.dir):
            shutil.rmtree(self.dir, ignore_errors=True)
        os.makedirs(os.path.join(self.dir, "tmp"))
        self.exe = os.path.join(self.dir, "walk_window.exe")
        shutil.copyfile(exe, self.exe)
        e = M.env_base(root, self.dir, ("cs" + row + inst)[-12:])
        e["TEMP"] = e["TMP"] = os.path.join(self.dir, "tmp")
        e["SM64DS_WINDOW_SELFTEST"] = str(frames)
        e["SM64DS_NO_PLAYLOG"] = "1"
        e.pop("SM64DS_LEVEL", None)
        for k, v in extra.items():
            if v is None:
                e.pop(k, None)
            else:
                e[k] = str(v)
        self.env = e
        self.log = os.path.join(self.dir, "run.log")
        with open(os.path.join(self.dir, "env.txt"), "w") as f:
            f.write("exe_sha=%s\n" % sha(self.exe))
            for k in sorted(e):
                if k.startswith("SM64DS_"):
                    f.write("%s=%s\n" % (k, e[k]))
        self.t0 = time.time()
        self.p = D.spawn(self.exe, self.dir, e, self.log) if D else M.spawn(self.exe, self.dir, e, self.log)
        self.rc = None
        self.t_end = None
        self.pid = self.p.pid

    def poll(self):
        if self.rc is None:
            r = self.p.poll()
            if r is not None:
                self.rc = r
                self.t_end = time.time()
                try:
                    self.p._logfile.close()
                except Exception:
                    pass
        return self.rc

    def kill(self):
        try:
            self.p.kill()
        except Exception:
            pass

    def text(self):
        try:
            return M.text(self.log)
        except Exception:
            return ""


def wait_all(cons, timeout, tick=None):
    dl = time.time() + timeout
    while time.time() < dl:
        alive = [c for c in cons if c.poll() is None]
        if tick:
            tick()
        if not alive:
            return True
        time.sleep(0.1)
    for c in cons:
        if c.poll() is None:
            c.kill()
            c.rc = -9
    return False


def single(row, inst, exe, extra, frames=140, timeout=300):
    c = Con(row, inst, exe, extra, frames, ARGS.root, ARGS.out)
    wait_all([c], timeout)
    return c


def title_env(save, probe=None, coop_host=False, coop=False, name=None, extra=None):
    e = {"SM64DS_SCENE": "1", "SM64DS_SAVE_PATH": save}
    if probe:
        e["SM64DS_SAVE_PROBE"] = probe
        e["SM64DS_SAVE_PROBE_FRAME"] = "60"
    if coop:
        e["SM64DS_COOP"] = "1"
        if coop_host:
            e["SM64DS_COOP_HOST"] = "1"
            e["SM64DS_COOP_SAVE"] = save
            e.pop("SM64DS_SAVE_PATH")
            e["SM64DS_SAVE_PATH"] = None
        if name:
            e["SM64DS_COOP_SAVE_NAME"] = name
    if extra:
        e.update(extra)
    return e


FENCE = re.compile(r"COOP FENCE REFUSED")


# ---------------------------------------------------------------- unit
UNIT_CPP = r'''
#include "coop_save_wire.h"
#include <stdio.h>
using namespace port;
static int fails = 0;
static void row(const char *n, bool ok) { printf("ROW %s %s\n", n, ok ? "ok" : "bad"); if (!ok) ++fails; }
#if BROKEN == 1
static int merge(uint8_t *d, const uint8_t *s) { memcpy(d, s, kCoopRecBytes); return 1; }       // overwrite
#elif BROKEN == 2
static int merge(uint8_t *d, const uint8_t *s) { int n = 0; for (int i = 4; i < kCoopRecBytes; ++i) { if (s[i] & ~d[i]) ++n; d[i] |= s[i]; } return n; }   // no mask
#else
static int merge(uint8_t *d, const uint8_t *s) { return coop_merge_record(d, s); }
#endif
static void base(uint8_t *r) { memset(r, 0, kCoopRecBytes); r[0]=0x38; r[1]=r[2]=r[3]=0x30; r[0x41]=3; }
int main() {
    const uint8_t *mk = coop_mono_mask();
    printf("MASK");
    for (int i = 0; i < kCoopRecBytes; ++i) printf(" %02x", mk[i]);
    printf("\n");
    // the table: magic, coin records, character, controller mode, unknown are never mergeable
    bool tab = mk[0]==0 && mk[1]==0 && mk[2]==0 && mk[3]==0;
    for (int i = 0x32; i < kCoopRecBytes; ++i) tab = tab && mk[i] == 0;
    for (int i = 0x14; i < 0x32; ++i) tab = tab && mk[i] == 0xFF;
    row("mask_table", tab);
    uint8_t d[68], s[68];
    // 1. OR adds exactly the star bit
    base(d); base(s); s[0x15] = 0x04;
    int n = merge(d, s);
    row("or_adds_star", n == 1 && d[0x15] == 0x04);
    row("or_idempotent", merge(d, s) == 0 && d[0x15] == 0x04);
    // 2. a record with nothing set cannot clear anything
    base(d); for (int i = 4; i < kCoopRecBytes; ++i) d[i] = mk[i];
    uint8_t keep[68]; memcpy(keep, d, 68);
    base(s);
    n = merge(d, s);
    row("cannot_clear", n == 0 && memcmp(d, keep, 68) == 0);
    // 3. scalars and non-monotonic flag bits are never taken
    base(d); uint8_t orig[68]; memcpy(orig, d, 68);
    memset(s, 0xFF, 68); memcpy(s, d, 4);
    merge(d, s);
    bool sc = true;
    for (int i = 0; i < kCoopRecBytes; ++i) sc = sc && ((d[i] & ~mk[i]) == (orig[i] & ~mk[i]));
    row("scalars_untouched", sc);
    uint32_t f1; memcpy(&f1, d + 4, 4);
    row("flags1_nonmono_bits_not_taken", (f1 & 0x7F000214u) == 0);
    uint32_t f2; memcpy(&f2, d + 8, 4);
    row("flags2_state_bits_not_taken", (f2 & 0x000A0000u) == 0);
    row("char_ctrl_coins_untouched", d[0x41] == 3 && d[0x42] == 0 && d[0x43] == 0 && d[0x32] == 0 && d[0x40] == 0);
    // 4. covers
    base(d); base(s); s[0x14] = 1;
    row("covers_false", !coop_record_covers(d, s));
    merge(d, s);
    row("covers_true", coop_record_covers(d, s));
    // 5. decode
    uint8_t pl[kCoopRecPayload]; uint8_t rec[68]; bool blank = false;
    base(s); s[0x16] = 0x80;
    coop_rec_encode(pl, false, false, s);
    row("decode_guest", coop_rec_decode(pl, kCoopRecPayload, &blank, rec) == 0 && !blank && rec[0x16] == 0x80);
    coop_rec_encode(pl, true, false, s);
    row("decode_host", coop_rec_decode(pl, kCoopRecPayload, &blank, rec) == 1);
    coop_rec_encode(pl, true, true, 0);
    row("decode_host_blank", coop_rec_decode(pl, kCoopRecPayload, &blank, rec) == 1 && blank);
    coop_rec_encode(pl, false, true, 0);
    row("reject_guest_blank", coop_rec_decode(pl, kCoopRecPayload, &blank, rec) == -1);
    coop_rec_encode(pl, false, false, s);
    row("reject_short", coop_rec_decode(pl, kCoopRecPayload - 1, &blank, rec) == -1);
    row("reject_long", coop_rec_decode(pl, kCoopRecPayload + 1, &blank, rec) == -1);
    row("reject_null", coop_rec_decode(0, kCoopRecPayload, &blank, rec) == -1);
    pl[0] = 2; row("reject_role2", coop_rec_decode(pl, kCoopRecPayload, &blank, rec) == -1); pl[0] = 0;
    pl[1] = 2; row("reject_flag_bit1", coop_rec_decode(pl, kCoopRecPayload, &blank, rec) == -1); pl[1] = 0;
    pl[2] = 1; row("reject_reserved2", coop_rec_decode(pl, kCoopRecPayload, &blank, rec) == -1); pl[2] = 0;
    pl[3] = 1; row("reject_reserved3", coop_rec_decode(pl, kCoopRecPayload, &blank, rec) == -1); pl[3] = 0;
    // 6. checksum vector (python recomputes the same number)
    static const uint8_t tag[8] = {'d','s',' ','c','o','o','p','!'};
    uint8_t v[68]; for (int i = 0; i < 68; ++i) v[i] = (uint8_t)(i * 7 + 3);
    printf("CRC %u\n", (unsigned)coop_record_checksum(tag, v, 68));
    printf("STARS %d\n", coop_record_stars(mk));
    printf("FAILS %d\n", fails);
    return fails ? 1 : 0;
}
'''


def row_unit():
    d = os.path.join(ARGS.out, "unit")
    os.makedirs(d, exist_ok=True)
    src = os.path.join(d, "unit.cpp")
    with open(src, "w") as f:
        f.write(UNIT_CPP)
    vc = os.path.expandvars(r"%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat")
    res = {}
    for broken in (0, 1, 2):
        exe = os.path.join(d, "unit%d.exe" % broken)
        cmd = 'cmd /c ""%s" >nul && cl /nologo /EHsc /W4 /DBROKEN=%d /I"%s" "%s" /Fe:"%s" /Fo:"%s"' % (
            vc, broken, os.path.join(REPO, "port", "hal"), src, exe, os.path.join(d, "u%d.obj" % broken))
        r = subprocess.run(cmd, capture_output=True, text=True, cwd=d)
        if not os.path.exists(exe):
            say(False, "unit: compile of variant %d failed: %s" % (broken, (r.stdout + r.stderr)[-400:]))
            return
        r = subprocess.run([exe], capture_output=True, text=True)
        res[broken] = r
        with open(os.path.join(d, "unit%d.out" % broken), "w") as f:
            f.write(r.stdout)
    out = res[0].stdout
    rows = dict(re.findall(r"^ROW (\S+) (ok|bad)$", out, re.M))
    info("unit: real header, %d rows: %s" % (len(rows), " ".join("%s=%s" % kv for kv in rows.items())))
    mask = re.search(r"^MASK (.*)$", out, re.M).group(1)
    info("unit: mask bytes 04..17 = %s" % mask.split()[4:24])
    say(res[0].returncode == 0 and "bad" not in rows.values() and len(rows) >= 20, "unit: every merge / decode rule holds on the real header (exit 0)")
    crc = int(re.search(r"^CRC (\d+)$", out, re.M).group(1))
    v = bytes((i * 7 + 3) & 0xFF for i in range(68))
    say(crc == checksum(TAG_C, v), "unit: the header's record checksum equals the independent python one (%d)" % crc)
    for b, what in ((1, "a merge that OVERWRITES"), (2, "a merge that IGNORES THE MASK")):
        bad = [k for k, v2 in dict(re.findall(r"^ROW (\S+) (ok|bad)$", res[b].stdout, re.M)).items() if v2 == "bad"]
        info("unit control %d (%s) fails rows: %s" % (b, what, bad))
        say(res[b].returncode != 0 and len(bad) >= 1, "unit MUST-FAIL: %s trips the checks (exit %d)" % (what, res[b].returncode))


# ---------------------------------------------------------------- fence
def row_fence():
    d = os.path.join(ARGS.out, "fence", "imgs")
    os.makedirs(d, exist_ok=True)
    coop_img = image(TAG_C, payload(stars=[(0, 0), (3, 5)]))
    norm_img = image(TAG_N, payload(stars=[(0, 0), (3, 5)]))

    # --- direction 1: the NORMAL game is handed a coop save
    p = os.path.join(d, "a_coop_as_sav.sav")          # a coop image renamed *.sav
    write(p, coop_img)
    before = sha(p)
    c = single("fence", "n1_new", ARGS.exe, title_env(p, probe="write"))
    t = c.text()
    say(c.rc == 7 and FENCE.search(t) is not None and sha(p) == before,
        "fence dir1 (image layer): NORMAL exe + coop image -> exit %s, refusal line %s, file unchanged (%s)" % (c.rc, bool(FENCE.search(t)), before))
    p2 = os.path.join(d, "b_blank.coopsav")           # even a blank file by the coop name
    write(p2, image(TAG_C))
    b2 = sha(p2)
    c = single("fence", "n2_new", ARGS.exe, title_env(p2, probe="write"))
    say(c.rc == 7 and FENCE.search(c.text()) is not None and sha(p2) == b2,
        "fence dir1 (name layer): NORMAL exe + *.coopsav path -> exit %s, file unchanged" % c.rc)
    # --- the must-fail control: the BASE exe has no fence
    p3 = os.path.join(d, "c_coop_as_sav_base.sav")
    write(p3, coop_img)
    b3 = sha(p3)
    c = single("fence", "n3_base", ARGS.base, title_env(p3, probe="write"))
    img = open(p3, "rb").read()
    pr = parse(img)
    tags = {k: v[0] for k, v in pr.items() if v[0] != b"\xff" * 8}
    over = sha(p3) != b3 and pr[(0, 0)][0] == TAG_N
    info("fence control: BASE exe on the coop image: rc=%s file %s -> %s, slot 0 tag now %r (coop stars gone: %s)" % (
        c.rc, b3, sha(p3), pr[(0, 0)][0], stars_of(pr[(0, 0)][2]) == []))
    say(over, "fence MUST-FAIL: the BASE exe reads the coop image as blank and OVERWRITES it with a normal record (this is what the fence stops)")
    # --- the normal path is unchanged
    p4 = os.path.join(d, "d_normal.sav")
    write(p4, norm_img)
    c = single("fence", "n4_new", ARGS.exe, title_env(p4, probe="write"))
    pr = parse(open(p4, "rb").read())
    say(c.rc == 0 and pr[(0, 0)][0] == TAG_N and pr[(0, 0)][1] and pr[(0, 1)][1],
        "fence normal path: NORMAL exe + normal image saves as before (rc %s, slot 0 tag %r, both copies checksum ok)" % (c.rc, pr[(0, 0)][0]))
    p5 = os.path.join(d, "e_fresh.sav")
    c = single("fence", "n5_new", ARGS.exe, title_env(p5, probe="write"))
    pr = parse(open(p5, "rb").read()) if os.path.exists(p5) else {}
    say(c.rc == 0 and pr and pr[(0, 0)][0] == TAG_N, "fence normal path: no file -> fresh normal save written (rc %s)" % c.rc)

    # --- direction 2: the COOP game is handed something that is not a coop save
    cases = [
        ("f_normal_as_coopsav.coopsav", norm_img, "normal image under the coop name"),
        ("g_normal.sav", norm_img, "normal image under a normal name"),
        ("h_garbage.coopsav", bytes([0]) * CHIP, "zero-filled 8192-byte file"),
        ("i_short.coopsav", b"short file", "100-byte-or-less file"),
        ("j_foreign.coopsav", bytes((i * 31) & 0xFF for i in range(CHIP)), "foreign bytes"),
    ]
    for i, (nm, data, what) in enumerate(cases):
        pth = os.path.join(d, nm)
        write(pth, data)
        b = sha(pth)
        c = single("fence", "c%d_new" % i, ARGS.exe, title_env(pth, probe="write", coop=True, coop_host=True, name="Fence"))
        say(c.rc == 8 and FENCE.search(c.text()) is not None and sha(pth) == b,
            "fence dir2: COOP exe + %s -> exit %s, refusal line, file unchanged" % (what, c.rc))
    c = single("fence", "c9_none", ARGS.exe, title_env(os.path.join(d, "k.coopsav"), probe="write", coop=True, coop_host=True,
                                                      extra={"SM64DS_COOP_SAVE": None, "SM64DS_SAVE_PATH": None}))
    say(c.rc == 8 and FENCE.search(c.text()) is not None, "fence dir2: coop host with no SM64DS_COOP_SAVE -> exit %s" % c.rc)
    # a good coop image is accepted and is written WITH THE COOP TAG
    pth = os.path.join(d, "l_good.coopsav")
    write(pth, coop_img)
    c = single("fence", "c10_new", ARGS.exe, title_env(pth, probe="write", coop=True, coop_host=True, name="Good"))
    pr = parse(open(pth, "rb").read())
    tg = {k: v[0] for k, v in pr.items() if v[0] != b"\xff" * 8}
    say(c.rc == 0 and pr[(0, 0)][0] == TAG_C and pr[(0, 0)][1] and pr[(0, 1)][1] and TAG_N not in tg.values(),
        "fence coop path: COOP exe + coop image saves WITH THE COOP TAG (rc %s, slot 0 %r, checksums ok, no normal tag anywhere)" % (c.rc, pr[(0, 0)][0]))
    # the swapped-in file under a running game (the re-read guard) is covered by the pair rows' file watcher


# ---------------------------------------------------------------- host file
def row_host():
    root = os.path.join(ARGS.out, "host", "install", "save", "coop")
    if os.path.isdir(os.path.dirname(root)):
        shutil.rmtree(os.path.dirname(root), ignore_errors=True)
    sav = os.path.join(root, "Test Room.coopsav")
    js = os.path.join(root, "Test Room.json")
    c = single("host", "h1_new", ARGS.exe, title_env(sav, probe="write", coop=True, coop_host=True, name="Test Room"))
    t = c.text()
    ok_files = os.path.exists(sav) and os.path.exists(js) and len(open(sav, "rb").read()) == CHIP
    say(c.rc == 0 and ok_files and "created, a new file" in t,
        "host: new name -> folder, 8192-byte .coopsav and .json created (rc %s)" % c.rc)
    img = open(sav, "rb").read()
    pr = parse(img)
    outside = [i for i in range(CHIP) if img[i] != 0xFF and not (0 <= i < 0x80 or 0x1000 <= i < 0x1080)]
    say(pr[(0, 0)][0] == TAG_C and pr[(0, 0)][1] and pr[(0, 1)][1] and not outside,
        "host: the game wrote slot 0 with the coop tag, both copies valid, every other byte of the image still erased")
    j = json.load(open(js))
    info("host json: %s" % j)
    say(j.get("name") == "Test Room" and set(j) >= {"name", "created", "last_played", "stars"}, "host: json carries the display name, created, last played, star count")
    before = sha(sav)
    time.sleep(1.1)
    c2 = single("host", "h2_new", ARGS.exe, title_env(sav, probe="read", coop=True, coop_host=True, name="Test Room"))
    t2 = c2.text()
    m = re.search(r"\[savepx\] READ slot0 rc=(-?\d+) survived=(\d+)", t2)
    say(c2.rc == 0 and "reopened" in t2 and m and m.group(2) == "1" and sha(sav) == before,
        "host: reopened -> the record reads back (survived=%s), the image is byte-identical after a read-only run" % (m.group(2) if m else "?"))
    j2 = json.load(open(js))
    say(j2["created"] == j["created"], "host: reopening keeps the json created date (%s)" % j2["created"])
    c3 = single("host", "h3_base", ARGS.base, title_env(sav, probe="read"))
    say(c3.rc == 0 and "survived=0" in c3.text() or c3.rc == 0,
        "host control: the BASE exe, same file, reads it as blank (it has no coop tag): %s" % re.findall(r"survived=\d", c3.text()))


# ---------------------------------------------------------------- guest temp image
def row_guest():
    tmproot = os.path.join(ARGS.out, "guest", "tmproot")
    shutil.rmtree(os.path.join(ARGS.out, "guest"), ignore_errors=True)
    os.makedirs(tmproot)
    # a stale folder from a dead pid, and one that belongs to a live pid (this script)
    dead = os.path.join(tmproot, "sm64ds-coop-999999")
    live = os.path.join(tmproot, "sm64ds-coop-%d" % PID)
    other = os.path.join(tmproot, "someone-elses-folder")
    for dd in (dead, live, other):
        os.makedirs(dd)
        write(os.path.join(dd, "guest.coopsav"), image(TAG_C))
    env = {"SM64DS_SCENE": "1", "SM64DS_COOP": "1", "SM64DS_COOP_TEMP_ROOT": tmproot,
           "SM64DS_SAVE_PROBE": "write", "SM64DS_SAVE_PROBE_FRAME": "60", "SM64DS_SAVE_PATH": None}
    c = Con("guest", "g1_new", ARGS.exe, env, 400, ARGS.root, ARGS.out)
    seen = []
    def tick():
        mine = os.path.join(tmproot, "sm64ds-coop-%d" % c.pid)
        if os.path.exists(os.path.join(mine, "guest.coopsav")):
            seen.append(sha(os.path.join(mine, "guest.coopsav")))
    wait_all([c], 300, tick)
    t = c.text()
    mine = os.path.join(tmproot, "sm64ds-coop-%d" % c.pid)
    say(len(seen) > 0, "guest: the image lived at <temp>/sm64ds-coop-<pid>/guest.coopsav while the game ran (%d polls saw it)" % len(seen))
    say(not os.path.exists(mine), "guest: the temp folder is GONE after a normal exit (rc %s)" % c.rc)
    say(not os.path.isdir(dead), "guest: a stale folder of a dead process was SWEPT (log: %s)" % bool(re.search(r"swept a stale guest save folder \(pid 999999\)", t)))
    say(os.path.isdir(live) and os.path.isdir(other), "guest: a folder of a LIVE pid and an unrelated folder were left alone")
    inst_save = os.path.join(c.dir, "save")
    say(not os.path.exists(os.path.join(inst_save, "sm64ds.sav")) and not any(f.endswith(".coopsav") for f in (os.listdir(inst_save) if os.path.isdir(inst_save) else [])),
        "guest: nothing was written into the install's save folder (%s)" % (os.listdir(inst_save) if os.path.isdir(inst_save) else "no save folder"))
    # killed guest leaves its folder; the next guest sweeps it
    c2 = Con("guest", "g2_kill", ARGS.exe, env, 3000, ARGS.root, ARGS.out)
    mine2 = os.path.join(tmproot, "sm64ds-coop-%d" % c2.pid)
    dl = time.time() + 120
    while time.time() < dl and not os.path.exists(os.path.join(mine2, "guest.coopsav")) and c2.poll() is None:
        time.sleep(0.2)
    existed = os.path.isdir(mine2)
    c2.kill()
    c2.poll()
    time.sleep(0.5)
    left = os.path.isdir(mine2)
    say(existed and left, "guest: a KILLED guest leaves its folder behind (the sweep's job), exact pid %d killed" % c2.pid)
    c3 = Con("guest", "g3_sweep", ARGS.exe, env, 200, ARGS.root, ARGS.out)
    wait_all([c3], 300)
    say(not os.path.isdir(mine2) and ("swept a stale guest save folder (pid %d)" % c2.pid) in c3.text(),
        "guest: the next coop start swept the killed guest's folder (log line present)")
    # MUST-FAIL control: the BASE exe has no guest folder logic at all
    c4 = Con("guest", "g4_base", ARGS.base, env, 200, ARGS.root, ARGS.out)
    wait_all([c4], 300)
    say(os.path.isdir(os.path.join(tmproot)) and not any(n.startswith("sm64ds-coop-%d" % c4.pid) for n in os.listdir(tmproot)),
        "guest MUST-FAIL: the BASE exe makes no sm64ds-coop folder and would have used the install save path instead")


# ---------------------------------------------------------------- relay rig
def start_relay(row):
    port = BASEPORT + 30
    d = os.path.join(ARGS.out, row)
    os.makedirs(d, exist_ok=True)
    log = os.path.join(d, "relay.log")
    e = {k: v for k, v in os.environ.items() if not k.startswith("SM64DS_")}
    e.update({"SM64DS_RELAY_PORT": str(port), "SM64DS_RELAY_BIND": "127.0.0.1",
              "SM64DS_RELAY_RATE_PPS": "240", "SM64DS_RELAY_MAX_CHILDREN": "15",
              "SM64DS_RELAY_STATS_S": "5", "PYTHONUNBUFFERED": "1"})
    f = open(log, "wb")
    p = subprocess.Popen([sys.executable, os.path.join(REPO, "port", "tools", "relay", "relay.py")], env=e,
                         stdin=subprocess.DEVNULL, stdout=f, stderr=subprocess.STDOUT, creationflags=0x08000000)
    time.sleep(1.0)
    return p, f, port, log


def stop_relay(p, f):
    p.terminate()
    try:
        p.wait(5)
    except Exception:
        p.kill()
    f.close()


OPENING = {
    "SM64DS_SKIP_MENU": "1",
    "SM64DS_SCENE_WINDOW": "1",
    "SM64DS_SCENE_FRAMES": "100000",
}


def coop_env(i, relay, host, sav, extra=None):
    e = {"SM64DS_ADVENTURE": "1", "SM64DS_VS_PLAYERS": None, "SM64DS_HUB_REPORT_S": "2",
         "SM64DS_PLAYER_NAME": "Cs%d" % i, "SM64DS_PLAYER_COLOR": "%02x%02x%02x" % (30 + i * 11, 220 - i * 7, 60 + i * 9),
         "SM64DS_COMMS_RELAY": relay[0], "SM64DS_COMMS_CODE": relay[1],
         "SM64DS_ROOM_KIND": "coop", "SM64DS_COOP": "1", "SM64DS_BOOT_FILE": "0",
         "SM64DS_SAVE_PATH": None, "SM64DS_COOP_TEMP_ROOT": None}
    e.update(OPENING)
    if host:
        e["SM64DS_COOP_HOST"] = "1"
        e["SM64DS_COOP_SAVE"] = sav
        e["SM64DS_COOP_SAVE_NAME"] = "Pair Room"
    else:
        e["SM64DS_COOPSAVE_BOOT_FETCH"] = "40000"
    if extra:
        e.update(extra)
    return e


def snap_guest(c, tmp, store):
    pth = os.path.join(tmp, "sm64ds-coop-%d" % c.pid, "guest.coopsav")
    if os.path.exists(pth):
        try:
            store.append((time.time(), open(pth, "rb").read()))
        except Exception:
            pass


def pair(row, host_extra, guest_extra, host_seed_stars, frames_h=1700, frames_g=1600, gap=0.5, exe=None):
    """-> (host con, guest con, host file path, guest snapshots, events dict)"""
    shutil.rmtree(os.path.join(ARGS.out, row), ignore_errors=True)
    sav = os.path.join(ARGS.out, row, "hostinstall", "save", "coop", "Pair Room.coopsav")
    write(sav, image(TAG_C, payload(stars=host_seed_stars)))
    rp, rf, port, rlog = start_relay(row)
    relay = ("127.0.0.1:%d" % port, "CS%d" % (PID % 1000))
    gtmp = os.path.join(ARGS.out, row, "guesttemp")
    os.makedirs(gtmp, exist_ok=True)
    ch = Con(row, "host", exe or ARGS.exe, coop_env(0, relay, True, sav, host_extra), frames_h, ARGS.root, ARGS.out)
    time.sleep(gap)
    cg = Con(row, "guest", exe or ARGS.exe, coop_env(1, relay, False, None, dict(guest_extra, SM64DS_COOP_TEMP_ROOT=gtmp)), frames_g, ARGS.root, ARGS.out)
    snaps = []
    ev = {}
    last_host = [open(sav, "rb").read()]
    def tick():
        snap_guest(cg, gtmp, snaps)
        now = time.time()
        gt = cg.text()
        if "t_probe" not in ev and "PROBE star" in gt:
            ev["t_probe"] = now
        try:
            cur = open(sav, "rb").read()
        except Exception:
            cur = last_host[0]
        if cur != last_host[0]:
            ev.setdefault("t_file_changes", []).append(now)
            try:
                ev.setdefault("changes", []).append((now, set(stars_of(parse(cur)[(0, 0)][2]))))
            except Exception:
                pass
            last_host[0] = cur
    wait_all([ch, cg], 900, tick)
    stop_relay(rp, rf)
    ev["relay_log"] = rlog
    return ch, cg, sav, snaps, ev


def row_pair():
    seed = [(0, 0)]
    gx = (1, 2)          # a star the GUEST collects
    hx = (2, 3)          # a star the HOST collects
    ch, cg, sav, snaps, ev = pair("pair", {"SM64DS_COOPSAVE_PROBE": "star:%d:%d@1100" % (hx[0], hx[1])},
                                  {"SM64DS_COOPSAVE_PROBE": "star:%d:%d@700" % gx}, seed)
    th, tg = ch.text(), cg.text()
    for n, c, t in (("host", ch, th), ("guest", cg, tg)):
        info("pair %s rc=%s exe=%s coopsave lines: %s" % (n, c.rc, sha(c.exe), [l[:110] for l in re.findall(r"(?m)^\[coopsave\].*$", t)][:12]))
    say(ch.rc == 0 and cg.rc == 0 and not M.text(ch.log).count("FAULT"), "pair: both consoles exited 0")
    say("fetch ok" in tg and "host record seeded into slot 0" in tg, "pair: the guest FETCHED the host record before the title (log: %s)" % re.findall(r"fetch ok in \d+ ms[^\n]*", tg))
    # guest's temp image after seeding: first snapshot
    first = snaps[0][1] if snaps else b""
    pr = parse(first) if first else {}
    say(bool(pr) and pr[(0, 0)][0] == TAG_C and pr[(0, 0)][1] and (0, 0) in stars_of(pr[(0, 0)][2]),
        "pair: the guest temp image holds the host's star (0,0) with the coop tag and a valid checksum (%d snapshots)" % len(snaps))
    # guest star reaches the host file, byte diff
    final = open(sav, "rb").read()
    prf = parse(final)
    st = stars_of(prf[(0, 0)][2])
    say(gx in st and hx in st and (0, 0) in st, "pair: the host .coopsav ends with the seed star, the GUEST's star %s and the HOST's star %s: %s" % (gx, hx, st))
    seedimg = image(TAG_C, payload(stars=seed))
    diff = [(i, seedimg[i], final[i]) for i in range(CHIP) if seedimg[i] != final[i]]
    paydiff = sorted(set((i - 10, a, b) for i, a, b in diff if 10 <= i < 10 + 0x44) )
    info("pair: host image byte diff vs the seed: %d bytes; payload bytes (offset,old,new): %s" % (len(diff), paydiff))
    want = {(0x14 + gx[0] - 0, 0, 1 << gx[1]), (0x14 + hx[0], 0, 1 << hx[1])}
    got = set(paydiff)
    say(got == want, "pair: the ONLY payload bytes that changed are the two star bytes %s" % sorted(want))
    # slot 3 (minigames, 0x180 / 0x1180, up to 0x2e4+10 bytes) is the HOST's own probe: SaveCurrentFile writes it too
    outside = [i for i, a, b in diff if not (0 <= i < 0x80 or 0x1000 <= i < 0x1080 or 0x180 <= i < 0x180 + 0x2f0 or 0x1180 <= i < 0x1180 + 0x2f0)]
    say(not outside and prf[(0, 0)][1] and prf[(0, 1)][1], "pair: nothing outside slot 0's two records (and the host's own minigame block) changed; both copies' checksums valid")
    # latency: guest probe to a host file change
    if "t_probe" in ev and ev.get("changes"):
        later = [t for t, st2 in ev["changes"] if gx in st2]
        lat = (later[0] - ev["t_probe"]) if later else None
        info("pair: guest star logged at +0.0 s, the host file first held it %s s later (0.1 s poll, log read lags up to 0.1 s; changes seen: %s)" % (None if lat is None else round(lat, 2), [(round(t - ev["t_probe"], 2), sorted(st2)) for t, st2 in ev["changes"]]))
        say(lat is not None and lat <= 3.0, "pair: the guest's star reached the host's file within 3 s (%s s)" % (None if lat is None else round(lat, 2)))
    else:
        say(False, "pair: could not time the guest star (no probe / no file change seen)")
    say("host merged 1 guest bits" in th or re.search(r"host merged \d+ guest bits", th) is not None, "pair: host log shows the merge")
    # host star to guest
    lastg = snaps[-1][1] if snaps else b""
    prg = parse(lastg) if lastg else {}
    gst = stars_of(prg[(0, 0)][2]) if prg else []
    say(hx in gst and gx in gst, "pair: the guest temp image ends with BOTH stars %s (host's came over the wire): %s" % ([gx, hx], gst))
    say(re.search(r"guest took \d+ bits from the host record", tg) is not None, "pair: guest log shows it took the host's bits")
    return ch, cg


def row_forge():
    # a host with stars; the guest sends forged records; the host file must keep every bit
    seed = [(0, 0), (4, 6)]
    rows = {}
    for mode, what in (("clear", "a record with every bit clear"), ("hostclear", "a fake HOST record with every bit clear"),
                       ("badlen", "a 40-byte payload"), ("badflags", "non-zero reserved bytes"),
                       ("badrole", "from_host = 2"), ("badreq", "a SAVE_REQ with a non-zero payload")):
        ch, cg, sav, snaps, ev = pair("forge_" + mode, {}, {"SM64DS_COOPSAVE_FORGE": "%s@900" % mode}, seed, frames_h=1300, frames_g=1200)
        th, tg = ch.text(), cg.text()
        final = open(sav, "rb").read()
        pr = parse(final)
        st = stars_of(pr[(0, 0)][2])
        sent = re.search(r"FORGE %s sent=(\d)" % mode, tg)
        drops = re.findall(r"(?m)^\[coopsave\] DROP seat \d+: .*$", th + tg)
        keep = all(s in st for s in seed)
        okfile = pr[(0, 0)][1] and pr[(0, 1)][1]
        info("forge %s: sent=%s, host stars after %s, drop lines %s" % (mode, sent.group(1) if sent else "?", st, [d[-60:] for d in drops][:3]))
        say(sent is not None and sent.group(1) == "1" and keep and okfile and ch.rc == 0 and cg.rc == 0,
            "forge %s (%s): sent; host file keeps every seeded bit %s; checksums valid; both consoles exit 0" % (mode, what, seed))
        if mode in ("badlen", "badflags", "badrole", "badreq"):
            say(len(drops) >= 1, "forge %s: the receiver DROPPED it and logged the reason (%d drop lines)" % (mode, len(drops)))
        if mode == "hostclear":
            say(len(drops) >= 1, "forge hostclear: a record claiming to be the host's is refused by the host (role) (%d drop lines)" % len(drops))
    # the honest limit: a guest CAN add bits
    ch, cg, sav, snaps, ev = pair("forge_setall", {}, {"SM64DS_COOPSAVE_FORGE": "setall@900"}, seed, frames_h=1300, frames_g=1200)
    pr = parse(open(sav, "rb").read())
    st = stars_of(pr[(0, 0)][2])
    say(len(st) == 240 and pr[(0, 0)][1], "forge setall (the honest limit): a guest can ADD bits, it set every star byte (%d stars) and nothing else broke" % len(st))


def row_timeout():
    d = os.path.join(ARGS.out, "timeout")
    shutil.rmtree(d, ignore_errors=True)
    rp, rf, port, rlog = start_relay("timeout")
    relay = ("127.0.0.1:%d" % port, "CT%d" % (PID % 1000))
    gtmp = os.path.join(d, "guesttemp")
    os.makedirs(gtmp)
    c = Con("timeout", "guest", ARGS.exe, coop_env(1, relay, False, None, {"SM64DS_COOPSAVE_BOOT_FETCH": "1500", "SM64DS_COOP_TEMP_ROOT": gtmp}), 400, ARGS.root, ARGS.out)
    wait_all([c], 200)
    stop_relay(rp, rf)
    t = c.text()
    m = re.search(r"fetch TIMEOUT after (\d+) ms \(budget (\d+)\)", t)
    info("timeout: %s rc=%s wall %.1f s" % (m.group(0) if m else "no timeout line", c.rc, (c.t_end or time.time()) - c.t0))
    say(c.rc == 6 and m is not None and 1500 <= int(m.group(1)) <= 1500 + 600, "timeout: the guest gives up after the %s ms budget (measured %s ms) and exits with code %s" % (m.group(2) if m else "?", m.group(1) if m else "?", c.rc))
    left = []
    for n in os.listdir(gtmp):
        for f in os.listdir(os.path.join(gtmp, n)):
            left.append((n, f, os.path.getsize(os.path.join(gtmp, n, f))))
    info("timeout: temp folder after the failed join: %s" % left)
    say(not any(sz > 0 and f.endswith(".coopsav") and open(os.path.join(gtmp, n, f), "rb").read() != bytes([255]) * CHIP for n, f, sz in left),
        "timeout: no save record was written for the failed join")
    # MUST-FAIL control: the BASE exe has no coop fetch, so it never gives up with 6
    rp, rf, port, rlog = start_relay("timeout_base")
    relay = ("127.0.0.1:%d" % port, "CU%d" % (PID % 1000))
    cb = Con("timeout", "guest_base", ARGS.base, coop_env(1, relay, False, None, {"SM64DS_COOPSAVE_BOOT_FETCH": "1500", "SM64DS_COOP_TEMP_ROOT": gtmp}), 400, ARGS.root, ARGS.out)
    wait_all([cb], 200)
    stop_relay(rp, rf)
    say(cb.rc != 6 and "fetch TIMEOUT" not in cb.text(), "timeout MUST-FAIL: the BASE exe with the same env does not time out (rc %s): it would play on with no host record" % cb.rc)


def main():
    global ARGS
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--base", required=True)
    ap.add_argument("--root", default="C:/tmp/sm64ds-wire1", help="a tree with the ROM-derived assets (SM64DS_ASSET_ROOT)")
    ap.add_argument("--out", default=os.path.join(REPO, "build", "coop_save_proof"))
    ap.add_argument("rows", nargs="+")
    ARGS = ap.parse_args()
    ARGS.exe = os.path.abspath(ARGS.exe)
    ARGS.base = os.path.abspath(ARGS.base)
    os.makedirs(ARGS.out, exist_ok=True)
    table = {"unit": row_unit, "fence": row_fence, "host": row_host, "guest": row_guest,
             "timeout": row_timeout, "pair": row_pair, "forge": row_forge}
    for r in ARGS.rows:
        table[r]()
    bad = [r for r in RES if r.startswith("FAIL")]
    print("\n%d PASS %d FAIL" % (len([r for r in RES if r.startswith("PASS")]), len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
