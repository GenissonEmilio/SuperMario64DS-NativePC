#!/usr/bin/env python3
"""mk_symtab.py MAP OUT -- the plugin API's name -> address table.

Reads the MSVC /MAP file the link wrote and writes a compact text table next to
the exe (walk_window.symtab): a header line "# sm64ds-symtab 1 base=<hex>" (the
image's preferred load address; the exe links with /DYNAMICBASE:NO so it loads
there) and one "name<TAB>address" line per public symbol, the address being the
"Rva+Base" column. hal/mod_plugins.cpp's symbol() reads it; with no table it
falls back to parsing the map itself. String literals and compiler constants
are left out; they are not things a plugin can name."""
import re, sys

SKIP = ('??_C@', '__real@', '__xmm@', '__imp_', '??_R', '__ehfuncinfo', '__tryblocktable',
        '__unwindtable', '__ip2state')


def main(mp, out):
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
        o.write('# sm64ds-symtab 1 base=%s\n' % base)
        for n, a in names.items():
            o.write('%s\t%s\n' % (n, a))
    print('mk_symtab: %d names -> %s' % (len(names), out))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
