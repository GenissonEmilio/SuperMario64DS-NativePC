#!/usr/bin/env python3
"""dataalias -- bind decorated DATA references onto the hosted C data.

    python port/tools/dataalias.py --unresolved <list> --map <map> --out port/hal/portsync3_data_aliases.cpp

<list> is the linker's unresolved-external lines, one per line, in the form
parse_link.py writes them:  "<pretty text>" (<decorated>)  or a bare name.

Main's class TUs declare ROM data at file scope with C++ linkage, so MSVC asks
for `?data_ov006_0213dd34@@3PAEA` while the ROM mounts (build/port/host-src/
ov*_syms.c, tools/ovdata.py) and the arm9 data (hal/auto_bss.cpp, romdata)
define the C name `_data_ov006_0213dd34`. For data an /alternatename is the
right bridge: no calling convention is involved (facegen.py's ALIAS class).

Rows left out on purpose, each printed with its reason:
  * pointer-to-member data (`P8` in the mangle): handled by pmfvalues.py, which
    builds real host values instead of aliasing ROM bytes;
  * a name the link has no C definition for (the C symbol is missing from the
    map): that is a hosting gap, not an alias.
"""
import argparse
import re
import sys


def load_defs(map_path):
    txt = open(map_path, encoding="utf-8", errors="replace").read()
    return set(re.findall(r"\s(_[\w$@?]+|\?[\w$@?]+)\s+[0-9a-f]{8}\s", txt))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unresolved", required=True)
    ap.add_argument("--map", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    defs = load_defs(a.map)
    rows = []
    skipped = []
    for line in open(a.unresolved, encoding="utf-8", errors="replace"):
        line = line.split("  <- ")[0].strip()
        if not line:
            continue
        m = re.search(r"\((\?[\w$@?]+)\)$", line)
        dec = m.group(1) if m else (line if line.startswith("?") else None)
        if not dec:
            continue
        mm = re.match(r"\?((?:data|g)_\w+|\w+)@@3(.*)$", dec)
        if not mm:
            continue
        name, ty = mm.group(1), mm.group(2)
        if "P8" in ty or "Q8" in ty or "::*" in line:
            skipped.append((dec, "pointer to member"))
            continue
        flat = "_" + name
        if flat not in defs:
            skipped.append((dec, "no C definition %s in the link" % flat))
            continue
        rows.append((dec, flat))
    rows = sorted(set(rows))
    with open(a.out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("/* hal/portsync3_data_aliases.cpp -- decorated DATA names onto the hosted C data.\n")
        fh.write(" *\n * PORTSYNC3. The second pass of hal/portsync2_data_aliases.cpp: the names the\n")
        fh.write(" * first link of the 2026-10-14 main merge left unresolved, bound the same way\n")
        fh.write(" * (see port/tools/dataalias.py for the rule and for what it leaves out).\n")
        fh.write(" * %d rows. GENERATED, then checked in. Includes nothing.\n */\n\n" % len(rows))
        for dec, flat in rows:
            fh.write('#pragma comment(linker, "/alternatename:%s=%s")\n' % (dec, flat))
    print("dataalias: %d rows -> %s" % (len(rows), a.out))
    for dec, why in skipped:
        print("  left out: %s (%s)" % (dec, why))


if __name__ == "__main__":
    main()
