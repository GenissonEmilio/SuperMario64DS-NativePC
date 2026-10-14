#!/usr/bin/env python3
"""linkalias -- bind the flat C names main's TUs ask for onto what the link has.

    python port/tools/linkalias.py --unresolved <list> --map <map> --old-config <config dir of the pre-sync tree> --out port/hal/portsync3_link_aliases.cpp

Three rules, tried in order, for every unresolved name of the form `_name`
(a C-linkage reference; Itanium `__ZN...` names go through facegen.py):

  1. RENAMED BY ADDRESS. Main renamed a ROM datum or function (data_020a0de8 ->
     gTouchHeld). The old name is still the one the port hosts. The old name at
     the same (module, address) in the pre-sync config, if the link defines it,
     is the target. Never by name alone (sm64ds-port-clinkage-flip, family 1).
  2. DEFINED WITH C++ LINKAGE. Main's folded TU defines `void func_0200d3f8(...)`
     with no extern "C" while another TU calls it through a C-linkage
     declaration. A cdecl free function or a variable that the link defines
     under exactly one decorated spelling (`?name@@YA...`, `?name@@3...`) is the
     target. A signature mismatch cannot be seen here; both sides are the same
     source tree and only the linkage differs.
  3. RENAMED INTO A MEMBER. The old flat name (func_ov006_020d25fc) is now the
     member dScMgAmida_c::StepWalker (config: _ZN12dScMgAmida_c10StepWalkerEii).
     The row aliases the old spelling onto the new flat Itanium name and lists
     that name in --faces-out; facegen.py --derive writes the face that makes it
     a real cdecl entry into the member.
  4. A STATIC MEMBER OR NAMESPACE FUNCTION. The flat Itanium name
     `__ZN5Sound13Func_02048ec4Ev` names a function with no receiver; MSVC defines
     the same thing as `?Func_02048ec4@Sound@@SAXXZ` (static member) or
     `...@@YAXXZ` (namespace function). Both are cdecl, so an /alternatename is
     the whole bridge (facegen refuses these rows only because they are not
     __thiscall members). Taken when the link holds exactly one such definition
     of that class and method.
  5. A DECORATED FREE FUNCTION WITH ANOTHER RETURN TYPE OR A FLAT HOST BODY.
     TUs of main spell one cdecl function with different return types
     (`G2S::GetBG3CharPtr` as char *, void *, int; the link defines it once, as
     unsigned int). A cdecl function with no parameters returns the same register
     whatever the declared type, so the unresolved spelling is aliased onto the
     defined one. A decorated namespace/static function whose Itanium flat
     spelling (`__ZN4CP1527FlushAndInvalidateDataCacheEjj`) the port defines is
     aliased onto that flat definition. Both only when the match is unique.
  6. THE SAME ADDRESS UNDER ANOTHER dsd SPELLING. dsd names an address by the
     module it picked (`data_ov000_020ad524`, `data_020ad524`, `data_ov062_...`)
     and main's TUs and the port's mounts do not always pick the same one. A
     name the link lacks, whose address-and-kind another definition in the link
     already carries under exactly one other module spelling, is aliased onto it.
  7. VIRTUAL VERSUS NON-VIRTUAL SPELLING OF ONE MEMBER. A port shadow class
     declares `int InitResources();` (public non-virtual, `QAE`) where main's
     real class has `virtual int InitResources()` (`UAE`); the shadow means
     "call this class's own body directly", which is what the __thiscall entry
     of the virtual's definition is. The access/virtual letter is swapped
     (Q<->U, I<->M, A<->E) and the swapped name must be defined.
  8. CLASS-KEY MISMATCH. One header declares `struct Heap`, a port file says
     `class Heap`; MSVC mangles them differently (U versus V) and the symbol
     is one function. The tag in a pointer or reference parameter is swapped
     and the swapped name must be defined.
  9. STATIC MEMBER VERSUS NAMESPACE FUNCTION (`SA` <-> `YA`) AND FLAT NAMES SPELLED
     WITH C++ LINKAGE (`?_ZN3OAM11GetObjWidthEii@@YAEHH@Z` is the flat Itanium
     name declared without extern "C"); both are cdecl and the same function.
  10. THE FIXED TABLE below: linker-script symbols dsd generated for the DS
     image (OVERLAY_<n>_ID, OV075_BSS_END, OV100_TEXT_START), bound to the hosts
     the port already defines under their pre-sync spellings.
"""
import argparse
import collections
import glob
import os
import re
import sys

FIXED = {
    # the two Heap*-taking Memory entry points: the flat spellings ActorBase.cpp's
    # destructors call. Heap.cpp defines them as namespace functions (cdecl).
    "__ZN6Memory10DeallocateEPvP4Heap": "?Deallocate@Memory@@YAXPAXPAUHeap@@@Z",
    "__ZN6Memory8AllocateEjiP4Heap": "?Allocate@Memory@@YAPAXIHPAUHeap@@@Z",
    # the short-angle helpers of src/ApproachAngle.cpp: callers disagree about the
    # return type (void / int / bool); every spelling is the same cdecl function
    "?GetBG3CharPtr@G2S@@YAPADXZ": "?GetBG3CharPtr@G2S@@YAIXZ",
    "?FlushAndInvalidateDataCache@CP15@@YAXII@Z": "__ZN4CP1527FlushAndInvalidateDataCacheEjj",
    "__Z11UpdateAngleRssis": "?UpdateAngle@@YAXAAFFHF@Z",
    "__Z11UpdateAngleRssis__hg_ns_020cb2b4": "?UpdateAngle@@YAXAAFFHF@Z",
    "?ApproachLinear@@YAHAAFFF@Z": "?ApproachLinear@@YA_NAAFFF@Z",
    "?ApproachLinear2@@YAFAAFFF@Z": "?ApproachLinear2@@YAHAAFFF@Z",
    # The id of an overlay is the ADDRESS of a symbol. The port hosts one int per id.
    "_OV075_BSS_END": "_data_ov062_0211d9c0",   # hal/link21_rows.cpp's arena-base trap
    "_OV100_TEXT_START": "_func_02140d80",      # hal/scene_boot.cpp's trap
}
OVERLAY_IDS = (2, 3, 4, 5, 6, 7, 60, 64, 66, 75, 98, 100, 102)


def load_cfg(root):
    names = {}
    inv = collections.defaultdict(list)
    for f in glob.glob(os.path.join(root, "config", "arm9", "**", "symbols.txt"), recursive=True):
        lab = os.path.basename(os.path.dirname(f))
        for l in open(f, encoding="utf-8", errors="ignore"):
            m = re.match(r"(\S+)\s+kind:(\w+)(?:\(([^)]*)\))?\s+(?:.*?)addr:0x([0-9a-fA-F]+)", l)
            if m:
                names[(lab, int(m.group(4), 16))] = m.group(1)
                inv[m.group(1)].append((lab, int(m.group(4), 16)))
    return names, inv


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unresolved", required=True)
    ap.add_argument("--map", required=True)
    ap.add_argument("--old-config", required=True, help="root holding the pre-sync config/")
    ap.add_argument("--out", required=True)
    ap.add_argument("--faces-out", help="write the flat Itanium names a face must define")
    a = ap.parse_args()
    repo = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    new, newinv = load_cfg(repo)
    old, oldinv = load_cfg(a.old_config)
    faces = []
    txt = open(a.map, encoding="utf-8", errors="replace").read()
    defs = set(re.findall(r"\s(_[\w$@?]+|\?[\w$@?]+)\s+[0-9a-f]{8}\s", txt))
    # The map of a link that already carried the previous output lists every alias
    # as a public. A name this tool aliased is not a REAL definition, so it must
    # not count as a candidate target (that made the second run find two).
    if os.path.isfile(a.out):
        prev = open(a.out, encoding="utf-8", errors="replace").read()
        defs -= set(re.findall(r"/alternatename:([^=\s\"]+)=", prev))
    dec = collections.defaultdict(list)
    for d in defs:
        m = re.match(r"\?(\w+)@@(Y|3)", d)
        if m:
            dec[m.group(1)].append(d)
    statics = collections.defaultdict(list)
    for d in defs:
        m = re.match(r"\?(\w+)@(\w+)@@(?:SA|YA)", d)
        if m:
            statics[(m.group(2), m.group(1))].append(d)
    freefn = collections.defaultdict(list)
    for d in defs:
        m = re.match(r"\?(\w+)@(\w+)@@(YA|SA)(.*)$", d)
        if m:
            freefn[(m.group(2), m.group(1))].append(d)
    flatit = collections.defaultdict(list)
    for d in defs:
        m = re.match(r"__ZN(\d+)(\w+)", d)
        if m:
            # first two components only; the rest is parameters
            s = d[4:]
            parts, i = [], 0
            while i < len(s) and s[i].isdigit():
                j = i
                while s[j].isdigit():
                    j += 1
                ln = int(s[i:j])
                parts.append(s[j:j + ln])
                i = j + ln
                if len(parts) == 2:
                    break
            if len(parts) == 2:
                flatit[(parts[0], parts[1])].append(d)
    samead = collections.defaultdict(list)
    for d in defs:
        ms = re.match(r"_(data|func)_(?:ov\d+_)?([0-9a-f]{8})$", d)
        if ms:
            samead[(ms.group(1), ms.group(2))].append(d)
    rows = []
    left = []
    for line in open(a.unresolved, encoding="utf-8", errors="replace"):
        s = line.split("  <- ")[0].strip()
        fk = re.search(r"\((\?[\w$@?]+)\)$", s)
        fk = fk.group(1) if fk else s
        if fk in FIXED and FIXED[fk] in defs:
            rows.append((fk, FIXED[fk], "fixed table"))
            continue
        if s.startswith('"') and "(?" in s:
            dm = re.search(r"\((\?[\w$@?]+)\)$", s)
            if dm:
                dsym = dm.group(1)
                mm = re.match(r"(\?\??[\w$]+@[\w@]+?@@)([QUIMAE])([AB]E)(.*)$", dsym)
                if mm and dsym not in defs:
                    swap = {"Q": "U", "U": "Q", "I": "M", "M": "I", "A": "E", "E": "A"}[mm.group(2)]
                    alt = mm.group(1) + swap + mm.group(3) + mm.group(4)
                    if alt in defs:
                        rows.append((dsym, alt, "virtual / non-virtual spelling of one member"))
                        continue
        if s.startswith('"') and "(?" in s:
            dm = re.search(r"\((\?[\w$@?]+)\)$", s)
            if dm and dm.group(1) not in defs:
                d0 = dm.group(1)
                for a_, b_ in (("PAV", "PAU"), ("PAU", "PAV"), ("AAV", "AAU"), ("AAU", "AAV")):
                    alt = d0.replace(a_, b_)
                    if alt != d0 and alt in defs:
                        rows.append((d0, alt, "class key mismatch (struct versus class)"))
                        break
        if s.startswith('"') and "(?" in s:
            dm = re.search(r"\((\?[\w$@?]+)\)$", s)
            if dm:
                d1 = dm.group(1)
                alt = None
                if "@@SA" in d1:
                    alt = d1.replace("@@SA", "@@YA", 1)
                elif "@@YA" in d1:
                    alt = d1.replace("@@YA", "@@SA", 1)
                if alt and alt in defs and d1 not in defs:
                    rows.append((d1, alt, "static member versus namespace function"))
                    continue
                mf = re.match(r"\?(_Z\w+)@@YA", d1)
                if mf and ("_" + mf.group(1)) in defs and d1 not in defs:
                    rows.append((d1, "_" + mf.group(1), "flat Itanium name declared with C++ linkage"))
                    continue
        if s.startswith("?") or (s.startswith('"') and "(?" in s):
            dsym = re.search(r"\((\?[\w$@?]+)\)$", s)
            dsym = dsym.group(1) if dsym else s
            m = re.match(r"\?(\w+)@(\w+)@@(YA|SA)(.*)$", dsym)
            if m:
                key = (m.group(2), m.group(1))
                others = [d for d in freefn.get(key, []) if d != dsym]
                if len(others) == 1 and dsym.endswith("XZ") and others[0].endswith("XZ"):
                    rows.append((dsym, others[0], "same cdecl function, other return type"))
                elif len(flatit.get(key, [])) == 1:
                    rows.append((dsym, flatit[key][0], "flat host definition of the same function"))
            continue
        if s.startswith("__ZN"):
            i, parts = 4, []
            while i < len(s) and s[i].isdigit():
                j = i
                while s[j].isdigit():
                    j += 1
                ln = int(s[i:j])
                parts.append(s[j:j + ln])
                i = j + ln
            if len(parts) == 2 and len(statics.get((parts[0], parts[1]), [])) == 1:
                rows.append((s, statics[(parts[0], parts[1])][0],
                             "static member / namespace function (cdecl)"))
            continue
        if not s.startswith("_") or s.startswith("__Z") or s.startswith("___sinit") or s.startswith('"'):
            continue
        n = s[1:]
        tgt = None
        why = ""
        for key in newinv.get(n, []):
            o = old.get(key)
            if o and o != n and ("_" + o) in defs:
                tgt, why = "_" + o, "renamed from %s at the same address" % o
                break
        if tgt is None:
            for key in oldinv.get(n, []):
                nn = new.get(key)
                if nn and nn != n and nn.startswith("_ZN"):
                    tgt, why = "_" + nn, "renamed into member %s" % nn
                    faces.append("_" + nn)
                    break
        if tgt is None and len(dec.get(n, [])) == 1:
            tgt, why = dec[n][0], "defined with C++ linkage"
        if tgt is None:
            ms = re.match(r"_(data|func)_(?:ov\d+_)?([0-9a-f]{8})$", s)
            if ms:
                cands = [d for d in samead.get((ms.group(1), ms.group(2)), []) if d != s]
                if len(cands) == 1:
                    tgt, why = cands[0], "same address, another module spelling"
        if tgt is None and s in FIXED and FIXED[s] in defs:
            tgt, why = FIXED[s], "linker-script symbol, hosted"
        if tgt is None:
            m = re.match(r"_OVERLAY_(\d+)_ID$", s)
            if m and int(m.group(1)) in OVERLAY_IDS and ("_overlay_" + m.group(1)) in defs:
                tgt, why = "_overlay_" + m.group(1), "linker-script overlay id, hosted as overlay_%s" % m.group(1)
        if tgt:
            rows.append((s, tgt, why))
        else:
            left.append(s)
    with open(a.out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("/* hal/portsync3_link_aliases.cpp -- flat C names onto what the link already has.\n")
        fh.write(" *\n * PORTSYNC3. GENERATED by port/tools/linkalias.py, then checked in. %d rows;\n" % len(rows))
        fh.write(" * the rule for each is the comment on its line. Includes nothing.\n */\n\n")
        for s, t, why in rows:
            fh.write('#pragma comment(linker, "/alternatename:%s=%s")  /* %s */\n' % (s, t, why))
    if a.faces_out:
        with open(a.faces_out, "w", encoding="utf-8", newline="\n") as fh:
            fh.write("\n".join(faces) + "\n")
    print("linkalias: %d rows -> %s; %d left; %d faces needed" % (len(rows), a.out, len(left), len(faces)))
    for s in left:
        print("  left: " + s)


if __name__ == "__main__":
    main()
