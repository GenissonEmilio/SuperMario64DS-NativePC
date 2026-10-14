#!/usr/bin/env python3
"""hostarm_recover -- keep the port's `#ifdef _MSC_VER` arms across a main merge.

    python port/tools/hostarm_recover.py --old-ref <rev> --before <old config/> --report
    python port/tools/hostarm_recover.py --old-ref <rev> --before <old config/> --emit

WHY
---
The port line carried host-only arms INSIDE src/ files: `#ifdef _MSC_VER`
blocks that mwccarm never compiles (so the ROM bytes are untouched) and that
give MSVC what it cannot derive from the decomp's C++ -- above all the ROM's
FLAT destructor names (`extern "C" X *_ZN1XD0Ev(X *)`), which MSVC never
emits because it folds the Itanium D0/D1/D2 variants into one ~X().

A main merge resolved by rule ("src/ takes main") drops every such arm whose
file main folded into a production TU or rewrote: main's TU has no arm, the
flat name goes undefined, and the hand-written vtables in port/hal/ that name
it fail to link. The 2026-10 merge (749f5799a) dropped about 230 of them.

WHAT THIS DOES
--------------
For every OLD src/ file (at --old-ref) that the OLD port compiled (it is named
by a port/ slice list or port/CMakeLists.txt at --old-ref), that carries a
_MSC_VER arm, and whose code now lives in a src/ file WITHOUT an arm, it
writes port/unmatched/hostarm/<old stem>.cpp:

  * D0 FACES (the template almost every one of them used):
        extern "C" X *_ZN..D0Ev(X *thiz)
        { thiz->X::~X(); X::operator delete(thiz); return thiz; }
    are re-emitted against the NEW class: the flat name is carried across by
    (module, addr) like every other rename (port/tools/merge_remap.py), the
    class is read off the new mangled name, and the header is the one under
    include/ that declares it. The deallocation stays the class's own
    `operator delete`, which is what the old arm called.
  * ANYTHING ELSE is the old file with the _MSC_VER conditionals resolved as
    MSVC saw them (the arm kept, the mwccarm `#else` dropped), with renamed
    flat symbols and renamed classes substituted. These are marked REVIEW in
    the manifest: they compiled on the port line against the old headers and
    have not been compiled against main's.

It also writes port/unmatched/hostarm/MANIFEST.txt, and with --wire inserts
each recovered file into every slice list that named the old source (after
the row the old source was remapped to), so it reaches the same targets.
"""

import argparse
import collections
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(REPO / "tools"))
import merge_remap as MR  # noqa: E402
import srcpath as SP  # noqa: E402

OUT = REPO / "port" / "unmatched" / "hostarm"
ARM_RE = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif)\b.*_MSC_VER", re.M)


def git(*args):
    return subprocess.run(["git", "-C", str(REPO)] + list(args),
                          capture_output=True, text=True, check=True).stdout


def resolve_msc(text, defined=True):
    """Resolve conditionals on _MSC_VER as MSVC sees them (defined).

    Returns (text, ok). Conditionals not about _MSC_VER pass through. An
    expression mixing _MSC_VER with anything else is refused (ok False)."""
    out = []
    stack = []  # entries: ("msc", keep_now, any_taken) or ("other",)
    ok = True

    def live():
        return all(e[1] for e in stack if e[0] == "msc")

    for line in text.split("\n"):
        m = re.match(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$", line)
        if not m:
            if live():
                out.append(line)
            continue
        d, rest = m.group(1), m.group(2).split("//")[0].split("/*")[0].strip()
        if d in ("if", "ifdef", "ifndef"):
            if "_MSC_VER" in rest:
                if d == "ifdef" and rest == "_MSC_VER":
                    v = True
                elif d == "ifndef" and rest == "_MSC_VER":
                    v = False
                elif re.fullmatch(r"defined\s*\(?\s*_MSC_VER\s*\)?", rest):
                    v = True
                elif re.fullmatch(r"!\s*defined\s*\(?\s*_MSC_VER\s*\)?", rest):
                    v = False
                else:
                    ok = False
                    v = True
                if not defined:
                    v = not v
                stack.append(["msc", v, v])
            else:
                stack.append(["other"])
                if live():
                    out.append(line)
            continue
        if not stack:
            ok = False
            continue
        top = stack[-1]
        if d == "elif":
            if top[0] == "msc":
                ok = False
            elif live():
                out.append(line)
            continue
        if d == "else":
            if top[0] == "msc":
                top[1] = not top[2]
                top[2] = True
            else:
                if live():
                    out.append(line)
            continue
        if d == "endif":
            e = stack.pop()
            if e[0] == "other" and live():
                out.append(line)
    return "\n".join(out), ok and not stack


# a flat definition: `extern "C" ... name(` at line start, not a declaration
DEF_RE = re.compile(r'^extern\s+"C"\s+[^;\n(]*?\b([A-Za-z_]\w*)\s*\([^;\n]*\)\s*\{?\s*$', re.M)


def resolve_msc_negated(text):
    """The text as mwccarm sees it (_MSC_VER undefined)."""
    return resolve_msc(text, defined=False)[0]


D0_RE = re.compile(
    r'extern\s+"C"\s+(\w+)\s*\*\s*(_ZN\w+D0Ev)\s*\(\s*(\w+)\s*\*\s*(\w+)\s*\)\s*\{\s*'
    r'(?:/\*.*?\*/\s*)?'
    r'\4\s*->\s*\w+::~\w+\s*\(\s*\)\s*;\s*(?:/\*.*?\*/\s*)?'
    r'\w+::operator\s+delete\s*\(\s*\4\s*\)\s*;\s*(?:/\*.*?\*/\s*)?'
    r'return\s+\4\s*;\s*\}', re.S)


def itanium_class(name):
    """'_ZN11daKpaFire_cD0Ev' -> 'daKpaFire_c' (nested names joined by ::)."""
    if not name.startswith("_ZN"):
        return None
    s, parts = name[3:], []
    while s and s[0].isdigit():
        n = re.match(r"\d+", s).group(0)
        parts.append(s[len(n):len(n) + int(n)])
        s = s[len(n) + int(n):]
    if not parts or not re.match(r"(C[123]|D[012])E", s):
        return None
    return "::".join(parts)


def header_for(cls):
    leaf = cls.split("::")[-1]
    pat = re.compile(r"^\s*(?:class|struct)\s+%s\b[^;]*$" % re.escape(leaf), re.M)
    hits = []
    for h in sorted((REPO / "include").rglob("*.h")):
        try:
            if pat.search(h.read_text(encoding="utf-8", errors="ignore")):
                hits.append(h.relative_to(REPO / "include").as_posix())
        except OSError:
            pass
    pref = [h for h in hits if pathlib.PurePosixPath(h).stem == leaf]
    return (pref or hits or [None])[0], hits


def class_renames(rm):
    votes = collections.defaultdict(collections.Counter)
    for old, new in rm.renames.items():
        a, b = re.match(r"_ZN(\d+)(\w+)", old), re.match(r"_ZN(\d+)(\w+)", new)
        if not (a and b):
            continue
        oc = a.group(2)[:int(a.group(1))]
        nc = b.group(2)[:int(b.group(1))]
        if oc != nc:
            votes[oc][nc] += 1
    return {oc: c.most_common(1)[0][0] for oc, c in votes.items()}


def old_port_refs(ref):
    names = git("ls-tree", "--name-only", ref, "port/").split()
    blob = git("show", "%s:port/CMakeLists.txt" % ref)
    for n in names:
        if re.search(r"port/(slice_.*|.*_syms)\.txt$", n):
            blob += "\n" + git("show", "%s:%s" % (ref, n))
    return blob


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--old-ref", required=True)
    ap.add_argument("--before", required=True)
    ap.add_argument("--emit", action="store_true")
    ap.add_argument("--wire", action="store_true")
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--only-d0", action="store_true",
                    help="emit only the D0 faces (leave hand-carried REVIEW files alone)")
    args = ap.parse_args()

    before = pathlib.Path(args.before)
    rm = MR.Remap(before)
    rm.load_old_delinks(before, MR.module_labels())
    cmap = class_renames(rm)
    refs = old_port_refs(args.old_ref)
    hits = git("grep", "-l", "-E", r"_MSC_VER", args.old_ref, "--", "src").split()
    rows = []
    for h in hits:
        path = h.split(":", 1)[1]
        if path not in refs:
            continue
        text = git("show", "%s:%s" % (args.old_ref, path))
        if not ARM_RE.search(text):
            continue
        new, why = rm.resolve_path(path)
        resolved, ok = resolve_msc(text)
        if new and ";" not in new and (REPO / new).is_file():
            # still there only if every name the old arm defined is defined in
            # an arm of the new owner too (main adopted some arms, not all)
            ntext, _ = resolve_msc((REPO / new).read_text(encoding="utf-8", errors="ignore"))
            olddefs = set(DEF_RE.findall(resolved)) - set(DEF_RE.findall(
                resolve_msc_negated(text)))
            olddefs = {rm.renames.get(n, n) for n in olddefs}
            has_arm = bool(ARM_RE.search((REPO / new).read_text(encoding="utf-8", errors="ignore")))
            if (olddefs and olddefs <= set(DEF_RE.findall(ntext))) or (not olddefs and has_arm):
                continue  # the arm (or one like it) is still there
        stem = pathlib.PurePosixPath(path).stem
        m = D0_RE.search(resolved)
        kind, body, note = "REVIEW", None, ""
        if m:
            oldflat = m.group(2)
            newflat = rm.renames.get(oldflat, oldflat)
            cls = itanium_class(newflat)
            hdr, allh = header_for(cls) if cls else (None, [])
            if cls and hdr:
                kind = "D0"
                leaf = cls.split("::")[-1]
                body = (
                    '#include "%s"\n\n'
                    'extern "C" %s *%s(%s *thiz)\n{\n'
                    "    thiz->%s::~%s();    /* the D1 body, through MSVC's one destructor */\n"
                    "    %s::operator delete(thiz);  /* the class-specific delete D0 ends with */\n"
                    "    return thiz;\n}\n" % (hdr, cls, newflat, cls, cls, leaf, cls))
                note = "%s -> %s, class %s, header %s%s" % (
                    oldflat, newflat, cls, hdr,
                    "" if len(allh) < 2 else " (also declared in %s)" % ", ".join(allh[1:]))
            else:
                note = "D0 shape, but no header declares %s" % cls
        if body is None:
            t = re.sub(r"\A//cpp[^\n]*\n", "", resolved)
            for old, newn in sorted(rm.renames.items(), key=lambda kv: -len(kv[0])):
                if old in t:
                    t = re.sub(r"\b%s\b" % re.escape(old), newn, t)
            for oc, nc in sorted(cmap.items(), key=lambda kv: -len(kv[0])):
                t = re.sub(r"\b%s\b" % re.escape(oc), nc, t)
            body = t
            if not ok:
                note += " _MSC_VER conditional not resolved mechanically;"
            missing = [i for i in re.findall(r'#include\s+"([^"]+)"', t)
                       if not (REPO / "include" / i).is_file()]
            if missing:
                note += " missing headers: " + ", ".join(missing)
        rows.append(dict(old=path, stem=stem, new=new, kind=kind, note=note.strip(),
                         body=body))

    print("lost arms: %d  (D0 re-emitted %d, review %d)" % (
        len(rows), sum(r["kind"] == "D0" for r in rows),
        sum(r["kind"] != "D0" for r in rows)))
    if args.report:
        for r in rows:
            print("%-6s %s -> %s  %s" % (r["kind"], r["old"], r["new"], r["note"]))
    if args.emit:
        OUT.mkdir(parents=True, exist_ok=True)
        man = ["# port/unmatched/hostarm: _MSC_VER arms the 2026-10 main merge dropped",
               "# from src/, recovered by port/tools/hostarm_recover.py.",
               "# kind  old src file  ->  file main put that code in  |  note", ""]
        for r in rows:
            if r["kind"] != "D0" and (OUT / (r["stem"] + ".cpp")).is_file():
                # a REVIEW file someone carried by hand is never overwritten
                man.append("%-6s %s -> %s | %s" % (r["kind"], r["old"], r["new"], r["note"]))
                continue
            if args.only_d0 and r["kind"] != "D0":
                man.append("%-6s %s -> %s | %s" % (r["kind"], r["old"], r["new"], r["note"]))
                continue
            banner = (
                "/* HOST ARM recovered by port/tools/hostarm_recover.py.\n"
                " * The port line carried this in %s under #ifdef _MSC_VER; the\n"
                " * 2026-10 main merge (749f5799a) took main's src/, where that code\n"
                " * now lives in %s without the arm. mwccarm never saw the arm,\n"
                " * so nothing here moves a ROM byte.%s */\n" % (
                    r["old"], r["new"],
                    "" if r["kind"] == "D0" else
                    "\n * REVIEW: the old file's text with the _MSC_VER arm kept and\n"
                    " * renamed symbols/classes substituted; not yet compiled\n"
                    " * against main's headers."))
            (OUT / (r["stem"] + ".cpp")).write_text(banner + r["body"].rstrip() + "\n",
                                                   encoding="utf-8")
            man.append("%-6s %s -> %s | %s" % (r["kind"], r["old"], r["new"], r["note"]))
        (OUT / "MANIFEST.txt").write_text("\n".join(man) + "\n", encoding="utf-8")
    if args.wire:
        wire(rows, args.old_ref)
    return 0


def wire(rows, ref):
    """Add each recovered file to every slice list that named its old source."""
    by_old = {r["old"]: r for r in rows}
    n = 0
    for p in sorted((REPO / "port").glob("slice_*.txt")):
        rel = p.relative_to(REPO).as_posix()
        try:
            old = git("show", "%s:%s" % (ref, rel))
        except subprocess.CalledProcessError:
            continue
        olds = [l.strip() for l in old.splitlines()
                if l.strip() in by_old]
        if not olds:
            continue
        text = p.read_bytes().decode("utf-8", errors="surrogateescape")
        eol = "\r\n" if "\r\n" in text else "\n"
        lines = text.split(eol)
        for o in olds:
            add = "port/unmatched/hostarm/%s.cpp" % by_old[o]["stem"]
            if not (REPO / add).is_file():
                continue          # a REVIEW arm not carried yet
            if add in (l.strip() for l in lines):
                continue
            # after the row that names the old source (now a FOLDED comment)
            # or, failing that, the row its new owner took
            idx = next((i for i, l in enumerate(lines)
                        if l.strip().endswith(": " + o) or l.strip() == o), None)
            if idx is None:
                idx = next((i for i, l in enumerate(lines)
                            if l.strip() == by_old[o]["new"]), len(lines) - 1)
            lines.insert(idx + 1, add)
            n += 1
        p.write_bytes(eol.join(lines).encode("utf-8", errors="surrogateescape"))
    print("wired %d slice row(s)" % n)


if __name__ == "__main__":
    sys.exit(main())
