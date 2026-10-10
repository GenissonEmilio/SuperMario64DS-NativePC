#!/usr/bin/env python3
"""Fail if a built exe imports any DLL other than KERNEL32 (normal or delay-load).

The port exe may not grow a static import of user32/xinput/dinput8/setupapi/...: a new
DLL chain can map over the DS's fixed 0x02000000 reservation before the TLS callback
claims it. SDL3 is linked static with a lazy-resolution shim (third_party/SDL3_glue) so
this stays true; this guard is the post-link proof. Standard library only.
usage: sdl_import_guard.py <exe>
"""
import struct
import sys


def main(path):
    d = open(path, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    assert d[pe:pe + 4] == b"PE\0\0", "not a PE file"
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    opt = pe + 24
    magic = struct.unpack_from("<H", d, opt)[0]
    dd = opt + (96 if magic == 0x10B else 112)
    secs = []
    p = opt + struct.unpack_from("<H", d, pe + 20)[0]
    for i in range(nsec):
        vsz, va, rsz, raw = struct.unpack_from("<IIII", d, p + 8 + i * 40)
        secs.append((va, max(vsz, rsz), raw))

    def off(rva):
        for va, sz, raw in secs:
            if va <= rva < va + sz:
                return raw + rva - va
        raise ValueError("rva not in a section")

    def cstr(o):
        return d[o:d.index(b"\0", o)].decode("ascii", "replace")

    bad = []
    names = []
    imp_rva, _ = struct.unpack_from("<II", d, dd + 8)
    if imp_rva:
        o = off(imp_rva)
        while True:
            ilt, _, _, name_rva, iat = struct.unpack_from("<IIIII", d, o)
            if not name_rva:
                break
            dll = cstr(off(name_rva)).upper()
            names.append(dll)
            if dll != "KERNEL32.DLL":
                bad.append(dll)
            o += 20
    dly_rva, _ = struct.unpack_from("<II", d, dd + 13 * 8)
    if dly_rva:
        bad.append("(delay-load import directory present)")
    if bad:
        print("sdl_import_guard: FAIL, %s imports %s" % (path, ", ".join(bad)))
        return 1
    print("sdl_import_guard: ok, imports %s only" % ", ".join(names))
    return 0


sys.exit(main(sys.argv[1]))
