#!/usr/bin/env python3
"""merge_remap -- carry port/ references across a decomp-main merge, BY ADDRESS.

    python port/tools/merge_remap.py --before <old-config-root> --report
    python port/tools/merge_remap.py --before <old-config-root> --apply

A main merge does three things to the names port/ hard-codes:
  * renames a symbol (config/**/symbols.txt row at the same (module, addr)
    now carries a different name),
  * folds one-function files into a production TU (src/X.c disappears, the
    delinks.txt range that held X now belongs to src/<dir>/<Tu>.cpp),
  * moves a file into a subdirectory.

The only key that survives all three is (module, addr). So this tool reads
the PRE-merge symbols.txt tree (--before, the config/ directory of the old
commit, laid out exactly like config/), and for every stale `src/...` path or
renamed symbol in port/ it answers:

    old path src/<sym>.<ext>  ->  sym  ->  (module, addr) in --before
                              ->  srcpath.owner_at(module, addr)   (by range)
                              ->  else srcpath.path_for(new name at that addr)

Renaming by NAME alone is wrong (sm64ds-port-clinkage-flip, family 1): a
class-shift fix leaves the old name valid for a DIFFERENT function, so a
name-only check passes while the slice now compiles another class's code.

What --apply rewrites:
  * slice_*.txt / *_syms.txt / CMakeLists.txt: non-comment rows only. A row
    whose path is stale is replaced by its owner TU; when several rows of one
    list resolve to the same TU, the first keeps it and the rest are commented
    out with a `# FOLDED into <tu>:` marker so the history stays readable.
    CMake de-duplicates one source named twice in a target anyway; the marker
    is for the reader.
  * renamed symbols (old name present in port/ text, its (module, addr) now
    carries a different name): replaced in slice/syms lists, CMake *_SYMS
    lists, hal/*.cpp and hal/*.inc, as whole identifiers and inside mangled
    spellings only where the whole old token appears.
Comments are left alone: they describe history and a rewritten comment is
noise in review. Anything it cannot resolve is listed, never guessed.
"""

import argparse
import collections
import os
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent
sys.path.insert(0, str(REPO / "tools"))
import srcpath as SP  # noqa: E402
import relocs  # noqa: E402

SYM_RE = re.compile(r"^(\S+)\s+kind:(function|data)\(([^)]*)\)\s+addr:0x([0-9a-fA-F]+)")
# `src/X.c` bare, or behind `../`, `}/` (${PORT_REPO_ROOT_ABS}/src/...) and
# `host-src/` (a hostgen OUTPUT, whose name is the source's own path with
# .cpp; see rewrite_lists).
PATH_RE = re.compile(
    r"(?:(?<![A-Za-z0-9_/.-])|(?<=\.\./)|(?<=\}/)|(?<=host-src/))"
    r"(src/[A-Za-z0-9_./-]+\.(?:c|cpp))\b")


def load_symbols(config_root, labels):
    """name -> [(label, addr, kind)] and (label, addr, kind) -> name."""
    by_name = collections.defaultdict(list)
    by_key = {}
    for rel, label in labels.items():
        p = config_root / rel
        if not p.is_file():
            continue
        for line in p.read_text(encoding="utf-8", errors="ignore").splitlines():
            m = SYM_RE.match(line.strip())
            if not m:
                continue
            name, kind, addr = m.group(1), m.group(2), int(m.group(4), 16)
            by_name[name].append((label, addr, kind))
            by_key.setdefault((label, addr, kind), name)
    return by_name, by_key


def module_labels():
    out = {}
    for path, label in relocs.module_universe(repo=REPO):
        out[path.relative_to(REPO / "config").as_posix()] = label
    return out


class Remap:
    def __init__(self, before_root):
        labels = module_labels()
        self.old_by_name, self.old_by_key = load_symbols(before_root, labels)
        self.new_by_name, self.new_by_key = load_symbols(REPO / "config", labels)
        self.renames = {}       # old name -> new name (unique, by address)
        self.ambiguous = {}
        # Several names can share one address (`data_02091528` and
        # `_ZTV9dScBoot_c`): keep every new name per key and pick the one that
        # corresponds to the old spelling, never just the first listed.
        self.new_names_by_key = collections.defaultdict(list)
        for name, keys in self.new_by_name.items():
            for k in keys:
                self.new_names_by_key[k].append(name)
        generic = re.compile(r"^(data|func)_(ov\d+_)?[0-9a-f]{6,8}$")

        def pick(old, cands):
            if not cands:
                return None
            if old in cands:
                return old
            if len(cands) == 1:
                return cands[0]
            same = [c for c in cands if c.startswith("_Z") == old.startswith("_Z")
                    and bool(generic.match(c)) == bool(generic.match(old))]
            if len(same) == 1:
                return same[0]
            named = [c for c in cands if not generic.match(c)]
            if len(named) == 1:
                return named[0]
            return sorted(cands)[0]
        self.pick = pick
        for old, keys in self.old_by_name.items():
            news = {pick(old, self.new_names_by_key.get(k, [])) for k in keys}
            news.discard(None)
            if old in self.new_by_name and old not in news:
                # old name still exists in the new tree but at another address:
                # the class-shift landmine.  Record it; never rewrite by name.
                pass
            if len(news) == 1:
                new = news.pop()
                if new != old:
                    self.renames[old] = new
            elif len(news) > 1:
                self.ambiguous[old] = sorted(news)

    def keys_for_old(self, name):
        return self.old_by_name.get(name, [])

    def owner_for_symbol(self, name):
        """(new path or None, why)."""
        keys = [k for k in self.keys_for_old(name) if k[2] == "function"]
        if not keys and name in self.new_by_name:
            keys = [k for k in self.new_by_name[name] if k[2] == "function"]
        owners = set()
        for label, addr, _kind in keys:
            p = SP.owner_at(label, addr)
            if p is None:
                new = self.new_by_key.get((label, addr, "function"))
                if new:
                    q = SP.path_for(new)
                    if q is not None:
                        p = q
            if p is not None:
                owners.add(pathlib.Path(p).resolve().relative_to(REPO).as_posix())
        if len(owners) == 1:
            return owners.pop(), "addr"
        if len(owners) > 1:
            return None, "several owners: " + ", ".join(sorted(owners))
        if not keys:
            return None, "not a symbol in the old or new config"
        return None, "no src/ owner at %s" % ", ".join(
            "%s:0x%08x" % (k[0], k[1]) for k in keys)

    def load_old_delinks(self, before_root, labels):
        """old src path -> [(label, start, end)] from the PRE-merge delinks.txt."""
        self.old_ranges = collections.defaultdict(list)
        for rel, label in labels.items():
            d = before_root / pathlib.PurePosixPath(rel).parent / "delinks.txt"
            if not d.is_file():
                continue
            entry = None
            for line in d.read_text(encoding="utf-8", errors="ignore").splitlines():
                if not line.strip():
                    continue
                if not line[0].isspace():
                    entry = line.strip().rstrip(":")
                    continue
                m = re.search(r"^\s*\.text\s+start:0x([0-9a-fA-F]+)\s+end:0x([0-9a-fA-F]+)", line)
                if m and entry and entry.startswith("src/"):
                    self.old_ranges[entry].append(
                        (label, int(m.group(1), 16), int(m.group(2), 16)))

    def owners_by_old_range(self, path):
        """Every new owner of the old file's .text, from the old function rows."""
        owners = []
        for label, start, end in self.old_ranges.get(path, ()):
            for (lab, addr, kind), _name in self.old_by_key.items():
                if lab != label or kind != "function" or not (start <= addr < end):
                    continue
                p = SP.owner_at(label, addr)
                if p is None:
                    new = self.new_by_key.get((label, addr, "function"))
                    p = SP.path_for(new) if new else None
                if p is None:
                    owners.append((addr, None, "%s:0x%08x" % (label, addr)))
                else:
                    rel = pathlib.Path(p).resolve().relative_to(REPO).as_posix()
                    owners.append((addr, rel, None))
        return sorted(owners, key=lambda o: o[0])

    def resolve_path(self, path):
        """(new path or None, why).  A multi-owner answer is "a;b;c"."""
        stem = pathlib.PurePosixPath(path).stem
        if (REPO / path).is_file():
            # A leftover one-function file beside the TU that now OWNS its
            # symbol is not what the ROM build compiles, and compiling both
            # is a duplicate definition. Follow the enrolment.
            enrolled = SP.enrolment_index().get(stem)
            if enrolled and enrolled != path and \
                    path not in SP.source_ownership_index():
                return enrolled, "leftover"
            return path, "exists"
        new, why = self.owner_for_symbol(stem)
        if new is not None:
            return new, why
        if getattr(self, "old_ranges", None) and path in self.old_ranges:
            owners = self.owners_by_old_range(path)
            lost = [o[2] for o in owners if o[1] is None]
            paths = []
            for _a, rel, _l in owners:
                if rel and rel not in paths:
                    paths.append(rel)
            if lost:
                return None, "old range has functions with no src/ owner now: " + \
                    ", ".join(lost[:6]) + (" ..." if len(lost) > 6 else "") + \
                    (" (others -> %s)" % ";".join(paths) if paths else "")
            if paths:
                return ";".join(paths), "old-range"
        return None, why


def port_files():
    port = REPO / "port"
    for p in sorted(port.glob("slice_*.txt")):
        yield p, "list"
    for p in sorted(port.glob("*_syms.txt")):
        yield p, "list"
    yield port / "CMakeLists.txt", "cmake"


def rewrite_lists(rm, apply, extra_map):
    """Stale path rows -> owner TU.  Returns (changed files, unresolved rows)."""
    unresolved = []
    changed = {}
    for p, kind in port_files():
        text = p.read_bytes().decode("utf-8", errors="surrogateescape")
        lines = text.splitlines(keepends=True)
        seen_in_file = {}
        out = []
        dirty = False
        for i, line in enumerate(lines, 1):
            body = line.rstrip("\r\n")
            eol = line[len(body):]
            stripped = body.strip()
            if not stripped or stripped.startswith("#"):
                out.append(line)
                continue
            code = body.split("#", 1)[0] if kind == "list" else body
            paths = PATH_RE.findall(code)
            if not paths:
                out.append(line)
                continue
            new_body = body
            fold_note = None
            for old in paths:
                if (REPO / old).is_file() and rm.resolve_path(old)[1] == "exists":
                    if kind == "list":
                        seen_in_file.setdefault(old, i)
                    continue
                new = extra_map.get(old)
                why = "manual"
                if new is None:
                    new, why = rm.resolve_path(old)
                if new is None:
                    unresolved.append((p.relative_to(REPO).as_posix(), i, old, why))
                    continue
                if ";" in new and kind == "list" and stripped == old:
                    rows = []
                    for n in new.split(";"):
                        if n in seen_in_file:
                            rows.append("# FOLDED into %s (row %d): %s" % (
                                n, seen_in_file[n], old))
                        else:
                            seen_in_file[n] = i
                            rows.append(n)
                    new_body = (eol or "\n").join(rows)
                    continue
                if ";" in new:
                    new_body = new_body.replace(old, " ".join(new.split(";")))
                    continue
                # hostgen writes host-src/<source path>.cpp whatever the
                # source's extension, so an OUTPUT name follows the new path
                # with .cpp.
                hs_old = "host-src/" + old[:old.rfind(".")] + ".cpp"
                hs_new = "host-src/" + new[:new.rfind(".")] + ".cpp"
                new_body = new_body.replace(hs_old, hs_new)
                new_body = new_body.replace(old, new)
                if kind == "list" and stripped == old:
                    if new in seen_in_file:
                        fold_note = "# FOLDED into %s (row %d): %s" % (
                            new, seen_in_file[new], old)
                    else:
                        seen_in_file[new] = i
            if fold_note:
                new_body = fold_note
            if new_body != body:
                dirty = True
            out.append(new_body + eol)
        if dirty:
            changed[p] = "".join(out)
    if apply:
        for p, t in changed.items():
            p.write_bytes(t.encode("utf-8", errors="surrogateescape"))
    return changed, unresolved


IDENT_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_$@?]*")


def rename_scan(rm):
    """Every renamed old name that appears as an identifier in port/ code."""
    hits = collections.defaultdict(list)
    port = REPO / "port"
    files = [p for p in port.rglob("*")
             if p.is_file() and p.suffix in (".cpp", ".c", ".h", ".inc", ".txt")
             and "tools" not in p.relative_to(port).parts
             and p.name != "CMakeLists.txt"]
    files.append(port / "CMakeLists.txt")
    for p in files:
        try:
            text = p.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for ln, line in enumerate(text.splitlines(), 1):
            code = line
            for tok in set(IDENT_RE.findall(code)):
                if tok in rm.renames:
                    hits[tok].append((p.relative_to(REPO).as_posix(), ln))
    return hits


def cmake_folds(rm, out_path):
    """Write port/src_folds.cmake: every bare symbol a CMake hostgen list names
    whose one-function source main moved (PORT_SYM_MOVED_*) or folded into a
    multi-function TU (PORT_SYM_FOLDED_*). A name may be an OLD name (resolved by
    address) or one merge_rename.py already carried to the NEW name."""
    cm = (REPO / "port" / "CMakeLists.txt").read_text(encoding="utf-8")
    code = "\n".join("" if l.strip().startswith("#") else l.split("#", 1)[0]
                     for l in cm.split("\n"))
    names = set()
    for m in re.finditer(r"(?:set|list\(APPEND)\(?\s*(\w+_SYMS?)\s+([^)]*)\)", code):
        for t in m.group(2).split():
            if not t.startswith(("$", '"')) and "/" not in t and "." not in t:
                names.add(t)
    own = SP.source_ownership_index()
    enr = SP.enrolment_index()
    moved, folded, unres = {}, {}, []
    for s in sorted(names):
        flat = [e for e in (".c", ".cpp") if (REPO / ("src/%s%s" % (s, e))).is_file()]
        if flat and enr.get(s) in (None, "src/%s%s" % (s, flat[0])):
            continue
        new = enr.get(s)
        if new is None:
            new, _why = rm.resolve_path("src/%s.c" % s)
        if new is None or ";" in new:
            unres.append(s)
            continue
        if len(own.get(new, [])) > 1 or pathlib.PurePosixPath(new).stem != s:
            folded[s] = new
        else:
            moved[s] = new
    xd = []
    for m in re.finditer(r"foreach\(sym IN LISTS (\w+)\)(.*?)endforeach\(\)", code, re.S):
        if "--extern-data" in m.group(2):
            lm = re.search(r"set\(%s\s+([^)]*)\)" % m.group(1), code)
            if lm:
                xd += [folded[t] for t in lm.group(1).split() if t in folded]
    lines = ["# GENERATED by port/tools/merge_remap.py --cmake-folds. Do not hand-edit;",
             "# re-run it after the next main merge.",
             "#",
             "# Symbols the port names one by one in a CMake hostgen list, whose",
             "# one-function source main moved (PORT_SYM_MOVED_*: same body, new path) or",
             "# folded into a production TU that also owns other functions",
             "# (PORT_SYM_FOLDED_*). Resolved by (module, addr), never by name; consumed by",
             "# port_hostgen_sym() in port/CMakeLists.txt.", ""]
    lines += ["set(PORT_SYM_MOVED_%s %s)" % kv for kv in sorted(moved.items())]
    lines.append("")
    lines += ["set(PORT_SYM_FOLDED_%s %s)" % kv for kv in sorted(folded.items())]
    lines.append("set(PORT_SYM_FOLDED_TUS")
    lines += ["    " + p for p in sorted(set(folded.values()))]
    lines.append(")")
    # Production TUs a PATH-based hostgen block substitutes (the *_HOSTGEN_SRCS
    # and *_HOSTGEN_TU lists, slice_gate29's rows, `src/X hostgen` rows): before
    # the merge each named one function and no other slice named the same file;
    # now other slices name the same TU raw, so it has to be substituted for
    # every target or the TU is compiled twice (raw and generated).
    multi = set()
    for m in re.finditer(r"set\((\w+_HOSTGEN_(?:SRCS|TU))\b(.*?)\)", code, re.S):
        if m.group(1) == "PORT_HOSTGEN_TU":
            continue
        multi |= set(re.findall(r"(src/\S+?\.(?:cpp|c))\b", m.group(2)))
    for sl in REPO.joinpath("port").glob("slice_*.txt"):
        t = sl.read_text(encoding="utf-8", errors="ignore")
        if sl.name == "slice_gate29.txt":
            multi |= set(re.findall(r"^(src/\S+?\.(?:cpp|c))\s*$", t, re.M))
        multi |= set(re.findall(r"^(src/\S+?\.(?:cpp|c))\s+hostgen\s*$", t, re.M))
    multi = sorted(p for p in multi if len(own.get(p, [])) > 1 and (REPO / p).is_file())
    lines += ["",
              "# Production TUs a path-based hostgen block substitutes; every target takes",
              "# the one generated copy (see port_hostgen_sym in port/CMakeLists.txt).",
              "set(PORT_HOSTGEN_SHARED_TUS"]
    lines += ["    " + p for p in multi]
    lines.append(")")
    lines += ["",
              "# Folded TUs a per-symbol `--extern-data` hostgen list reached: the old row",
              "# needed its engine BSS to stay the HAL's, so the whole-TU rule passes",
              "# --extern-data too. A TU that was already on PORT_HOSTGEN_TU (plain) before",
              "# the merge keeps that ruling and is left out.",
              "set(PORT_HOSTGEN_TU_EXTERN_DATA"]
    pre = set(re.findall(r"(src/\S+?\.(?:cpp|c))\b",
                         re.search(r"\nset\(PORT_HOSTGEN_TU\b(.*?)\)", code, re.S).group(1)))
    lines += ["    " + p for p in sorted(set(xd)) if p not in pre]
    lines.append(")")
    pathlib.Path(out_path).write_text("\n".join(lines) + "\n", encoding="utf-8")
    return moved, folded, unres


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--before", required=True,
                    help="the PRE-merge config/ directory (same layout as config/)")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--renames-out")
    ap.add_argument("--cmake-folds", metavar="OUT",
                    help="write the hostgen fold map (port/src_folds.cmake)")
    args = ap.parse_args()
    rm = Remap(pathlib.Path(args.before))
    rm.load_old_delinks(pathlib.Path(args.before), module_labels())
    print("renames by address: %d (ambiguous %d)" % (len(rm.renames), len(rm.ambiguous)))
    if args.renames_out:
        with open(args.renames_out, "w") as f:
            for o, n in sorted(rm.renames.items()):
                keys = rm.old_by_name[o]
                f.write("%s %s %s\n" % (o, n, " ".join(
                    "%s:0x%08x" % (k[0], k[1]) for k in keys)))
    if args.cmake_folds:
        moved, folded, unres = cmake_folds(rm, args.cmake_folds)
        print("cmake folds: moved %d, folded %d, unresolved %s" % (
            len(moved), len(folded), unres))
        return 0
    changed, unresolved = rewrite_lists(rm, args.apply, {})
    print("files with stale path rows rewritten: %d" % len(changed))
    print("unresolved stale rows: %d" % len(unresolved))
    for f, ln, old, why in unresolved:
        print("  UNRESOLVED %s:%d %s -- %s" % (f, ln, old, why))
    return 0


if __name__ == "__main__":
    sys.exit(main())
