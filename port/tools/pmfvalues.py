#!/usr/bin/env python3
"""pmfvalues -- build real host values for the ROM's pointer-to-member constants.

    python port/tools/pmfvalues.py --unresolved <list> --map <map> --out port/hal/pmf_host_values.cpp

THE PROBLEM. Main's class TUs declare ROM state functions as file-scope
pointer-to-member data (`extern PMF data_ov102_0214e278;`) and copy them into
their own State tables in the compiler's static initialiser. The ROM holds each
as an eight-byte pair {code address, this-adjust}, and the port compiles with
/vmg /vmm so that an MSVC pointer to member is the same eight bytes. The
decorated reference (`?data_ov102_0214e278@@3P8C@@AEXXZQ1@`) therefore had
nothing to bind to except the mount's ROM bytes, whose code word is a DS address:
the first call through it jumps into mapped data.

THE ANSWER. Define the constant natively. Each row here reads the pair out of
the cartridge, names the function its code word points at (config symbols), and
emits {&host function, 0} under the decorated name:

  * a MEMBER target (`_ZN7daCnn_c19func_ov098_0213b0a4Ev`): the host definition
    is the TU's own thiscall member; the pair holds its address, found in the
    link map by class and method name;
  * a FLAT target (`func_ov060_021169b0`, receiver as the first cdecl
    argument): the pair holds a generated __fastcall thunk of the PMF's arity,
    which forwards (this, args...) to the cdecl body. A __fastcall callee with
    (this, dummy, a1..an) is entered exactly like a thiscall member with n
    stack arguments, and pops them the same way.

The values are link-time constants, so a TU's static initialiser can copy them
before main() and a call through them lands on host code, with no seam at the
call site. An adjustment word other than zero, a target with no host
definition, or an overloaded member name refuses the row and says so.

Pointer-to-PMF arrays (`P8...` behind a second `*`) are not handled here; they
are tables the ROM's own sinit fills and need the table's owner.
"""
import argparse
import os
import pathlib
import re
import struct
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent
sys.path.insert(0, str(HERE))

SYM_RE = re.compile(r"^(\S+)\s+kind:(function|data|bss)(?:\(([^)]*)\))?\s+addr:0x([0-9a-fA-F]+)")
SEC_RE = re.compile(r"^\s+(\S+)\s+start:0x([0-9a-fA-F]+)\s+end:0x([0-9a-fA-F]+)\s+kind:(\w+)")


def load_syms(path):
    out = {}
    p = pathlib.Path(path)
    if not p.is_file():
        return out
    for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
        m = SYM_RE.match(line.strip())
        if m and m.group(2) == "function":
            out.setdefault(int(m.group(4), 16), m.group(1))
    return out


class Overlay:
    def __init__(self, n):
        self.n = n
        dl = REPO / ("config/arm9/overlays/ov%03d/delinks.txt" % n)
        secs = []
        for line in dl.read_text(encoding="utf-8-sig", errors="replace").splitlines():
            if line and not line.startswith(" "):
                break
            m = SEC_RE.match(line)
            if m:
                secs.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), m.group(4)))
        infile = [s for s in secs if s[3] != "bss"]
        self.base = infile[0][1]
        self.image = (REPO / ("extracted/overlays/overlay_%04d.bin" % n)).read_bytes()
        self.syms = load_syms(REPO / ("config/arm9/overlays/ov%03d/symbols.txt" % n))

    def word(self, addr):
        off = addr - self.base
        if not 0 <= off <= len(self.image) - 4:
            return None
        return struct.unpack_from("<I", self.image, off)[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unresolved", required=True)
    ap.add_argument("--map", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    map_txt = open(a.map, encoding="utf-8", errors="replace").read()
    defs = set(re.findall(r"\s(_[\w$@?]+|\?[\w$@?]+)\s+[0-9a-f]{8}\s", map_txt))
    arm9 = load_syms(REPO / "config/arm9/symbols.txt")
    ovs = {}

    def ov(n):
        if n not in ovs:
            ovs[n] = Overlay(n)
        return ovs[n]

    def target_name(n, code):
        for k in (n, 2, 4, 6, 9):
            try:
                t = ov(k).syms.get(code)
            except OSError:
                continue
            if t:
                return t
        return arm9.get(code)

    members = {}
    for d in defs:
        m = re.match(r"\?(\w+)@(\w+)@@", d)
        if m:
            members.setdefault((m.group(2), m.group(1)), []).append(d)

    rows = []
    refused = []
    for line in open(a.unresolved, encoding="utf-8", errors="replace"):
        line = line.split("  <- ")[0].strip()
        if "::*" not in line or "::**" in line:
            continue
        mm = re.search(r"\* (data_ov(\d+)_([0-9a-f]{8}))\)\((.*?)\)\"\s*\((\?[\w$@?]+)\)$", line)
        if not mm:
            refused.append((line[:90], "unparsed"))
            continue
        name, n, addr = mm.group(1), int(mm.group(2)), int(mm.group(3), 16)
        params, dec = mm.group(4).strip(), mm.group(5)
        arity = 0 if params in ("", "void") else params.count(",") + 1
        o = ov(n)
        code, adj = o.word(addr), o.word(addr + 4)
        if code is None or adj is None:
            refused.append((name, "address outside the overlay file image (a .bss table)"))
            continue
        if adj != 0:
            refused.append((name, "nonzero this-adjust 0x%08x" % adj))
            continue
        tgt = target_name(n, code)
        if not tgt:
            refused.append((name, "no function symbol at 0x%08x" % code))
            continue
        kind = None
        host = None
        if tgt.startswith("_ZN"):
            i, parts = 3, []
            while i < len(tgt) and tgt[i].isdigit():
                j = i
                while tgt[j].isdigit():
                    j += 1
                ln = int(tgt[i:j])
                parts.append(tgt[j:j + ln])
                i = j + ln
            if len(parts) == 2:
                cands = members.get((parts[0], parts[1]), [])
                if len(cands) == 1:
                    kind, host = "member", cands[0]
                elif len(cands) > 1:
                    refused.append((name, "member %s::%s is overloaded in the link: %s" % (parts[0], parts[1], cands)))
                    continue
            if kind is None and ("_" + tgt) in defs:
                kind, host = "flat", "_" + tgt
            if kind is None and ("__" + tgt[1:]) in defs:
                kind, host = "flat", "__" + tgt[1:]
            if kind is None:
                refused.append((name, "no host definition of %s" % tgt))
                continue
        else:
            if ("_" + tgt) in defs:
                kind, host = "flat", "_" + tgt
            else:
                refused.append((name, "no host definition of %s" % tgt))
                continue
        rows.append((name, dec, code, tgt, kind, host, arity))

    lines = []
    w = lines.append
    w("/* hal/pmf_host_values.cpp -- host values for the ROM's pointer-to-member constants.")
    w(" *")
    w(" * PORTSYNC3. GENERATED by port/tools/pmfvalues.py, then checked in. Read that")
    w(" * file for the rule. %d rows: each is a ROM {code, this-adjust} pair rebuilt as" % len(rows))
    w(" * {host function, 0}. A flat (cdecl) target gets a __fastcall thunk of the")
    w(" * PMF's arity. Includes nothing, for hal/actor_class_faces.cpp's reasons.")
    w(" */")
    w("")
    w("struct PortPmfPair { unsigned code; int adj; };")
    w("")
    for name, dec, code, tgt, kind, host, arity in rows:
        tag = name
        if kind == "member":
            w('extern "C" void port_pmft_%s(void);' % tag)
            w('#pragma comment(linker, "/alternatename:_port_pmft_%s=%s")' % (tag, host))
            tgt_expr = "(unsigned)&port_pmft_%s" % tag
        else:
            args = "".join(", int a%d" % i for i in range(1, arity + 1))
            call = "".join(", a%d" % i for i in range(1, arity + 1))
            w('extern "C" int %s_decl_%s(void *self%s);' % ("port_pmff", tag, "".join(", int" for _ in range(arity))))
            w('#pragma comment(linker, "/alternatename:_port_pmff_decl_%s=%s")' % (tag, host))
            w("static int __fastcall port_pmfthunk_%s(void *self, void *%s)" % (tag, args))
            w("{ return port_pmff_decl_%s(self%s); }" % (tag, call))
            tgt_expr = "(unsigned)&port_pmfthunk_%s" % tag
        w('extern "C" PortPmfPair port_pmfv_%s = { %s, 0 };  /* ROM 0x%08x %s */' % (tag, tgt_expr, code, tgt))
        w('#pragma comment(linker, "/alternatename:%s=_port_pmfv_%s")' % (dec, tag))
        w("")
    pathlib.Path(a.out).write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print("pmfvalues: %d rows (%d member, %d flat) -> %s" % (
        len(rows), sum(1 for r in rows if r[4] == "member"), sum(1 for r in rows if r[4] == "flat"), a.out))
    for name, why in refused:
        print("  refused: %s (%s)" % (name, why))


if __name__ == "__main__":
    main()
