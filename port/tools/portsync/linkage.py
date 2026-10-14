#!/usr/bin/env python3
"""portsync linkage.py -- did the new build keep everything the old one linked?

    python port/tools/portsync/linkage.py [--config C] [--new-map M] [--new-exe E]
                                          [--ref-map M] [--ref-exe E] [--ref-root R]

Reads two MSVC /MAP files (the reference = the OLD exe's map, and the new one)
and two exes, and prints:

  LINKED  before -> after   matched src/ TUs whose object is in each map,
                            classified exactly like port/tools/linkage.py
                            (its own matched_index + map_symbols are used).
  GAINED / LOST             TU stems linked only in the new / only in the old
                            map. A LOST stem whose symbol is still defined in
                            the new map (a TU fold: e.g. Player ordinals folded
                            into src/actors/Player.cpp) is listed as FOLDED,
                            not LOST.
  MULTI                     stems that sit on more than one object.
  SYMBOLS REMOVED           public symbols the old map defines and the new one
                            does not, any object (count; list in the log).
  IMPORTS                   DLLs, import count and the sorted-name sha256 of
                            both exes (W42: KERNEL32 x155, sha 8eb5755c on 0.5.6).
  IMAGE                     base, size and end of both exes. The image must
                            not overlap DS main RAM (0x02000000..0x02800000)
                            or the fixed arena (0x30000000..0x30800000).

Verdict: FAIL on any truly LOST stem, any import change, or an image overlap.
GAINED stems and an image-size move are reported, not failed.

By default the "matched TU" list comes from tree_root (the new tree). With
--ref-root, the reference map's stems are classified against that (old) tree.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import portsync_common as C  # noqa: E402

RESERVED = [(0x01ff8000, 0x02800000, "DS ITCM / main RAM"),
            (0x30000000, 0x30800000, "the fixed game arena")]


def tree_linkage(tree):
    return C.load_tree_module("tree_linkage", "linkage.py", tree)


def matched_stems(tree):
    tl = tree_linkage(C.THIS_TREE)
    if tl is not None:
        return tl.matched_index(tree)
    out = {}
    for dp, _d, files in os.walk(os.path.join(tree, "src")):
        for fn in files:
            if fn.endswith((".c", ".cpp")):
                out.setdefault(os.path.splitext(fn)[0], os.path.relpath(os.path.join(dp, fn), tree))
    return out


def linked_stems(mapfile, matched):
    """{stem: set(objects)} for matched stems in this map (canonical rule:
    port/tools/linkage.py's map_symbols + stem = name without .c/.cpp.obj)."""
    tl = tree_linkage(C.THIS_TREE)
    if tl is not None:
        rows = tl.map_symbols(mapfile)
    else:
        rows = [(s, o) for s, a, o, st in C.MapInfo(mapfile).rows]
    out = {}
    for _sym, obj in rows:
        stem = C.obj_stem(obj)
        if stem in matched:
            out.setdefault(stem, set()).add(obj)
    return out


def compare_maps(ref_map, new_map, matched_new, matched_ref=None):
    matched_ref = matched_ref or matched_new
    old = linked_stems(ref_map, matched_ref)
    new = linked_stems(new_map, matched_new)
    new_info = C.MapInfo(new_map)
    old_info = C.MapInfo(ref_map)
    new_def = new_info.defined()
    old_def = old_info.defined()
    gained = sorted(set(new) - set(old))
    lost_all = sorted(set(old) - set(new))
    folded, lost = [], []
    for st in lost_all:
        o = new_def.get(st)
        if o is not None and C.obj_stem(o) in matched_new:
            folded.append((st, o))
        else:
            lost.append(st)
    multi = sorted(s for s, objs in new.items() if len(objs) > 1)
    removed = sorted(s for s in old_def if s not in new_def)
    return {"linked_before": len(old), "linked_after": len(new), "gained": gained,
            "lost": lost, "folded": folded, "multi": multi, "symbols_removed": removed,
            "symbols_before": len(old_def), "symbols_after": len(new_def)}


def overlaps(base, end):
    return [name for lo, hi, name in RESERVED if base < hi and end > lo]


def compare_exes(ref_exe, new_exe):
    a, b = C.pe_info(ref_exe), C.pe_info(new_exe)
    added = sorted(set(b["imports"]) - set(a["imports"]))
    removed = sorted(set(a["imports"]) - set(b["imports"]))
    return {"ref": a, "new": b, "imports_added": added, "imports_removed": removed,
            "imports_same": a["import_sha256"] == b["import_sha256"] and a["dlls"] == b["dlls"],
            "overlap": overlaps(b["base"], b["end"])}


def describe_pe(tag, p):
    return ("%s: dlls=%s imports=%d sorted-name sha256=%s base 0x%08x size 0x%08x END 0x%08x"
            % (tag, ",".join(p["dlls"]), p["import_count"], p["import_sha256"][:8],
               p["base"], p["size"], p["end"]))


def run(cfg, new_map, new_exe, ref_map=None, ref_exe=None, ref_root=None):
    ref_exe = ref_exe or cfg["old_exe"]
    ref_map = ref_map or C.reference_map(cfg)
    for p in (ref_map, new_map, ref_exe, new_exe):
        if not os.path.exists(p):
            return C.StepResult("linkage", C.ERROR, "missing file %s" % p)
    for p in (ref_map, new_map):
        if os.path.getsize(p) < 4096:
            return C.StepResult("linkage", C.ERROR, "map %s is %d bytes: a failed link"
                                % (p, os.path.getsize(p)))
    mn = matched_stems(cfg["tree_root"])
    mr = matched_stems(ref_root) if ref_root else None
    m = compare_maps(ref_map, new_map, mn, mr)
    e = compare_exes(ref_exe, new_exe)
    d = []
    d.append("LINKED %d -> %d (%+d)   [matched TUs from %s]" % (
        m["linked_before"], m["linked_after"], m["linked_after"] - m["linked_before"],
        "the old tree for the old map" if ref_root else "this tree for both maps"))
    d.append("GAINED %d: %s" % (len(m["gained"]), " ".join(m["gained"][:40]) + (" ..." if len(m["gained"]) > 40 else "")))
    d.append("LOST %d: %s" % (len(m["lost"]), " ".join(m["lost"][:40])))
    d.append("FOLDED %d (stem gone, symbol still linked from another matched TU): %s" % (
        len(m["folded"]), " ".join("%s->%s" % f for f in m["folded"][:20])))
    d.append("stems on more than one object in the new map: %d %s" % (len(m["multi"]), " ".join(m["multi"][:20])))
    d.append("public symbols %d -> %d; removed %d" % (m["symbols_before"], m["symbols_after"],
                                                     len(m["symbols_removed"])))
    for s in m["symbols_removed"][:30]:
        d.append("   removed: %s" % s)
    d.append(describe_pe("imports ref", e["ref"]))
    d.append(describe_pe("imports new", e["new"]))
    if e["imports_added"] or e["imports_removed"]:
        d.append("imports added %s removed %s" % (e["imports_added"], e["imports_removed"]))
    d.append("image size %+d bytes vs the reference (0x%08x -> 0x%08x)" % (
        e["new"]["size"] - e["ref"]["size"], e["ref"]["size"], e["new"]["size"]))
    bad = []
    if m["lost"]:
        bad.append("%d LOST" % len(m["lost"]))
    if not e["imports_same"]:
        bad.append("imports changed")
    if e["overlap"]:
        bad.append("image overlaps %s" % ", ".join(e["overlap"]))
    summ = "LINKED %d -> %d, GAINED %d LOST %d FOLDED %d, imports %s x%d %s, image end 0x%08x" % (
        m["linked_before"], m["linked_after"], len(m["gained"]), len(m["lost"]), len(m["folded"]),
        "+".join(e["new"]["dlls"]), e["new"]["import_count"], e["new"]["import_sha256"][:8],
        e["new"]["end"])
    status = C.FAIL if bad else C.PASS
    if bad:
        summ = "; ".join(bad) + " | " + summ
    return C.StepResult("linkage", status, summ, d, data={"maps": m, "exes": {
        "imports_same": e["imports_same"], "overlap": e["overlap"]}})


def main(argv=None):
    ap = C.common_args(argparse.ArgumentParser(description="portsync: linkage pair"))
    ap.add_argument("--new-map")
    ap.add_argument("--new-exe")
    ap.add_argument("--ref-map")
    ap.add_argument("--ref-exe")
    ap.add_argument("--ref-root")
    a = ap.parse_args(argv)
    r = C.handle_write_config(a)
    if r is not None:
        return r
    try:
        cfg = C.load_config(a.config, require=("work_dir", "old_exe"))
    except C.ConfigError as e:
        print("linkage: %s" % e)
        return 2
    C.open_log(cfg, "linkage")
    new_exe = a.new_exe or os.path.join(cfg["tree_root"], "build", "port", "walk_window.exe")
    new_map = a.new_map or os.path.splitext(new_exe)[0] + ".map"
    res = run(cfg, new_map, new_exe, a.ref_map, a.ref_exe, a.ref_root)
    for line in res.details:
        C.log(line)
    print(C.table([[res.step, res.status, res.summary]], ["STEP", "RESULT", "WHAT"]))
    return 0 if res.status == C.PASS else 1


if __name__ == "__main__":
    sys.exit(main())
