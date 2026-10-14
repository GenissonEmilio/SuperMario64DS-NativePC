#!/usr/bin/env python3
"""cdefs_merge -- turn the second and later REPLACING per-source property rows
of port/CMakeLists.txt into APPEND rows, so a folded TU keeps every pre-fold
row (the PORTSYNC3 follow-up to cdefs_guard's refusal).

    python port/tools/cdefs_merge.py --report     list what would change
    python port/tools/cdefs_merge.py --apply      rewrite port/CMakeLists.txt

Why: main folded one-function files into one TU and the port's rows were
carried across by name, so several rows name one source. set_source_files_
properties REPLACES, so only the last survived. An APPEND keeps all of them.
What it cannot decide: two rows that define the SAME macro name with
DIFFERENT values for one TU (G0, VT0 ...). Those are listed as CONFLICT and
need a per-body decision; the tool still appends (the later value is the one
the compiler keeps, with a C4005 warning) so nothing is lost silently.
"""
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cdefs_guard as G  # noqa: E402

CMAKE = os.path.normpath(os.path.join(HERE, "..", "CMakeLists.txt"))


def cleaned_props_index(clean_call):
    m = re.search(r"\bPROPERTIES\b", clean_call)
    return m.start() if m else -1


def plan(text):
    calls, _unres = G.parse(text)
    coll = G.collisions(calls)
    # a call is rewritten when it is NOT the first replacing call for one of
    # its (source, prop) pairs
    first = {}
    for c in calls:
        if c.appends:
            continue
        for s in c.sources:
            first.setdefault((s, c.prop), c)
    victims = {}
    for (s, prop), lst in coll.items():
        for c in lst[1:]:
            victims.setdefault(id(c), (c, set()))[1].add(prop)
    return calls, victims


def rewrite(text):
    calls, victims = plan(text)
    clean = G._strip_comments(text)
    edits = []
    # group the Call objects by call span (a multi-prop call yields one Call per prop)
    by_span = {}
    for c in calls:
        by_span.setdefault((c.start, c.end), []).append(c)
    for (st, en), cs in by_span.items():
        vic = [c for c in cs if id(c) in victims]
        if not vic:
            continue
        orig = text[st:en]
        cl = clean[st:en]
        kind = cs[0].kind
        if kind == "set_property":
            # bare replacing form: make it APPEND
            new = re.sub(r"(\bPROPERTY\b)", r"APPEND \1", orig, count=1)
            edits.append((st, en, new))
            continue
        k = cleaned_props_index(cl)
        if k < 0:
            continue
        head_start = orig.index("(") + 1
        head = orig[head_start:k]
        pairs_all = []
        # re-split the property pairs from the cleaned tail
        tail = cl[k + len("PROPERTIES"):-1]
        args = G._split_args(tail)
        for i in range(0, len(args), 2):
            pairs_all.append((args[i], args[i + 1] if i + 1 < len(args) else ""))
        vprops = set(c.prop for c in vic)
        keep = [(p, v) for p, v in pairs_all if p not in vprops]
        out = []
        if keep:
            out.append("set_source_files_properties(" + head +
                       "PROPERTIES " + " ".join('%s "%s"' % (p, v) for p, v in keep) + ")")
        for p, v in pairs_all:
            if p in vprops:
                assert '"' not in v, v
                out.append("set_property(SOURCE " + head.strip() +
                           "\n    APPEND PROPERTY %s \"%s\")" % (p, v))
        edits.append((st, en, "\n".join(out)))
    return edits, victims


def apply_edits(text, edits):
    for st, en, new in sorted(edits, reverse=True):
        text = text[:st] + new + text[en:]
    return text


def conflicts(text):
    calls, _ = G.parse(text)
    per = {}
    for c in calls:
        if c.prop != "COMPILE_DEFINITIONS":
            continue
        for s in c.sources:
            for d in c.value.split(";"):
                if "=" in d:
                    n, v = d.split("=", 1)
                    per.setdefault(s, {}).setdefault(n, [])
                    if v not in per[s][n]:
                        per[s][n].append(v)
    return {s: {n: v for n, v in d.items() if len(v) > 1}
            for s, d in per.items() if any(len(v) > 1 for v in d.values())}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--report", action="store_true")
    a = ap.parse_args()
    text = open(CMAKE, encoding="utf-8", newline="").read()
    edits, victims = rewrite(text)
    print("calls to convert: %d" % len(edits))
    new = apply_edits(text, edits)
    cf = conflicts(new)
    for s, d in sorted(cf.items()):
        for n, v in d.items():
            print("CONFLICT %s %s -> %s" % (s, n, v))
    if a.apply:
        open(CMAKE, "w", encoding="utf-8", newline="").write(new)
        calls, _ = G.parse(new)
        print("collisions left: %d" % len(G.collisions(calls)))


if __name__ == "__main__":
    main()
