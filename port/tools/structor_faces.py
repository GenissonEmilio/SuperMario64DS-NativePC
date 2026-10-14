#!/usr/bin/env python3
"""structor_faces -- host constructors/destructors for main's file-handle wrappers.

    python port/tools/structor_faces.py --root <repo> --out <file.cpp>
    python port/tools/structor_faces.py --root <repo> --report

WHY THIS EXISTS (PORTSYNC2, the 2026-10 main merge). Main's class TUs define
their static file handles as small wrapper classes,

    struct KpaModelFilePtr : SharedFilePtr {
        u32 words[2];
        KpaModelFilePtr(u32 fileID);
        ~KpaModelFilePtr();
    };
    KpaModelFilePtr data_ov060_0211ac78(0x377);

and leave each wrapper's constructor and destructor UNDEFINED on purpose. On
the DS the TU manifest (config/tu_manifest.d/**/<tu>.json,
"undefined_symbol_aliases") binds the Itanium names the compiler emits for
them -- _ZN15KpaModelFilePtrC1Ej, _ZN15KpaModelFilePtrD1Ev -- onto the ROM
veneers the cartridge really calls (func_02017acc, func_02017ab4,
SharedFilePtr::Construct, ...). MSVC emits ??0KpaModelFilePtr@@QAE@I@Z and
??1KpaModelFilePtr@@QAE@XZ for the same source, and nothing on the host
defines those: a link simulation of walk_window counted 782 of them, one
pair per wrapper, the largest single block of the merge's new unresolved
externals.

WHAT IT EMITS. For every manifest alias whose symbol is a single-component
class's C1 or D1 with a parameter list in the scalar subset below, a FACE:
the wrapper declared as a shadow struct carrying only its structors (this
file includes nothing, so the shadow cannot meet the real class), and each
structor defined as a call of the canonical veneer's FLAT host name with the
receiver as the first argument -- the same shape every face in
port/faces_sync.txt has. MSVC's constructor returns `this` itself and a D1
takes no flag, so neither needs the veneer's return value.

The canonical names are the manifest's, spelled flat (`_ZN13SharedFilePtr9
ConstructEj` stays that string): the host defines each of them (hal/, or a
reverse face in faces_sync_gen.cpp). Each is declared once, without
parameters, and called through a cast to the exact signature at each site,
so two wrappers that reach one veneer with different argument lists never
redeclare it.

A structor the port already defines by hand (a `Cls::Cls(` or `Cls::~Cls(`
under port/hal or port/unmatched) is left to that definition.

WHAT IT REFUSES, and lists with --report: a class name with more than one
component, any structor other than C1/D1, and any parameter outside the
subset. Those keep whatever the port already does for them.
"""

import argparse
import glob
import json
import os
import re
import sys

SCALAR = {
    "j": "unsigned int", "i": "int", "t": "unsigned short", "s": "short",
    "h": "unsigned char", "c": "char", "a": "signed char", "b": "bool",
    "m": "unsigned long", "l": "long",
}
ITANIUM = re.compile(r"^_ZN(\d+)([A-Za-z_]\w*)$")


def parse(symbol):
    """(class, kind, [param types]) or None for anything outside the subset."""
    m = ITANIUM.match(symbol)
    if not m:
        return None
    n = int(m.group(1))
    body = m.group(2)
    cls, rest = body[:n], body[n:]
    if len(cls) != n or not re.match(r"^[A-Za-z_]\w*$", cls):
        return None
    m2 = re.match(r"^(C1|D1)E(.*)$", rest)
    if not m2:
        return None
    kind, params = m2.group(1), m2.group(2)
    if params == "v":
        return cls, kind, []
    out = []
    for ch in params:
        if ch not in SCALAR:
            return None
        out.append(SCALAR[ch])
    if kind == "D1" and out:
        return None
    return cls, kind, out


def collect(root):
    rows, refused = {}, []
    pat = os.path.join(root, "config", "tu_manifest.d", "**", "*.json")
    for path in sorted(glob.glob(pat, recursive=True)):
        with open(path, encoding="utf-8") as f:
            man = json.load(f)
        for a in man.get("undefined_symbol_aliases") or []:
            sym, canon = a.get("symbol", ""), a.get("canonical_symbol", "")
            p = parse(sym)
            if p is None or not re.match(r"^[A-Za-z_]\w*$", canon):
                refused.append((sym, canon, os.path.relpath(path, root)))
                continue
            prev = rows.get(sym)
            if prev and prev[0] != canon:
                sys.exit("structor_faces: %s aliases onto both %s and %s"
                         % (sym, prev[0], canon))
            rows[sym] = (canon, p)
    return rows, refused


def port_defined(root, rows):
    """Structors the port already defines by hand (a class -> {"C1","D1"}),
    so a face here would be a second definition."""
    classes = {p[0] for _c, p in rows.values()}
    have = {}
    pat = re.compile(r"\b(\w+)::(~?)(\w+)\s*\(")
    for sub in ("hal", "unmatched"):
        for path in glob.glob(os.path.join(root, "port", sub, "**", "*.c*"),
                              recursive=True):
            with open(path, encoding="utf-8", errors="replace") as f:
                text = f.read()
            for m in pat.finditer(text):
                cls, tilde, name = m.groups()
                if cls == name and cls in classes:
                    have.setdefault(cls, set()).add("D1" if tilde else "C1")
    return have


def emit(rows):
    classes = {}
    for sym, (canon, (cls, kind, params)) in sorted(rows.items()):
        classes.setdefault(cls, {})[kind] = (canon, params, sym)
    canons = sorted({canon for canon, _p in rows.values()})
    out = ["/* GENERATED by port/tools/structor_faces.py from config/tu_manifest.d"
           " -- do not edit.",
           " * Host structors for the file-handle wrappers main's class TUs leave",
           " * undefined; see the generator's docstring. */",
           ""]
    out.append('extern "C" {')
    for c in canons:
        out.append("void %s(void);" % c)
    out.append("}")
    out.append("")
    for cls in sorted(classes):
        k = classes[cls]
        decl = ["struct %s {" % cls]
        if "C1" in k:
            decl.append("    %s(%s);" % (cls, ", ".join(k["C1"][1])))
        if "D1" in k:
            decl.append("    ~%s();" % cls)
        decl.append("};")
        out.extend(decl)
        if "C1" in k:
            canon, params, sym = k["C1"]
            formals = ", ".join("%s a%d" % (t, i) for i, t in enumerate(params))
            sig = ", ".join(["void *"] + params)
            args = ", ".join(["this"] + ["a%d" % i for i in range(len(params))])
            out.append("/* %s -> %s */" % (sym, canon))
            out.append("%s::%s(%s)\n{ ((void (*)(%s))&%s)(%s); }"
                       % (cls, cls, formals, sig, canon, args))
        if "D1" in k:
            canon, _params, sym = k["D1"]
            out.append("/* %s -> %s */" % (sym, canon))
            out.append("%s::~%s()\n{ ((void (*)(void *))&%s)(this); }"
                       % (cls, cls, canon))
        out.append("")
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--out")
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()
    rows, refused = collect(args.root)
    have = port_defined(args.root, rows)
    skipped = sorted(s for s, (_c, (cls, kind, _p)) in rows.items()
                     if kind in have.get(cls, ()))
    for s in skipped:
        del rows[s]
    if args.report or not args.out:
        print("%d structor faces, %d aliases outside the subset, %d the "
              "port already defines" % (len(rows), len(refused), len(skipped)))
        for s in skipped:
            print("  PORT-DEFINED %s" % s)
        for sym, canon, path in refused:
            print("  REFUSED %s -> %s  (%s)" % (sym, canon, path))
    if args.out:
        text = emit(rows)
        old = None
        if os.path.exists(args.out):
            with open(args.out, encoding="utf-8") as f:
                old = f.read()
        if old != text:
            os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
            with open(args.out, "w", encoding="utf-8") as f:
                f.write(text)
        print("structor_faces: %d faces -> %s" % (len(rows), args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
