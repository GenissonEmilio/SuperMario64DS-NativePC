#!/usr/bin/env python3
"""sinit_noops -- generate port/hal/ov_sinit_noops.cpp.

    python port/tools/sinit_noops.py            write the file
    python port/tools/sinit_noops.py --check    exit 1 if the file is stale
    python port/tools/sinit_noops.py --list     print the rows only

WHY. The port used to call the overlays' hand-matched static initialisers
(`__sinit_ov085_0212f2a8` ...) from hal/actor_overlays.cpp, hal/scene_mg.cpp and
the actor_classes_ov*.cpp tables, right after an overlay's data was mounted.
Main replaced those hand files (src/__sinit_ovNNN_ADDR.c) with the compiler's
own initialisers: the objects they filled are now DEFINED by the class's
production TU, and the DS compiler emitted the TU's initialisers as one
`__sinit_<tu>.cpp` function at the same address.

On MSVC the same TU's initialisers (the State tables, the SharedFilePtr
constructions) are ordinary static initialisation: they run once, before main.
So the explicit call has nothing left to do, and the callers keep their order
and their text. This file gives each retired name a definition that does
nothing, and says which TU's initialiser it stands for.

WHAT THIS DOES NOT DO (the reversible choice, see out/PORTSYNC3/DECISIONS.md):
the DS re-runs an overlay's initialisers and re-zeroes its .bss every time the
overlay is loaded; MSVC initialises a TU once. An object a TU owns therefore
keeps whatever the previous load left in it. Replacing a no-op here with a
function that restores the object's start-up image is the upgrade path.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(REPO, "port", "hal", "ov_sinit_noops.cpp")
SEGDIR = os.path.join(REPO, "port", "hal", "sinit_seg")
SEGS = os.path.join(REPO, "port", "sinit_segs.cmake")
# TUs whose statics are already kept off the CRT by hal/s4ov0_overlay_sinit.h
S4OV0 = ("src/game/actors/d_a_obj_fm_battan.cpp", "src/game/actors/d_a_obj_kb1_billboard.cpp",
         "src/game/actors/d_a_star_gate.cpp")
NAME_RE = re.compile(r"\b__sinit_ov(\d{3})_([0-9a-f]{8})\b")
SYM_RE = re.compile(r"^(\S+)\s+kind:function\(([^)]*)\)\s+addr:0x([0-9a-fA-F]+)")


def referenced():
    refs = {}
    for root in ("port/hal", "port/unmatched", "port/tests", "port/ntr"):
        top = os.path.join(REPO, root)
        for dp, _dn, fn in os.walk(top):
            for f in fn:
                if not f.endswith((".cpp", ".c", ".h", ".inc")):
                    continue
                p = os.path.join(dp, f)
                if os.path.normpath(p) == os.path.normpath(OUT):
                    continue
                try:
                    text = open(p, encoding="utf-8", errors="replace").read()
                except OSError:
                    continue
                # only code, not the comments that talk about a sinit
                text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
                text = re.sub(r"//[^\n]*", "", text)
                for m in NAME_RE.finditer(text):
                    refs.setdefault(m.group(0), set()).add(
                        os.path.relpath(p, REPO).replace(os.sep, "/"))
    return refs


def defined_in_src():
    defs = set()
    top = os.path.join(REPO, "src")
    for dp, _dn, fn in os.walk(top):
        for f in fn:
            if not f.endswith((".c", ".cpp")):
                continue
            if not f.startswith("__sinit_ov"):
                continue
            m = NAME_RE.match(f)
            if m:
                defs.add(m.group(0))
    return defs


def defined_in_port():
    """sinit names a port file already DEFINES by hand (hal/link21_rows.cpp keeps
    two); a no-op for them would be a second definition."""
    out = set()
    pat = re.compile(r"\b(__sinit_ov\d{3}_[0-9a-f]{8})\s*\([^)]*\)\s*\{")
    for root in ("port/hal", "port/unmatched", "port/tests", "port/ntr"):
        for dp, _dn, fn in os.walk(os.path.join(REPO, root)):
            for f in fn:
                if not f.endswith((".cpp", ".c", ".inc")):
                    continue
                p = os.path.join(dp, f)
                if os.path.normpath(p) == os.path.normpath(OUT):
                    continue
                try:
                    text = open(p, encoding="utf-8", errors="replace").read()
                except OSError:
                    continue
                text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
                text = re.sub(r"//[^\n]*", "", text)
                out.update(pat.findall(text))
    return out


def config_names():
    out = {}
    for ov in range(0, 128):
        p = os.path.join(REPO, "config", "arm9", "overlays", "ov%03d" % ov,
                         "symbols.txt")
        if not os.path.isfile(p):
            continue
        for line in open(p, encoding="utf-8", errors="replace"):
            m = SYM_RE.match(line.strip())
            if m:
                out[(ov, int(m.group(3), 16))] = m.group(1)
    return out


def rows():
    refs = referenced()
    have = defined_in_src() | defined_in_port()
    cfg = config_names()
    res = []
    # every overlay initialiser the config knows, not only the ones a port file
    # calls: the unreferenced ones are run once, late (port_ov_sinit_late)
    for (ov, addr), cname in sorted(cfg.items()):
        if cname.startswith("__sinit_") and cname.endswith(".cpp"):
            nm = "__sinit_ov%03d_%08x" % (ov, addr)
            if nm not in refs and nm not in have:
                refs[nm] = set()
    for name in sorted(refs):
        if name in have:
            continue
        m = NAME_RE.match(name)
        ov, addr = int(m.group(1)), int(m.group(2), 16)
        res.append((name, ov, addr, cfg.get((ov, addr)), sorted(refs[name])))
    return res


def src_index():
    idx = {}
    for dp, _dn, fn in os.walk(os.path.join(REPO, "src")):
        for f in fn:
            if f.endswith((".c", ".cpp")):
                idx.setdefault(f, []).append(
                    os.path.relpath(os.path.join(dp, f), REPO).replace(os.sep, "/"))
    return idx


def tu_of(cfgname, idx):
    """__sinit_<tu>.cpp -> the src/ file of that TU, or None."""
    if not cfgname or not cfgname.startswith("__sinit_"):
        return None
    stem = cfgname[len("__sinit_"):]
    if stem.endswith(".cpp"):
        stem = stem[:-4]
    hits = idx.get(stem + ".cpp", []) + idx.get(stem + ".c", [])
    return hits[0] if len(hits) == 1 else None


def seg_name(tu):
    """One input-section family per TU inside the single output section "sinit":
    sinit$<tu>$a (begin marker), sinit$<tu>$m (the TU's initialiser pointers),
    sinit$<tu>$z (end marker). The linker sorts by the text after the first $, so
    a TU's three parts are adjacent, and one output section keeps the PE section
    count flat (there are 200+ of these)."""
    return "sinit$" + re.sub(r"[^A-Za-z0-9_]", "_", os.path.splitext(os.path.basename(tu))[0])


def plan(res):
    idx = src_index()
    out = []
    seen = {}
    for name, ov, addr, cfgname, callers in res:
        tu = tu_of(cfgname, idx)
        if tu in S4OV0:
            tu = None
        if tu is not None and tu in seen:
            tu = None  # a second sinit name for the same TU stays a no-op
        if tu is not None:
            seen[tu] = name
        out.append((name, ov, addr, cfgname, callers, tu))
    return out


def render(res):
    rows = plan(res)
    out = []
    out.append("// GENERATED by port/tools/sinit_noops.py -- do not edit by hand.")
    out.append("// See that file for what these stand for and what they leave undone.")
    live = sum(1 for r in rows if r[5])
    out.append("// %d retired overlay initialisers, %d of them re-run their TU's own initialisers." % (len(rows), live))
    out.append("")
    out.append("#pragma warning(disable : 4075)")
    out.append("typedef void (__cdecl *port_sinit_fn)(void);")
    out.append("static void port_sinit_run(port_sinit_fn const *begin, port_sinit_fn const *end)")
    out.append("{")
    out.append("    for (port_sinit_fn const *p = begin + 1; p < end; ++p)")
    out.append("        if (*p) (*p)();")
    out.append("}")
    out.append("")
    for name, ov, addr, cfgname, callers, tu in rows:
        if tu:
            sg = seg_name(tu)
            out.append('#pragma section("%s$a", read)' % sg)
            out.append('#pragma section("%s$z", read)' % sg)
    out.append("")
    for name, ov, addr, cfgname, callers, tu in rows:
        if tu:
            sg = seg_name(tu)
            v = re.sub(r"[^A-Za-z0-9_]", "_", sg)
            out.append('__declspec(allocate("%s$a")) static port_sinit_fn const %s_b = 0;' % (sg, v))
            out.append('__declspec(allocate("%s$z")) static port_sinit_fn const %s_e = 0;' % (sg, v))
    out.append("")
    out.append('extern "C" {')
    for name, ov, addr, cfgname, callers, tu in rows:
        note = "ov%03d 0x%08x: %s; called from %s" % (
            ov, addr, cfgname or "(no config symbol)", ", ".join(callers) or "nobody (run late)")
        if tu:
            v = re.sub(r"[^A-Za-z0-9_]", "_", seg_name(tu))
            out.append("void %s(void) { port_sinit_run(&%s_b, &%s_e); }  /* %s; runs %s */" % (
                name, v, v, note, tu))
        else:
            out.append("void %s(void) {}  /* %s */" % (name, note))
    out.append("}")
    out.append("")
    out.append("/* Run once, after the ov0 handle table exists (hal/s4ov0_ov0table.cpp): the")
    out.append("   initialisers no port file calls. They used to run before main; the ones that")
    out.append("   look a file handle up need the table first. */")
    out.append('extern "C" void port_ov_sinit_late(void)')
    out.append("{")
    for name, ov, addr, cfgname, callers, tu in rows:
        if tu and not callers:
            out.append("    %s();" % name)
    out.append("}")
    out.append("")
    return "\n".join(out)


def write_segs(res):
    """port/sinit_segs.cmake rows + one tiny header per TU."""
    rows = [r for r in plan(res) if r[5]]
    os.makedirs(SEGDIR, exist_ok=True)
    keep = set()
    lines = ["# GENERATED by port/tools/sinit_noops.py -- do not edit by hand.",
             "# TU sources whose dynamic initialisers are held back from the CRT and",
             "# run by the matching __sinit_ovNNN_ADDR (hal/ov_sinit_noops.cpp).",
             "set(PORT_SINIT_SEG_ROWS"]
    for name, ov, addr, cfgname, callers, tu in rows:
        h = re.sub(r"[^A-Za-z0-9_]", "_", seg_name(tu)) + ".h"
        keep.add(h)
        with open(os.path.join(SEGDIR, h), "w", encoding="utf-8", newline="\n") as fh:
            fh.write('/* GENERATED by port/tools/sinit_noops.py. The CRT does not walk this section;\n'
                     '   %s runs it. */\n' % name)
            fh.write('#pragma section("%s$m", read)\n#pragma warning(push)\n'
                     '#pragma warning(disable : 4075)\n#pragma init_seg("%s$m")\n#pragma warning(pop)\n'
                     % (seg_name(tu), seg_name(tu)))
        lines.append("    %s|%s" % (tu, h))
    lines.append(")")
    for f in os.listdir(SEGDIR):
        if f not in keep:
            os.remove(os.path.join(SEGDIR, f))
    with open(SEGS, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lines) + "\n")


def main():
    res = rows()
    if "--list" in sys.argv:
        for r in res:
            print(r[0], r[3])
        return 0
    text = render(res)
    if "--check" in sys.argv:
        cur = open(OUT, encoding="utf-8").read() if os.path.isfile(OUT) else ""
        if cur.replace("\r\n", "\n") != text:
            print("sinit_noops: %s is stale" % OUT)
            return 1
        print("sinit_noops: OK, %d rows" % len(res))
        return 0
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    write_segs(res)
    print("sinit_noops: wrote %d rows to %s" % (len(res), OUT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
