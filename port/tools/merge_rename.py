#!/usr/bin/env python3
"""merge_rename -- carry decomp SYMBOL and CLASS renames into port/, by address.

    python port/tools/merge_rename.py --before <old config/> --old-ref <rev> --report
    python port/tools/merge_rename.py --before <old config/> --old-ref <rev> --apply

The companion of merge_remap.py (which carries src/ PATHS). A main merge renames
symbols -- `func_ov100_02146280` becomes `_ZN8daFish_c19func_ov100_02146280Ev`,
`_ZN10BowserFireD0Ev` becomes `_ZN11daKpaFire_cD0Ev` -- and port/ spells those
names by hand in /alternatename pragmas, extern "C" declarations, hand-built
vtables, face ledgers and slice/sym lists. The rename is keyed on (module, addr):
an old name's address in the PRE-merge config/**/symbols.txt is looked up in the
current config, never matched by name (sm64ds-port-clinkage-flip, family 1).

What is rewritten, outside comments:
  * an identifier that IS an old symbol name -> the new name;
  * the C-decorated spelling `_<old>` (alternatename right-hand sides, and
    `__ZN..` for an Itanium name) -> `_<new>` / `__<new>`, when the
    undecorated name is a renamed symbol and the decorated one is not itself a
    symbol;
  * MSVC decorations: `?<old>@@...` (a C++-linkage free name), and a renamed
    CLASS's components `@<OldCls>@`, `??0<OldCls>@`, `??1<OldCls>@`,
    `??_7<OldCls>@` ... -> the new class;
  * an Itanium-mangled token that is not itself a renamed symbol but embeds a
    renamed class as `<len><OldCls>` -> `<len'><NewCls>`;
  * `#include "<OldCls>.h"` for a header main DELETED -> the header that now
    declares the new class, and in that same file the bare class identifier
    OldCls -> NewCls (only there: `Key`, `View`, `Stage` are too common to
    rename anywhere a header does not prove what they mean).
The class map is voted from the symbol renames themselves (an old member
`_ZN10BowserFire...` renamed to `_ZN11daKpaFire_c...`).

Files: port/hal, port/unmatched, port/tests, port/ntr, port/mods_api, the
slice/sym/faces/ledger *.txt lists at port/ top level, port/CMakeLists.txt, and
the patch tables of port/tools/hostgen.py. Other port/tools/ baselines are
left as they are (they are measurements, re-taken on the PC).
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
import hostarm_recover as HA  # noqa: E402

CODE_EXT = {".c", ".cpp", ".h", ".hpp", ".inc"}
IDENT = re.compile(r"[A-Za-z_$][A-Za-z0-9_$]*")


def comment_mask(text, kind):
    """[(start, end)] spans that are comments."""
    spans = []
    if kind == "c":
        i, n = 0, len(text)
        while i < n:
            c = text[i]
            if text.startswith("//", i):
                j = text.find("\n", i)
                j = n if j < 0 else j
                spans.append((i, j))
                i = j
            elif text.startswith("/*", i):
                j = text.find("*/", i + 2)
                j = n if j < 0 else j + 2
                spans.append((i, j))
                i = j
            elif c in "\"'":
                j = i + 1
                while j < n and text[j] != c and text[j] != "\n":
                    j += 2 if text[j] == "\\" else 1
                i = j + 1
            else:
                i += 1
    else:  # '#' comments (txt lists, CMake)
        for m in re.finditer(r"#[^\n]*", text):
            # a '#' inside a quoted CMake string is not a comment; good enough
            # for these files, whose code lines carry no '#'
            spans.append((m.start(), m.end()))
    return spans


class Renamer:
    def __init__(self, before, old_ref):
        self.rm = MR.Remap(before)
        # A new name that is not a C identifier (`@1095`, mwcc's numbered
        # statics; `__sinit_d_s_mg_base.cpp`, a TU's static initializer) cannot
        # be spelled in port code: keep the old spelling and list it.
        ident = re.compile(r"^[A-Za-z_$][A-Za-z0-9_$]*$")
        self.ren = {o: n for o, n in self.rm.renames.items() if ident.match(n)}
        self.unspellable = sorted(o for o, n in self.rm.renames.items()
                                  if not ident.match(n))
        self.known = set(self.rm.old_by_name) | set(self.rm.new_by_name)
        self.cmap = HA.class_renames(self.rm)
        # never rename a class whose old header still exists: the name is live
        self.cmap = {o: n for o, n in self.cmap.items()
                     if not (REPO / "include" / (o + ".h")).is_file()}
        self.old_ref = old_ref
        self.deleted_headers = set()
        if old_ref:
            old = subprocess.run(["git", "-C", str(REPO), "ls-tree", "-r", "--name-only",
                                  old_ref, "include"], capture_output=True,
                                 text=True).stdout.split()
            for p in old:
                rel = p[len("include/"):]
                if not (REPO / p).is_file():
                    self.deleted_headers.add(rel)
        self.hdr_cache = {}
        self.stats = collections.Counter()
        self.unmapped_includes = collections.Counter()

    def new_header(self, old_rel):
        stem = pathlib.PurePosixPath(old_rel).stem
        cls = self.cmap.get(stem)
        if not cls:
            return None, None
        if cls not in self.hdr_cache:
            self.hdr_cache[cls] = HA.header_for(cls)[0]
        return self.hdr_cache[cls], (stem, cls)

    def map_token(self, tok):
        if tok in self.ren:
            return self.ren[tok]
        # the C-decorated spelling: _func_X, and __ZN.. for an Itanium name
        if tok.startswith("_") and tok not in self.known and tok[1:] in self.ren:
            return "_" + self.ren[tok[1:]]
        # a token-pasted face name: `tr_##sym` spelled out as tr_<old>
        if tok not in self.known:
            for i, ch in enumerate(tok[:-1]):
                if ch == "_" and i > 0 and tok[i + 1:] in self.ren:
                    return tok[:i + 1] + self.ren[tok[i + 1:]]
        if tok.startswith("_Z") or tok.startswith("__Z"):
            new = self.itanium_class_sub(tok)
            if new != tok:
                return new
        return None

    def itanium_class_sub(self, tok):
        def rep(m):
            n, rest = int(m.group(1)), m.group(2)
            name = rest[:n]
            if len(name) == n and name in self.cmap:
                nn = self.cmap[name]
                return "%d%s" % (len(nn), nn) + rest[n:]
            return m.group(0)
        # walk every <len><name> component
        out, i = [], 0
        for m in re.finditer(r"(?<![0-9])([1-9][0-9]*)([A-Za-z_][A-Za-z0-9_]*)", tok):
            out.append(tok[i:m.start()])
            out.append(rep(m))
            i = m.end()
        out.append(tok[i:])
        return "".join(out)

    def msvc_sub(self, text):
        def free(m):
            name = m.group(1)
            if name in self.ren:
                self.stats["msvc-free"] += 1
                return "?" + self.ren[name] + "@@"
            return m.group(0)
        text = re.sub(r"(?<![?\w])\?([A-Za-z_$][\w$]*)@@", free, text)
        if self.cmap:
            alt = "|".join(sorted(map(re.escape, self.cmap), key=len, reverse=True))
            def cls(m):
                self.stats["msvc-class"] += 1
                return m.group(1) + self.cmap[m.group(2)] + "@"
            text = re.sub(r"(@|\?\?(?:_[0-9A-Z]|[0-9]))(%s)@" % alt, cls, text)
        return text

    def rewrite(self, text, kind, classes_here=()):
        mask = comment_mask(text, kind)
        # rewrite only the code between comment spans
        out, pos = [], 0
        for a, b in mask + [(len(text), len(text))]:
            out.append(self.rewrite_code(text[pos:a], classes_here))
            out.append(text[a:b])
            pos = b
        return "".join(out)

    def rewrite_code(self, code, classes_here):
        code = self.msvc_sub(code)

        def tok(m):
            t = m.group(0)
            # part of a file path (src/<stem>.c, host-src/...): a path names a
            # file that exists or not, and merge_remap.py owns those
            a, b = m.start(), m.end()
            if (a > 0 and code[a - 1] in "/\\") or \
                    re.match(r"\.(c|cpp|h|inc|txt|s)\b", code[b:b + 5]):
                return t
            new = self.map_token(t)
            if new is not None and new != t:
                self.stats["symbol"] += 1
                return new
            if t in classes_here:
                self.stats["class-ident"] += 1
                return self.cmap[t]
            return t
        return IDENT.sub(tok, code)

    def rewrite_includes(self, text):
        classes = set()

        def inc(m):
            rel = m.group(2)
            if rel not in self.deleted_headers:
                return m.group(0)
            hdr, pair = self.new_header(rel)
            if not hdr:
                self.unmapped_includes[rel] += 1
                return m.group(0)
            classes.add(pair[0])
            self.stats["include"] += 1
            return '%s"%s"' % (m.group(1), hdr)
        text = re.sub(r'(#\s*include\s*)"([^"]+)"', inc, text)
        return text, classes


def files():
    port = REPO / "port"
    for sub in ("hal", "unmatched", "tests", "ntr", "mods_api"):
        for p in sorted((port / sub).rglob("*")):
            if p.is_file() and p.suffix in CODE_EXT:
                yield p, "c"
    for p in sorted(port.glob("*.txt")):
        yield p, "hash"
    yield port / "CMakeLists.txt", "hash"
    yield port / "src_folds.cmake", "hash"
    yield port / "tools" / "hostgen.py", "py"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--before", required=True)
    ap.add_argument("--old-ref")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()
    rn = Renamer(pathlib.Path(args.before), args.old_ref)
    changed = {}
    for p, kind in files():
        raw = p.read_bytes()
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            text = raw.decode("latin-1")
        new = text
        classes = set()
        if kind == "c":
            new, classes = rn.rewrite_includes(new)
        if kind == "py":
            # only string literals carry symbol names in hostgen's tables; the
            # identifiers are Python. Rewrite inside quotes only.
            new = re.sub(r'"(?:[^"\\\n]|\\.)*"', lambda m: rn.rewrite_code(m.group(0), ()), new)
        else:
            new = rn.rewrite(new, "c" if kind == "c" else "hash", classes)
        if new != text:
            changed[p] = new
            if args.apply:
                try:
                    p.write_bytes(new.encode("utf-8"))
                except UnicodeEncodeError:
                    p.write_bytes(new.encode("latin-1"))
    print("renames by address: %d, class renames: %d" % (len(rn.ren), len(rn.cmap)))
    print("files changed: %d" % len(changed))
    print("renames not spelled (new name is not an identifier): %d" % len(rn.unspellable))
    print("edits:", dict(rn.stats))
    if rn.unmapped_includes:
        print("deleted headers with no new home:", dict(rn.unmapped_includes))
    if args.report:
        for p in sorted(changed):
            print("  ", p.relative_to(REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main())
