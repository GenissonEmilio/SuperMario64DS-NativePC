#!/usr/bin/env python3
"""tu_props_audit -- per-source CMake properties after a TU fold.

    python port/tools/tu_props_audit.py [--json OUT]

Before main's TU folds, a set_source_files_properties() call in
port/CMakeLists.txt named ONE function's file, so a COMPILE_DEFINITIONS
rename like `VT=data_ov006_0213bbb4` or `G0=port_coffin_file0` meant that one
function's literal pool word. Once the file is a production TU that owns
twenty functions, the same call:

  * applies the macro to every function in the TU, and
  * REPLACES (does not append to) whatever an earlier call set on the same
    path, so the last of several per-function calls silently wins.

That is the G0/G1 landmine of port/docs (sm64ds-port-clinkage-flip, family
3) in a new shape: a placeholder renamed for the wrong body, and the linker
does not say so.

For every call that names a src/ file owning more than one function, this
reports each macro's LEFT-HAND token and where it still occurs in the TU
(outside comments), attributed to the enclosing `// @symbol` block:

  INERT     the token does not occur in the TU at all: main's fold spelled the
            body without the placeholder, the macro does nothing.
  ONE       the token occurs only inside the body the old per-function row
            named (by symbol), so the macro still means what it meant.
  WIDE      the token occurs in other bodies too: the macro now rewrites code
            it was never measured against.  Needs a decision.
  CLOBBER   two calls set the same property on one path; only the later one
            takes effect.  Needs merging.
COMPILE_OPTIONS rows are listed with the TU's function count, since an
option such as /Zp4 now governs every body's struct layout.
No compiler, no build.
"""

import argparse
import json
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent
sys.path.insert(0, str(REPO / "tools"))
import srcpath as SP  # noqa: E402

RAW = "${CMAKE_CURRENT_SOURCE_DIR}/../"
GEN = "${CMAKE_BINARY_DIR}/host-src/"


def strip_comments(text):
    """Blank comments out, keeping every offset (blocks() reads the raw text)."""
    blank = lambda m: re.sub(r"[^\n]", " ", m.group(0))
    text = re.sub(r"/\*.*?\*/", blank, text, flags=re.S)
    return re.sub(r"//[^\n]*", blank, text)


def calls(cmake):
    out = []
    for m in re.finditer(r"set_source_files_properties\((.*?)\)\s*\n", cmake, re.S):
        bol = cmake.rfind("\n", 0, m.start()) + 1
        if cmake[bol:m.start()].strip().startswith("#"):
            continue
        code = "\n".join(l.split("#", 1)[0] for l in m.group(1).split("\n"))
        if "PROPERTIES" not in code:
            continue
        head, props = code.split("PROPERTIES", 1)
        files = re.findall(r'"([^"]+)"', head)
        toks = re.findall(r'"[^"]*"|\S+', props)
        pd = {}
        k = None
        for t in toks:
            if re.fullmatch(r"[A-Z_]+", t):
                k = t
                pd[k] = []
            elif k:
                pd[k].append(t.strip('"'))
        line = cmake[:m.start()].count("\n") + 1
        out.append((line, files, pd))
    return out


def rel_of(f):
    if f.startswith(RAW):
        return f[len(RAW):], "raw"
    if f.startswith(GEN):
        rel = f[len(GEN):]
        for ext in (".cpp", ".c"):
            cand = rel[:-4] + ext
            if (REPO / cand).is_file():
                return cand, "gen"
        return rel, "gen"
    return None, None


def blocks(text):
    """[(symbol, start, end)] from main's `// @symbol` markers."""
    marks = [(m.start(), m.group(1)) for m in
             re.finditer(r"//\s*@symbol\s+(\S+)", text)]
    out = []
    for i, (pos, sym) in enumerate(marks):
        end = marks[i + 1][0] if i + 1 < len(marks) else len(text)
        out.append((sym, pos, end))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json")
    ap.add_argument("--old-cmake", help="pre-merge CMakeLists.txt, to name "
                    "the function each row used to mean")
    args = ap.parse_args()
    cmake = (REPO / "port" / "CMakeLists.txt").read_text(encoding="utf-8")
    own = SP.source_ownership_index()
    old_calls = None
    if args.old_cmake:
        old_calls = calls(pathlib.Path(args.old_cmake).read_text(encoding="utf-8"))
    rows = []
    seen = {}
    new_calls = calls(cmake)
    align = {}
    if old_calls:
        import difflib
        key = lambda c: repr(sorted(c[2].items()))
        sm = difflib.SequenceMatcher(None, [key(c) for c in old_calls],
                                     [key(c) for c in new_calls], autojunk=False)
        for a, b, n in sm.get_matching_blocks():
            for k in range(n):
                align[b + k] = a + k
    for ci, (line, files, pd) in enumerate(new_calls):
        for fi, f in enumerate(files):
            rel, kind = rel_of(f)
            if rel is None or len(own.get(rel, [])) < 2:
                continue
            meant = None
            if ci in align:
                of = old_calls[align[ci]][1]
                if fi < len(of):
                    meant = pathlib.PurePosixPath(of[fi]).stem
            for prop, vals in pd.items():
                if prop == "LANGUAGE":
                    continue
                key = (rel, kind, prop)
                if key in seen and seen[key][0] != line:
                    rows.append(dict(tu=rel, kind=kind, line=line, prop=prop,
                                     verdict="CLOBBER", detail="also set at line %d"
                                     % seen[key][0], meant=meant))
                seen[key] = (line, vals)
                if prop != "COMPILE_DEFINITIONS":
                    rows.append(dict(tu=rel, kind=kind, line=line, prop=prop,
                                     verdict="OPTION", detail=";".join(vals) +
                                     " over %d functions" % len(own[rel]), meant=meant))
                    continue
                raw_text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
                text = strip_comments(raw_text)
                bl = blocks(raw_text)
                for d in ";".join(vals).split(";"):
                    if not d:
                        continue
                    lhs = d.split("=", 1)[0]
                    hits = [m.start() for m in re.finditer(r"\b%s\b" % re.escape(lhs), text)]
                    where = set()
                    for h in hits:
                        enc = [s for s, a, b in bl if a <= h < b]
                        where.add(enc[0] if enc else "<file scope>")
                    if not hits:
                        v = "INERT"
                    elif meant and where <= {meant}:
                        v = "ONE"
                    else:
                        v = "WIDE"
                    rows.append(dict(tu=rel, kind=kind, line=line, prop=prop,
                                     verdict=v, detail="%s (%d hit(s) in %s)" % (
                                         d, len(hits), ", ".join(sorted(where)) or "-"),
                                     meant=meant))
    for r in rows:
        print("%-8s %s:%d [%s] %s  -- was %s" % (r["verdict"], r["tu"], r["line"],
                                              r["kind"], r["detail"][:160], r["meant"]))
    if args.json:
        pathlib.Path(args.json).write_text(json.dumps(rows, indent=1))
    from collections import Counter
    print(Counter(r["verdict"] for r in rows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
