#!/usr/bin/env python3
"""mk_symtab.py MAP OUT EXE -- the plugin API's name -> address table.

Reads the MSVC /MAP file the link wrote and writes a compact text table next to
the exe (walk_window.symtab): a header line "# sm64ds-symtab 2 base=<hex> stamp=<hex> size=<hex>" (the
image's preferred load address; the exe links with /DYNAMICBASE:NO so it loads
there) and one "name<TAB>address" line per public symbol, the address being the
"Rva+Base" column. hal/mod_plugins.cpp's symbol() reads it; with no table it
falls back to parsing the map itself. String literals and compiler constants
are left out; they are not things a plugin can name.

The header line also carries the exe's PE TimeDateStamp and SizeOfImage
(stamp=<hex> size=<hex>), read from EXE. The game compares them with the image
it is running and refuses the table (symbol() answers NULL) on a mismatch, so a
table copied over from another build can never hand a plugin a wrong address."""
import re, struct, sys


def pe_stamp(exe):
    with open(exe, 'rb') as f:
        d = f.read(4096)
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    if d[pe:pe + 4] != b'PE\0\0':
        sys.exit('mk_symtab: %s is not a PE file' % exe)
    stamp = struct.unpack_from('<I', d, pe + 8)[0]
    size = struct.unpack_from('<I', d, pe + 24 + 56)[0]
    return stamp, size

SKIP = ('??_C@', '__real@', '__xmm@', '__imp_', '??_R', '__ehfuncinfo', '__tryblocktable',
        '__unwindtable', '__ip2state')


def main(mp, out, exe):
    stamp, size = pe_stamp(exe)
    base = '00400000'
    names = {}
    pat = re.compile(r'^\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8})\b')
    with open(mp, 'r', errors='replace') as f:
        for line in f:
            m = re.search(r'Preferred load address is ([0-9a-fA-F]{8})', line)
            if m:
                base = m.group(1).lower()
                continue
            m = pat.match(line)
            if not m:
                continue
            name, addr = m.group(1), m.group(2).lower()
            if name.startswith(SKIP):
                continue
            if int(addr, 16) < int(base, 16):
                continue
            names.setdefault(name, addr)
    with open(out, 'w', newline='\n') as o:
        o.write('# sm64ds-symtab 2 base=%s stamp=%08x size=%08x\n' % (base, stamp, size))
        for n, a in names.items():
            o.write('%s\t%s\n' % (n, a))
    print('mk_symtab: %d names -> %s' % (len(names), out))


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3])
