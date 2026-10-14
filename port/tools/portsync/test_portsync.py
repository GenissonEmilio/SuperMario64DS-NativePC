#!/usr/bin/env python3
"""Self-test of the portsync scripts' LOGIC (no game, no build, no Windows).

    python port/tools/portsync/run_all.py --selftest
    python port/tools/portsync/test_portsync.py

Covers: config scaffolding / placeholders / path resolution, MSVC map parsing
and the LINKED / GAINED / LOST / FOLDED pair on fake maps, PE import parsing on
a synthesized image, gate row measurement + verdicts and the two settings
modes, the frame-hash comparison (first difference, noise floor, NEW-only
instability, unstable fields), the arena mask (noise words, relocation
classification, mask file round trip), the foreign-process wait with a mocked
process list, the staged-script rewrite and the build pools, knob detection in
an exe, and run_all's stop-at-first-fail table.
"""
import io
import json
import os
import shutil
import struct
import sys
import tempfile
import unittest
from collections import OrderedDict
from contextlib import redirect_stdout

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import portsync_common as C  # noqa: E402
import build  # noqa: E402
import compare  # noqa: E402
import gate  # noqa: E402
import linkage  # noqa: E402
import run_all  # noqa: E402


def write(p, text, mode="w"):
    d = os.path.dirname(p)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(p, mode) as f:
        f.write(text)
    return p


MAP_HEAD = """ walk_window

 Timestamp is 66e0b1c2 (Tue Sep 10 12:00:00 2024)

 Preferred load address is 10000000

 Start         Length     Name                   Class
 0001:00000000 00123456H .text$mn                CODE

  Address         Publics by Value              Rva+Base       Lib:Object

 0000:00000000       ___safe_se_handler_count   00000000     <absolute>
"""


def map_text(rows, pad=120):
    """rows: [(sym as in the map, addr, obj, flag)]"""
    out = [MAP_HEAD]
    for sym, addr, obj, fl in rows:
        out.append(" 0001:%08x       %-40s %08x %s %s\n" % (addr - 0x10001000, sym, addr,
                                                            fl or " ", obj))
    out.append("\n entry point at        0001:00000000\n\n Static symbols\n\n")
    for i in range(pad):     # the tree's map_symbols refuses maps under 4 KB
        out.append(" 0001:%08x       _pad_static_%04d                 %08x f   pad.obj\n"
                   % (0x9000 + i * 16, i, 0x1000a000 + i * 16))
    return "".join(out)


class Tmp(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp(prefix="portsync_test_")

    def tearDown(self):
        shutil.rmtree(self.d, ignore_errors=True)


class TestConfig(Tmp):
    def test_scaffold_and_load(self):
        p = os.path.join(self.d, "cfg", "portsync_config.json")
        C.write_config(p)
        with self.assertRaises(C.ConfigError):
            C.write_config(p)                       # refuses to overwrite
        C.write_config(p, force=True)
        with open(p, "rb") as f:
            raw = f.read()
        self.assertTrue(all(b < 128 for b in raw), "config is ASCII")
        with self.assertRaises(C.ConfigError) as cm:
            C.load_config(p, require=("work_dir", "old_exe"))
        self.assertIn("work_dir", str(cm.exception))
        self.assertIn("old_exe", str(cm.exception))
        cfg = C.load_config(p)                      # nothing required: loads
        self.assertEqual(cfg["tree_root"], C.THIS_TREE)
        data = json.load(open(p))
        data["work_dir"] = "work"
        data["old_exe"] = "old/walk_window.exe"
        data["build"]["lock_path"] = "locks/port_build.lock"
        data["runs"] = {"max_parallel": 1}          # partial block: defaults merge in
        json.dump(data, open(p, "w"))
        cfg = C.load_config(p, require=("work_dir", "old_exe", "build.lock_path"))
        self.assertEqual(cfg["work_dir"], os.path.normpath(os.path.join(self.d, "cfg", "work")))
        self.assertEqual(cfg["runs"]["max_parallel"], 1)
        self.assertEqual(cfg["runs"]["foreign_processes"], ["walk_window.exe", "SM64DSLauncher.exe"])
        self.assertEqual(C.reference_map(cfg), os.path.join(self.d, "cfg", "old", "walk_window.map"))
        self.assertEqual(cfg["asset_root"], cfg["tree_root"])

    def test_no_config(self):
        with self.assertRaises(C.ConfigError):
            C.load_config(os.path.join(self.d, "missing.json"))

    def test_write_config_cli(self):
        p = os.path.join(self.d, "c.json")
        buf = io.StringIO()
        with redirect_stdout(buf):
            rc = run_all.main(["--write-config", "--config", p])
        self.assertEqual(rc, 0)
        self.assertTrue(os.path.exists(p))
        with redirect_stdout(io.StringIO()):
            self.assertEqual(run_all.main(["--config", p]), 2)   # placeholders -> 2


class TestMapsAndLinkage(Tmp):
    def make_tree(self):
        t = os.path.join(self.d, "tree")
        for stem in ("func_02000100", "func_02000200", "_ZN6Player8BehaviorEv", "Player", "func_02000300",
                     "func_02000400"):
            write(os.path.join(t, "src", stem + ".cpp"), "//cpp\n")
        return t

    def test_map_parse(self):
        p = write(os.path.join(self.d, "a.map"), map_text([
            ("_func_02000100", 0x10001000, "func_02000100.cpp.obj", "f"),
            ("__ZN6Player8BehaviorEv", 0x10001100, "walk_window_lib.lib:_ZN6Player8BehaviorEv.cpp.obj", "f"),
            ("??_7Foo@@6B@", 0x10002000, "foo.cpp.obj", ""),
        ]))
        m = C.MapInfo(p)
        self.assertEqual(m.base, 0x10000000)
        d = m.defined()
        self.assertEqual(d["func_02000100"], "func_02000100.cpp.obj")
        self.assertEqual(d["_ZN6Player8BehaviorEv"], "_ZN6Player8BehaviorEv.cpp.obj")
        self.assertEqual(d["??_7Foo@@6B@"], "foo.cpp.obj")
        self.assertEqual(m.symbolize(0x10001104), ("_ZN6Player8BehaviorEv", 4))
        self.assertIsNone(m.symbolize(0x10000fff))
        self.assertEqual(C.obj_stem("x.c.obj"), "x")

    def test_pair(self):
        t = self.make_tree()
        old = write(os.path.join(self.d, "old.map"), map_text([
            ("_func_02000100", 0x10001000, "func_02000100.cpp.obj", "f"),
            ("_func_02000200", 0x10001100, "func_02000200.cpp.obj", "f"),
            ("__ZN6Player8BehaviorEv", 0x10001200, "_ZN6Player8BehaviorEv.cpp.obj", "f"),
            ("_func_02000300", 0x10001300, "func_02000300.cpp.obj", "f"),
            ("_host_only", 0x10001400, "hostfile.cpp.obj", "f"),
        ]))
        new = write(os.path.join(self.d, "new.map"), map_text([
            ("_func_02000100", 0x10001000, "func_02000100.cpp.obj", "f"),
            ("__ZN6Player8BehaviorEv", 0x10001200, "Player.cpp.obj", "f"),   # folded
            ("_func_02000400", 0x10001500, "func_02000400.cpp.obj", "f"),   # gained
            ("_func_02000400_dup", 0x10001600, "sub/func_02000400.cpp.obj", "f"),
            ("_host_only", 0x10001400, "hostfile.cpp.obj", "f"),
        ]))
        matched = linkage.matched_stems(t)
        m = linkage.compare_maps(old, new, matched)
        self.assertEqual(m["linked_before"], 4)
        self.assertEqual(m["linked_after"], 3)        # 0100, Player, 0400
        self.assertEqual(m["gained"], ["Player", "func_02000400"])
        self.assertEqual(m["lost"], ["func_02000200", "func_02000300"])
        self.assertEqual(m["folded"], [("_ZN6Player8BehaviorEv", "Player.cpp.obj")])
        self.assertEqual(m["symbols_removed"], ["func_02000200", "func_02000300"])

    def test_pe_and_run(self):
        t = self.make_tree()
        ref_pe = C.build_test_pe(0x10000000, 0x0133f000, [("KERNEL32.dll", ["Sleep", "GetTickCount"])])
        new_pe = C.build_test_pe(0x10000000, 0x01340000, [("KERNEL32.dll", ["GetTickCount", "Sleep"])])
        bad_pe = C.build_test_pe(0x00400000, 0x01c00000, [("KERNEL32.dll", ["Sleep", "ExitProcess"])])
        for n, b in (("ref.exe", ref_pe), ("new.exe", new_pe), ("bad.exe", bad_pe)):
            write(os.path.join(self.d, n), b, "wb")
        info = C.pe_info(os.path.join(self.d, "ref.exe"))
        self.assertEqual(info["base"], 0x10000000)
        self.assertEqual(info["end"], 0x1133f000)
        self.assertEqual(info["dlls"], ["KERNEL32.dll"])
        self.assertEqual(sorted(info["imports"]), ["GetTickCount", "Sleep"])
        import hashlib
        self.assertEqual(info["import_sha256"],
                         hashlib.sha256(b"GetTickCount\nSleep").hexdigest())
        e = linkage.compare_exes(os.path.join(self.d, "ref.exe"), os.path.join(self.d, "new.exe"))
        self.assertTrue(e["imports_same"])
        self.assertEqual(e["overlap"], [])
        e = linkage.compare_exes(os.path.join(self.d, "ref.exe"), os.path.join(self.d, "bad.exe"))
        self.assertFalse(e["imports_same"])
        self.assertEqual(e["imports_added"], ["ExitProcess"])
        self.assertIn("DS ITCM / main RAM", e["overlap"])        # 0x400000 + 0x1c00000 > 0x02000000
        maptxt = map_text([("_func_02000100", 0x10001000, "func_02000100.cpp.obj", "f")])
        for n in ("ref.map", "new.map"):
            write(os.path.join(self.d, n), maptxt)
        cfg = {"tree_root": t, "old_exe": os.path.join(self.d, "ref.exe"), "reference_map": ""}
        r = linkage.run(cfg, os.path.join(self.d, "new.map"), os.path.join(self.d, "new.exe"))
        self.assertEqual(r.status, C.PASS, r.summary)
        self.assertIn("LINKED 1 -> 1", r.details[0])
        write(os.path.join(self.d, "bad.map"), maptxt)
        r = linkage.run(cfg, os.path.join(self.d, "bad.map"), os.path.join(self.d, "bad.exe"))
        self.assertEqual(r.status, C.FAIL)
        self.assertIn("imports changed", r.summary)


class TestGate(Tmp):
    def test_measure(self):
        self.assertEqual(gate.measure("sha8", "", "0a913ea7ffff", "x"), "0a913ea7")
        self.assertIsNone(gate.measure("sha8", "", "NO_FILE", "x"))
        self.assertEqual(gate.measure("md58", "", "x", "946a90a4aaaa"), "946a90a4")
        out = "[scene] unmatched-body traps entered: 0 (none of the 24 trapping sites was reached)\n"
        self.assertEqual(gate.measure("traps", out, "", ""), 0)
        out = ("[heap] game heap 3000 , 0x1 bytes, 9 free after boot\n"
               "[heap] 164800 free after 600 frames\n[heap] root heap 5920128 free at the end of the run\n")
        self.assertEqual(gate.measure("irq", out, "", ""), {"heap": 164800, "root": 5920128})
        ab = ("[raster-ab] summary over 300 frame(s), 2 pass run(s), mismatches colour 0 depth 0 coverage 0 "
              "stencil 0 attrid 0 tlattr 0, frames with any 0 VERDICT=PASS\n")
        self.assertEqual(gate.measure("ab", ab, "", ""), "PASS")
        ab2 = ab.replace("frames with any 0", "frames with any 3")
        self.assertEqual(gate.measure("ab", ab2, "", ""), "PASS(3 bad frames)")
        tw = ("[twod] SUMMARY sub verify: 600 scan-outs compared, 0 differ\n"
              "[twod] SUMMARY engA verify: 600 composites compared, 0 differ\n")
        self.assertEqual(gate.measure("twod", tw, "", ""), 0)
        self.assertEqual(gate.measure("twod", tw + "[twod] MISMATCH at 3\n", "", ""), 1)
        end = "[lvl] change: level 40 -> 1\n" * 22
        v = gate.measure("ending", end, "", "d44a9218zz")
        self.assertEqual(v, {"theend": "d44a9218", "changes": 22, "quarantine": 0, "backstop": 0})

    def test_verdicts(self):
        refs = gate.references({"gate": {"references": {"on": {"f3off": "deadbeef"}}}})
        self.assertEqual(refs["off"]["ident_l6_f600"], "cd4c7a33")
        self.assertEqual(refs["on"]["ident_l8_f900"], "03ead794")
        self.assertEqual(refs["on"]["f3off"], "deadbeef")       # config override
        v = gate.verdict
        self.assertEqual(v("ident_l6_f600", "sha8", "64035479", refs["on"]["ident_l6_f600"], True)[0], C.PASS)
        self.assertEqual(v("ident_l6_f600", "sha8", "cd4c7a33", refs["on"]["ident_l6_f600"], True)[0], C.FAIL)
        self.assertEqual(v("ident_l1_f600", "sha8", "12345678", refs["on"].get("ident_l1_f600"), True)[0],
                         C.RECORD)
        self.assertEqual(v("ident_l1_f600", "sha8", "0a913ea7", "0a913ea7", False)[0], C.FAIL)
        self.assertEqual(v("x", "sha8", None, "0a913ea7", True)[0], C.FAIL)
        self.assertEqual(v("irq_L1", "irq", {"heap": 164800, "root": 1}, refs["off"]["irq_L1"], True)[0], C.FAIL)
        self.assertEqual(v("irq_L1", "irq", {"heap": 164800, "root": 5920128}, refs["off"]["irq_L1"], True)[0],
                         C.PASS)
        good = {"theend": "d44a9218", "changes": 22, "quarantine": 0, "backstop": 0}
        self.assertEqual(v("ending", "ending", good, refs["off"]["ending"], True)[0], C.PASS)
        self.assertEqual(v("ending", "ending", dict(good, quarantine=1), refs["off"]["ending"], True)[0], C.FAIL)
        # summary: one FAIL makes the step FAIL; RECORD does not
        res = gate.summarize([["a", "off", C.PASS, "", "", ""], ["b", "on", C.RECORD, "", "", ""]], __file__, ["off", "on"])
        self.assertEqual(res.status, C.PASS)
        res = gate.summarize([["a", "off", C.PASS, "", "", ""], ["b", "on", C.FAIL, "", "", "why"]], __file__, ["off", "on"])
        self.assertEqual(res.status, C.FAIL)
        self.assertIn("b/on", res.summary)

    def test_rows_and_modes(self):
        names = [r[0] for r in gate.ident_rows()]
        self.assertEqual(len(names), 8)
        self.assertIn("rendereron", names)
        envs = dict((r[0], r[1]) for r in gate.other_rows(gate.ALL_PARTS))
        self.assertEqual(envs["ending"]["SM64DS_SPAWN_ACTOR"], "283:7")
        self.assertEqual(envs["traps_all"]["SM64DS_RENDERER"], "1")
        # settings modes: off writes the two keys false, on writes nothing
        w = os.path.join(self.d, "row")
        os.makedirs(w)
        p = C.write_settings(w, "off")
        self.assertEqual(json.load(open(p)), {"OneScreenMenus": False, "HudLayout": False})
        self.assertIsNone(C.write_settings(w, "on"))
        self.assertFalse(os.path.exists(os.path.join(w, "settings.json")))
        p = C.write_settings(w, "off", {"SmoothMotion": False})
        self.assertEqual(json.load(open(p))["SmoothMotion"], False)
        with self.assertRaises(ValueError):
            C.write_settings(w, "sideways")

    def test_quiet_env(self):
        base = {"PATH": "x", "SM64DS_LEVEL": "99", "SM64DS_TEST_LOCK": "1"}
        e = C.quiet_env("W", "n", "A", {"SM64DS_LEVEL": 6}, base=base)
        self.assertEqual(e["SM64DS_LEVEL"], "6")
        self.assertNotIn("SM64DS_TEST_LOCK", e)                  # inherited SM64DS_* dropped
        for k, val in (("SM64DS_NO_FOCUS", "1"), ("SM64DS_VOLUME", "0"), ("SM64DS_MINIMIZED", "1"),
                       ("SM64DS_NO_DIALOG", "1"), ("SM64DS_FAULTS_FATAL", "1"), ("TEMP", "W")):
            self.assertEqual(e[k], val)


def stream(pairs):
    return OrderedDict(pairs)


class TestCompare(Tmp):
    def test_parse_streams(self):
        out = ("[scene-ramhash] f0 aaaaaaaa 11111111\n[scene-ramhash] f1 aaaaaaaa 22222222\n"
               "[scene-ramhash] f1 aaaaaaaa 33333333\n[pichash] t0 0000abcd\njunk\n")
        s = compare.parse_streams(out, "5 deadbeef\n6 DEADBEEF\n")
        self.assertEqual(list(s["arena"].items()), [("f0#0", "11111111"), ("f1#0", "22222222"),
                                                     ("f1#1", "33333333")])
        self.assertEqual(s["pic"]["t0"], "0000abcd")
        self.assertEqual(s["pic"]["s6"], "deadbeef")
        self.assertEqual(compare.key_frame("f12#1"), 12)

    def test_same_and_first_diff(self):
        o = stream([("f0#0", "a"), ("f1#0", "b"), ("f2#0", "c"), ("f3#0", "d")])
        r = compare.evaluate_field([o, o], [o, o], 0.5)
        self.assertEqual(r["status"], "same")
        self.assertEqual(r["compared"], 4)
        n = stream([("f0#0", "a"), ("f1#0", "b"), ("f2#0", "X"), ("f3#0", "Y")])
        r = compare.evaluate_field([o, o], [n, n], 0.5)
        self.assertEqual(r["first_diff"], ("f2#0", "c", "X"))

    def test_noise_floor_excludes(self):
        a = stream([("f0#0", "a"), ("f1#0", "N1"), ("f2#0", "c"), ("f3#0", "d")])
        b = stream([("f0#0", "a"), ("f1#0", "N2"), ("f2#0", "c"), ("f3#0", "d")])
        n = stream([("f0#0", "a"), ("f1#0", "N3"), ("f2#0", "c"), ("f3#0", "d")])
        r = compare.evaluate_field([a, b], [n, n], 0.5)
        self.assertEqual(r["status"], "same")            # f1 is noise: not compared
        self.assertEqual(r["old_unstable"], ["f1#0"])
        self.assertEqual(r["compared"], 3)
        # mostly unstable -> whole field excluded
        c = stream([("f0#0", "z"), ("f1#0", "y"), ("f2#0", "x"), ("f3#0", "d")])
        r = compare.evaluate_field([a, c], [n], 0.5)
        self.assertEqual(r["status"], "unstable")

    def test_new_only_instability_and_missing(self):
        o = stream([("f0#0", "a"), ("f1#0", "b"), ("f2#0", "c")])
        n1 = stream([("f0#0", "a"), ("f1#0", "q"), ("f2#0", "c")])
        n2 = stream([("f0#0", "a"), ("f1#0", "r"), ("f2#0", "c")])
        r = compare.evaluate_field([o, o], [n1, n2], 0.5)
        self.assertEqual(r["status"], "same")
        self.assertEqual(r["new_only_unstable"], ["f1#0"])
        short = stream([("f0#0", "a")])
        r = compare.evaluate_field([o, o], [short, short], 0.5)
        self.assertEqual(r["first_diff"], ("f1#0", "b", "MISSING"))
        longer = stream(list(o.items()) + [("f3#0", "z")])
        r = compare.evaluate_field([o, o], [longer, longer], 0.5)
        self.assertEqual(r["first_diff"], ("f3#0", "MISSING", "z"))
        r = compare.evaluate_field([OrderedDict(), OrderedDict()], [o], 0.5)
        self.assertEqual(r["status"], "unavailable")

    def test_finish_row(self):
        o = stream([("f0#0", "a"), ("f1#0", "b")])
        n = stream([("f0#0", "a"), ("f1#0", "X")])
        mk = lambda a, p: {"pic": p, "main": a, "arena": a}
        ev = compare.evaluate([mk(o, OrderedDict())] * 2, [mk(n, OrderedDict())] * 2, 0.5)
        row = compare.finish_row({"route": "r", "status": C.PASS, "first_diff": "-", "excluded": "-",
                                  "notes": []}, ev, {"aa"}, {"aa"})
        self.assertEqual(row["status"], C.FAIL)
        self.assertIn("main f1#0 (old b new X)", row["first_diff"])
        ev = compare.evaluate([mk(o, OrderedDict())] * 2, [mk(o, OrderedDict())] * 2, 0.5)
        row = compare.finish_row({"route": "r", "status": C.PASS, "first_diff": "-", "excluded": "-",
                                  "notes": []}, ev, {"aa"}, {"bb"})
        self.assertEqual(row["status"], C.FAIL)
        self.assertIn("final picture", row["first_diff"])
        row = compare.finish_row({"route": "r", "status": C.PASS, "first_diff": "-", "excluded": "-",
                                  "notes": ["WEAK: x"]}, ev, {"aa"}, {"aa"})
        self.assertEqual(row["status"], C.WARN)
        s = compare.summarize([row, {"route": "z", "status": C.SKIP, "first_diff": "-", "excluded": "-",
                                     "notes": ["harness lacks it"]}])
        self.assertEqual(s.status, C.WARN)

    def test_mask(self):
        def blob(ws):
            return struct.pack("<%dI" % len(ws), *ws)
        # word 1 = noise (clock), word 2 = vtable pointer that moved, word 3 = real, word 4 = host ptr
        o1 = blob([7, 100, 0x10002010, 5, 0x00650000])
        o2 = blob([7, 200, 0x10002010, 5, 0x00650000])
        n1 = blob([7, 300, 0x10003010, 6, 0x00770000])
        n2 = blob([7, 300, 0x10003010, 6, 0x00770000])
        self.assertEqual(compare.diff_words(o1, o2), [1])
        self.assertEqual(compare.noise_words([o1, o2]), {1})

        class FakeMap(object):
            def __init__(self, at):
                self.at = at

            def symbolize(self, v):
                return ("??_7Goomba@@6B@", v - self.at) if v >= self.at else None
        oi = compare.Image(0x10000000, 0x11000000, FakeMap(0x10002000))
        ni = compare.Image(0x10000000, 0x11000000, FakeMap(0x10003000))
        cls = compare.classify([2, 3, 4], compare.words(o1), compare.words(n1), oi, ni)
        self.assertEqual(cls, {2: "reloc", 3: "real", 4: "hostptr"})
        notes = []
        mask, real = compare.build_mask([o1, o2], [n1, n2], oi, ni, False, notes, "f10")
        self.assertEqual(mask, {1, 2})
        self.assertEqual(len(real), 2)
        self.assertIn("arena+0x00000c", real[0])
        self.assertIn("noise 1 words", notes[0])
        mask, real = compare.build_mask([o1, o2], [n1, n2], oi, ni, True, [], "f10")
        self.assertEqual(mask, {1, 2, 4})
        p = os.path.join(self.d, "m.mask")
        self.assertEqual(compare.write_mask(p, [5, 1, 5, 3]), 3)
        self.assertEqual(compare.read_mask(p), [1, 3, 5])
        with open(p, "rb") as f:
            self.assertEqual(f.read(), struct.pack("<3I", 1, 3, 5))

    def test_routes_and_env(self):
        cfg = {"compare": {"routes": {"lvl8": {"enabled": False}, "pole_swing": {"env": {"SM64DS_SPAWN": "1,2,3"}},
                                      "extra1": {"env": {"SM64DS_LEVEL": "4"}, "frames": 300}}}}
        rts = compare.routes(cfg)
        self.assertFalse(rts["lvl8"]["enabled"])
        self.assertEqual(rts["pole_swing"]["env"]["SM64DS_SPAWN"], "1,2,3")
        self.assertEqual(compare.ROUTES["pole_swing"]["env"]["SM64DS_SPAWN"], "100,2100,3680")  # untouched
        self.assertIn("extra1", rts)
        for name in ("title_fileselect", "castle_grounds", "lvl6", "lvl8", "minigame_slot", "star_select",
                     "pole_swing", "opening", "ending"):
            self.assertIn(name, rts)
        env = compare.route_env(rts["minigame_slot"], os.path.join("W", "row"), {"dump": 300, "mask": "M"})
        self.assertEqual(env["SM64DS_SCENE_FRAMES"], "600")
        self.assertEqual(env["SM64DS_WINDOW_SELFTEST"], "600")
        self.assertTrue(env["SM64DS_SCENE_RAMDUMP"].startswith("300:"))
        self.assertEqual(env["SM64DS_SCENE_RAMMASK"], "M")
        self.assertEqual(env["SM64DS_SCENE_RAMHASH"], "1")
        env = compare.route_env(rts["opening"], "W", {"pic": False})
        self.assertNotIn("SM64DS_SCENE_PICHASH", env)
        self.assertIn("SM64DS_SAVE_PATH", env)

    def test_markers(self):
        class R(object):
            def __init__(self, out):
                self.out = out
        route = compare.ROUTES["pole_swing"]
        notes = compare.marker_notes(route, [("old", R("  [state] 0x020cb5bc y=1.0\n"), None),
                                             ("new", R("  [state] 0x020cb5bc y=1.0\n"), None)])
        self.assertEqual(notes, ["states entered OLD St_Climb_Main NEW St_Climb_Main"])
        notes = compare.marker_notes(route, [("old", R(""), None)])
        self.assertTrue(notes[0].startswith("WEAK"))
        notes = compare.marker_notes(compare.ROUTES["opening"], [("old", R("nothing"), None)])
        self.assertTrue(notes[0].startswith("WEAK"))

    def test_exe_supports(self):
        p = write(os.path.join(self.d, "fake.exe"), b"MZ...SM64DS_SCENE_RAMHASH\0SM64DS_LEVEL\0", "wb")
        self.assertEqual(C.exe_supports(p, ["SM64DS_SCENE_RAMHASH", "SM64DS_LEVEL", "SM64DS_SCENE_RAMMASK"]),
                         ["SM64DS_SCENE_RAMMASK"])
        # SM64DS_LEVEL must not be satisfied by SM64DS_LEVEL_SCALE alone
        p = write(os.path.join(self.d, "f2.exe"), b"SM64DS_LEVEL_SCALE\0", "wb")
        self.assertEqual(C.exe_supports(p, ["SM64DS_LEVEL"]), ["SM64DS_LEVEL"])


FAKE_GAME = r"""
import os, random, struct, sys
variant = sys.argv[1]
frames = int(os.environ["SM64DS_WINDOW_SELFTEST"])
mask = set()
mp = os.environ.get("SM64DS_SCENE_RAMMASK")
if mp:
    data = open(mp, "rb").read()
    mask = set(struct.unpack("<%dI" % (len(data) // 4), data))
dframe = None
d = os.environ.get("SM64DS_SCENE_RAMDUMP")
if d:
    dframe, dpath = int(d.split(":", 1)[0]), d.split(":", 1)[1]
noise = random.getrandbits(32)
pres = os.environ.get("SM64DS_PRESENTER_HASH")
pf = open(pres + ".main", "w") if pres else None
def fnv(ws):
    h = 14695981039346656037
    for w in ws:
        h ^= w
        h = (h * 1099511628211) & 0xffffffffffffffff
    return (h ^ (h >> 32)) & 0xffffffff
for f in range(frames):
    st = f * 3 + (1 if variant == "bad" and f >= 40 else 0)
    arena = [f, noise, 0x10002010, st, 0]
    if f == dframe:
        open(dpath + ".arena", "wb").write(struct.pack("<5I", *arena))
    print("[scene-ramhash] f%d %08x %08x" % (f, 0x1234, fnv([0 if i in mask else w for i, w in enumerate(arena)])))
    if pf:
        pf.write("%d %08x\n" % (f, fnv([f, 7])))
open("walk_window_selftest.bmp", "wb").write(b"BM" + variant.encode())
"""


@unittest.skipIf(os.name == "nt", "the fake game is a POSIX shell script")
class TestCompareEndToEnd(Tmp):
    """compare_route's whole orchestration (quiet launch, streams, noise floor,
    mask, masked re-run, drill-down) against a fake game process."""

    def fake_exe(self, variant):
        game = write(os.path.join(self.d, "fake_game.py"), FAKE_GAME)
        knobs = b"\0".join(k.encode() for k in compare.STATE_KNOBS + compare.PIC_KNOBS + ["SM64DS_LEVEL"])
        body = ("#!/bin/sh\nexec \"%s\" \"%s\" %s\n" % (sys.executable, game, variant)).encode()
        p = os.path.join(self.d, variant, "walk_window.exe")
        write(p, body + b"\0" + knobs + b"\0", "wb")
        os.chmod(p, 0o755)
        return p

    def cfg(self):
        return C._merge(C.CONFIG_TEMPLATE, {
            "work_dir": os.path.join(self.d, "work"), "asset_root": self.d, "tree_root": C.THIS_TREE,
            "runs": {"max_parallel": 2, "timeout_seconds": 60, "foreign_processes": []}})

    def route(self):
        return {"desc": "fake", "frames": 80, "env": {"SM64DS_LEVEL": "1"}}

    def test_same_exe_passes_through_the_mask(self):
        cfg = self.cfg()
        old, new = self.fake_exe("good"), self.fake_exe("good2")
        with redirect_stdout(io.StringIO()):
            row = compare.compare_route(cfg, compare.Runner(cfg, "t"), "fake", self.route(), old, new,
                                        (None, None))
        self.assertEqual(row["status"], C.FAIL, row)      # final picture differs (BMgood vs BMgood2)
        self.assertIn("final picture", row["first_diff"])
        self.assertNotIn("arena", row["first_diff"])
        self.assertTrue(any("masked pass 1: 1 words masked, arena same" in n for n in row["notes"]), row["notes"])
        new = self.fake_exe("good")
        with redirect_stdout(io.StringIO()):
            row = compare.compare_route(cfg, compare.Runner(cfg, "u"), "fake", self.route(), old, new,
                                        (None, None))
        self.assertEqual(row["status"], C.PASS, row)

    def test_real_state_difference_is_found(self):
        cfg = self.cfg()
        old, new = self.fake_exe("good"), self.fake_exe("bad")
        with redirect_stdout(io.StringIO()):
            row = compare.compare_route(cfg, compare.Runner(cfg, "v"), "fake", self.route(), old, new,
                                        (None, None))
        self.assertEqual(row["status"], C.FAIL, row)
        self.assertIn("arena f40#0", row["first_diff"])
        self.assertTrue(any(n.startswith("REAL arena+0x00000c") and "old 0x00000078" in n
                            and "new 0x00000079" in n for n in row["notes"]), row["notes"])

    def test_missing_knob_skips(self):
        cfg = self.cfg()
        old = self.fake_exe("good")
        r = dict(self.route(), env={"SM64DS_NOT_A_KNOB": "1"})
        row = compare.compare_route(cfg, compare.Runner(cfg, "w"), "fake", r, old, old, (None, None))
        self.assertEqual(row["status"], C.SKIP)
        self.assertIn("SM64DS_NOT_A_KNOB", row["notes"][0])


class TestForeignWait(unittest.TestCase):
    def test_waits_until_gone(self):
        seq = [[("walk_window.exe", 999), ("walk_window.exe", 5), ("explorer.exe", 1)],
               [("SM64DSLAUNCHER.EXE", 77)],
               [("walk_window.exe", 5)]]
        calls = {"n": 0}
        slept = []
        said = []

        def lister():
            r = seq[min(calls["n"], len(seq) - 1)]
            calls["n"] += 1
            return r
        clock = {"t": 0.0}

        def sleep(s):
            slept.append(s)
            clock["t"] += s
        w = C.wait_for_foreign({"foreign_processes": ["walk_window.exe", "SM64DSLauncher.exe"],
                                "foreign_poll_seconds": 30}, lister=lister, sleep=sleep,
                               clock=lambda: clock["t"], say=said.append, own={5})
        self.assertEqual(slept, [30, 30])
        self.assertEqual(w, 60)
        self.assertIn("pid 999", said[0])
        self.assertIn("carrying on", said[-1])

    def test_own_pid_is_not_foreign(self):
        slept = []
        w = C.wait_for_foreign({"foreign_processes": ["walk_window.exe"]},
                               lister=lambda: [("walk_window.exe", 5)], sleep=slept.append,
                               say=lambda m: None, own={5})
        self.assertEqual((w >= 0, slept), (True, []))

    def test_gives_up_after_max(self):
        clock = {"t": 0.0}

        def sleep(s):
            clock["t"] += s
        with self.assertRaises(C.ForeignWaitTimeout):
            C.wait_for_foreign({"foreign_processes": ["walk_window.exe"], "foreign_poll_seconds": 10,
                                "foreign_max_wait_seconds": 25},
                               lister=lambda: [("walk_window.exe", 1)], sleep=sleep,
                               clock=lambda: clock["t"], say=lambda m: None, own=set())

    def test_chill(self):
        self.assertTrue(C.chill_active({"chill_processes": ["VRChat.exe"]}, lister=lambda: [("vrchat.exe", 3)]))
        self.assertFalse(C.chill_active({"chill_processes": ["VRChat.exe"]}, lister=lambda: []))


class TestBuild(Tmp):
    CMD = "\r\n".join([
        "@echo off",
        "rem ninja -C \"%~dp0..\\build\\port\"",
        "python \"%~dp0tools\\guardcache.py\" --prime",
        "set \"PORT_NEED_CONFIGURE=\"",
        "ninja -C \"%~dp0..\\build\\port\" build.ninja",
        "ninja -j12 -C \"%~dp0..\\build\\port\"",
        "python \"%~dp0tools\\pmf_guard.py\"",
        ""])

    def test_stage_script(self):
        src = write(os.path.join(self.d, "build-port-k0.cmd"), self.CMD, "w")
        dst = os.path.join(self.d, "build-port-staged.cmd")
        heavy, marks = build.stage_script(src, dst, me="ME.py", py="PY")
        self.assertTrue(heavy)
        self.assertEqual(marks, ["configure", "postlink", "regen", "wave"])
        with open(dst, "rb") as f:
            text = f.read().decode("ascii")
        self.assertIn("\r\n", text)
        self.assertIn('"PY" "ME.py" heavy --root "%~dp0.." -- ninja -C "%~dp0..\\build\\port"', text)
        self.assertNotIn("ninja -j12", text)
        self.assertIn('ninja -C "%~dp0..\\build\\port" build.ninja', text)     # regen kept
        self.assertIn("rem ninja", text)                                        # rem untouched

    def test_probe_and_dry_run(self):
        nin = ("rule x\nbuild build.ninja: RERUN_CMAKE | a\n  pool = console\n"
               "build a.obj: cc a.c\n  FLAGS = 1\nbuild CMakeFiles/cmake.verify_globs: VERIFY_GLOBS\n"
               "  pool = console\nbuild b.obj: cc b.c\n")
        p = build.probe_ninja_text(nin)
        self.assertNotIn("RERUN_CMAKE", p)
        self.assertNotIn("VERIFY_GLOBS", p)
        self.assertIn("build a.obj", p)
        self.assertIn("build b.obj", p)
        self.assertEqual(build.parse_dry_run("ninja: no work to do.\n"), 0)
        self.assertEqual(build.parse_dry_run("[1/283] cc a\n[2/283] cc b\n"), 283)
        self.assertIsNone(build.parse_dry_run("garbage"))

    def test_pools(self):
        old = os.environ.get("SM64DS_STAGE_DIR")
        os.environ["SM64DS_STAGE_DIR"] = os.path.join(self.d, "stage")
        try:
            self.assertEqual(build.pool_dir(), os.path.join(self.d, "stage"))
            a = build.pool_acquire("small", 2, "t1", self.d, timeout=5, poll=0.01)
            b = build.pool_acquire("small", 2, "t2", self.d, timeout=5, poll=0.01)
            self.assertNotEqual(a, b)
            held, _q = build.pool_state("small", 2)
            self.assertEqual(len(held), 2)
            with self.assertRaises(build.bl().BuildLockTimeout):
                build.pool_acquire("small", 2, "t3", self.d, timeout=0.2, poll=0.01)
            build.pool_release(a)
            c = build.pool_acquire("small", 2, "t3", self.d, timeout=5, poll=0.01)
            self.assertEqual(c, a)
            build.pool_release(b)
            build.pool_release(c)
            self.assertEqual(build.pool_state("small", 2)[0], [])
        finally:
            if old is None:
                os.environ.pop("SM64DS_STAGE_DIR", None)
            else:
                os.environ["SM64DS_STAGE_DIR"] = old


class TestRunAll(unittest.TestCase):
    def test_stop_at_first_fail(self):
        seen = []

        def step(name, status):
            def fn(state):
                seen.append(name)
                return C.StepResult(name, status, "%s did %s" % (name, status))
            return (name, fn)
        res = run_all.run_steps([step("build", C.PASS), step("linkage", C.WARN), step("gate", C.FAIL),
                                 step("compare", C.PASS)])
        self.assertEqual(seen, ["build", "linkage", "gate"])
        self.assertEqual([r.status for r in res], [C.PASS, C.WARN, C.FAIL, C.NOTRUN])
        text, rc = run_all.report(res)
        self.assertEqual(rc, 1)
        self.assertIn("OVERALL: FAIL at gate", text)
        self.assertIn("NOT RUN", text)
        self.assertIn("stopped after gate failed", text)

    def test_all_pass_and_crash(self):
        ok = lambda n: (n, lambda st: C.StepResult(n, C.PASS, "fine"))
        text, rc = run_all.report(run_all.run_steps([ok("build"), ok("linkage")]))
        self.assertEqual(rc, 0)
        self.assertIn("OVERALL: PASS", text)

        def boom(st):
            raise RuntimeError("kaput")
        res = run_all.run_steps([ok("build"), ("linkage", boom), ok("gate")])
        self.assertEqual([r.status for r in res], [C.PASS, C.ERROR, C.NOTRUN])
        self.assertIn("kaput", res[1].summary)

    def test_state_passes_between_steps(self):
        def b(st):
            st["exe"] = "X"
            return C.StepResult("build", C.SKIP, "given")

        def l(st):
            return C.StepResult("linkage", C.PASS, st["exe"])
        res = run_all.run_steps([("build", b), ("linkage", l)])
        self.assertEqual(res[1].summary, "X")


class TestHygiene(unittest.TestCase):
    def test_ascii_and_no_local_paths(self):
        bad = []
        for fn in sorted(os.listdir(HERE)):
            if not fn.endswith((".py", ".md", ".gitignore")) and fn != ".gitignore":
                continue
            with open(os.path.join(HERE, fn), "rb") as f:
                raw = f.read()
            if any(b > 127 for b in raw):
                bad.append(fn + ": non-ASCII")
            low = raw.lower()
            # spelled split so this file does not match itself
            for needle in (":|\\users", ":|/users", "c:|/tmp", "c:|\\tmp", "app|data", "bma|nu", "/ho|me/"):
                needle = needle.replace("|", "").encode()
                if needle in low:
                    bad.append("%s: contains %s" % (fn, needle.decode()))
        self.assertEqual(bad, [])


def main():
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    r = unittest.TextTestRunner(verbosity=2).run(suite)
    print("PORTSYNC SELFTEST: %s (%d tests, %d failures, %d errors)" % (
        "PASS" if r.wasSuccessful() else "FAIL", r.testsRun, len(r.failures), len(r.errors)))
    return 0 if r.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
