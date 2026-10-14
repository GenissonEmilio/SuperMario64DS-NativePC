#!/usr/bin/env python3
"""Tests for queue_audit's --check-promoted arm -- the one that runs in CI.

The point of these is the FAILURE branches. A gate that only proves it can go
green cannot be told apart from `exit 0`, and this one exists precisely because
an unwatched column went stale for four days and cost an agent.

Each test builds a throwaway tree and repoints the module's REPO/QUEUE globals at
it, so nothing here reads or writes the real repository.
"""
import contextlib
import io as _io
import json
import pathlib
import sys
import tempfile
import unittest

from tools import queue_audit

COLS = ["class_name", "shard_count", "total_lines", "overlay",
        "already_promoted", "blockers"]
HEADER = "\t".join(COLS) + "\n"


@contextlib.contextmanager
def tree(rows, manifests):
    """A minimal repo: config/tu_manifest.d/<ov>/<Class>.json plus a queue TSV."""
    with tempfile.TemporaryDirectory() as d:
        root = pathlib.Path(d)
        for ov, cls, status in manifests:
            p = root / "config" / "tu_manifest.d" / ov
            p.mkdir(parents=True, exist_ok=True)
            (p / (cls + ".json")).write_text(
                json.dumps({"id": "%s/%s" % (ov, cls), "status": status}),
                encoding="utf-8")
        q = root / "notes" / "data" / "tu-promotion-queue.tsv"
        q.parent.mkdir(parents=True, exist_ok=True)
        q.write_text(HEADER + "".join(rows), encoding="utf-8")
        old_repo, old_queue = queue_audit.REPO, queue_audit.QUEUE
        queue_audit.REPO, queue_audit.QUEUE = root, q
        try:
            yield root
        finally:
            queue_audit.REPO, queue_audit.QUEUE = old_repo, old_queue


def run():
    """Return (exit_code, stdout) for one --check-promoted invocation."""
    buf = _io.StringIO()
    with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(_io.StringIO()):
        rc = queue_audit.check_promoted_only()
    return rc, buf.getvalue()


def row(cls, ov, promoted):
    return "\t".join([cls, "1", "10", ov, promoted, "-"]) + "\n"


class CheckPromoted(unittest.TestCase):

    def test_agreeing_queue_passes(self):
        with tree([row("daFoo_c", "ov001", "yes"), row("daBar_c", "ov001", "no")],
                  [("ov001", "daFoo_c", "promoted"), ("ov001", "daBar_c", "draft")]):
            rc, out = run()
        self.assertEqual(rc, 0, out)
        self.assertIn("agrees with", out)

    def test_promoted_class_the_queue_calls_unpromoted_FAILS(self):
        """The exact shape that cost an agent: manifest promoted, queue says no."""
        with tree([row("daObjTh_Fall_Block_c", "ov063", "no")],
                  [("ov063", "daObjTh_Fall_Block_c", "promoted")]):
            rc, out = run()
        self.assertEqual(rc, 1, out)
        self.assertIn("daObjTh_Fall_Block_c", out)
        self.assertIn("is promoted but the queue says no", out)

    def test_unpromoted_class_the_queue_calls_promoted_FAILS(self):
        """The opposite direction must fail too, or the gate is one-sided."""
        with tree([row("daFoo_c", "ov001", "yes")],
                  [("ov001", "daFoo_c", "draft")]):
            rc, out = run()
        self.assertEqual(rc, 1, out)
        self.assertIn("is NOT promoted but the queue says yes", out)

    def test_multi_class_row_needs_EVERY_class_promoted(self):
        """`A+B` rows are one TU: a half-promoted pair is not promoted."""
        with tree([row("daFoo_c+daBar_c", "ov001", "yes")],
                  [("ov001", "daFoo_c", "promoted"), ("ov001", "daBar_c", "draft")]):
            rc, out = run()
        self.assertEqual(rc, 1, out)

    def test_rows_making_no_claim_are_skipped_not_failed(self):
        """UNATTRIBUTED carries '-': an aggregate count, not a class.

        Auditing it would red the gate permanently on a correct row, which is how
        a gate gets switched off.
        """
        with tree([row("UNATTRIBUTED", "(all overlays)", "-"),
                   row("daFoo_c", "ov001", "yes")],
                  [("ov001", "daFoo_c", "promoted")]):
            rc, out = run()
        self.assertEqual(rc, 0, out)
        self.assertIn("1 row(s) make no promotion claim", out)

    def test_comment_rows_are_not_audited(self):
        with tree([row("# a note row", "", ""), row("daFoo_c", "ov001", "yes")],
                  [("ov001", "daFoo_c", "promoted")]):
            rc, out = run()
        self.assertEqual(rc, 0, out)

    def test_no_manifest_entries_is_NOT_a_pass(self):
        with tree([row("daFoo_c", "ov001", "no")], []):
            rc, out = run()
        self.assertEqual(rc, 2, out)

    def test_meta_json_is_not_counted_as_an_entry(self):
        with tree([row("daFoo_c", "ov001", "no")], []) as root:
            p = root / "config" / "tu_manifest.d" / "ov001"
            p.mkdir(parents=True, exist_ok=True)
            (p / "_meta.json").write_text("{}", encoding="utf-8")
            rc, out = run()
        self.assertEqual(rc, 2, out)

    def test_queue_with_no_data_rows_is_NOT_a_pass(self):
        with tree([], [("ov001", "daFoo_c", "promoted")]):
            rc, out = run()
        self.assertEqual(rc, 2, out)

    def test_queue_where_no_row_makes_a_claim_is_NOT_a_pass(self):
        with tree([row("UNATTRIBUTED", "(all overlays)", "-")],
                  [("ov001", "daFoo_c", "promoted")]):
            rc, out = run()
        self.assertEqual(rc, 2, out)

    def test_output_names_what_it_does_not_check(self):
        """A green run must not read as the full audit."""
        with tree([row("daFoo_c", "ov001", "yes")],
                  [("ov001", "daFoo_c", "promoted")]):
            rc, out = run()
        self.assertEqual(rc, 0, out)
        self.assertIn("already_promoted ONLY", out)
        self.assertIn("NOT checked here", out)

    def test_needs_no_ROM_derived_input(self):
        """The whole reason this arm exists: build/ is absent on a CI runner."""
        with tree([row("daFoo_c", "ov001", "yes")],
                  [("ov001", "daFoo_c", "promoted")]) as root:
            self.assertFalse((root / "build").exists())
            rc, out = run()
        self.assertEqual(rc, 0, out)


SCUTTLE = frozenset({"Scuttlebug", "daSpd_c"})
POKEY = frozenset({"Pokey", "daSanbo_c"})


class FactorySpellings(unittest.TestCase):
    """The run extends over a zero-gap factory under either class spelling.

    srcpath.class_of cannot see `_classInit` (`_SPAWN_RE` is `^(\\w+)_Spawn$`),
    and the queue row is often the coined name while the symbol is the ROM one.
    """

    def test_class_of_does_not_see_classInit(self):
        # srcpath imports `relocs` as a sibling, the same way its own tests do.
        tools_dir = str(pathlib.Path(__file__).resolve().parent)
        sys.path.insert(0, tools_dir)
        try:
            import srcpath
        finally:
            sys.path.remove(tools_dir)
        self.assertIsNone(srcpath.class_of("daYurei_Mucho_c_classInit"))
        self.assertIsNone(srcpath.class_of("daSpd_c_classInit"))
        self.assertEqual(srcpath._SPAWN_RE.pattern, r"^(\w+)_Spawn$")

    def test_factory_stem_keeps_a_profile_suffix_off_the_class(self):
        self.assertEqual(queue_audit.factory_stem("daSanbo_c_classInit_SANBO_BODY"),
                         "daSanbo_c")
        self.assertEqual(queue_audit.factory_stem("daNknk_c_classInit_NOKONOKO_S"),
                         "daNknk_c")
        self.assertEqual(queue_audit.factory_stem("Scuttlebug_Spawn"), "Scuttlebug")
        self.assertIsNone(queue_audit.factory_stem(
            "_ZN10Scuttlebug13OnTurnIntoEggER6Player"))

    def test_rtti_vtable_joins_the_coined_symbol(self):
        """Eyerok's vtable symbol is coined; the ROM record names daIwante_c."""
        vt = {("ov066", 0x0211ad64): {"Eyerok"}}
        rom = {("ov066", 0x0211ad64): {"daIwante_c"}}
        aliases = queue_audit.class_aliases(vt, rom)
        self.assertEqual(aliases["Eyerok"], frozenset({"Eyerok", "daIwante_c"}))
        self.assertEqual(aliases["daIwante_c"], aliases["Eyerok"])

    def test_colocated_vtables_are_one_class(self):
        vt = {("ov071", 0x02122c2c): {"Scuttlebug", "daSpd_c"}}
        aliases = queue_audit.class_aliases(vt, {})
        self.assertEqual(aliases["Scuttlebug"], SCUTTLE)

    def test_a_shared_address_in_two_modules_is_not_one_class(self):
        vt = {("ov001", 0x1000): {"Aaa"}, ("ov002", 0x1000): {"Bbb"}}
        rom = {("ov001", 0x1000): {"RomA"}, ("ov002", 0x1000): {"RomB"}}
        aliases = queue_audit.class_aliases(vt, rom)
        self.assertEqual(aliases["Aaa"], frozenset({"Aaa", "RomA"}))
        self.assertEqual(aliases["Bbb"], frozenset({"Bbb", "RomB"}))
        self.assertTrue(aliases["Aaa"].isdisjoint(aliases["Bbb"]))

    def test_two_rom_names_on_one_vtable_are_not_joined(self):
        vt = {("ov001", 0x1000): {"Coined", "RomA"}}
        rom = {("ov001", 0x1000): {"RomA", "RomB"}}
        aliases = queue_audit.class_aliases(vt, rom)
        self.assertEqual(aliases["Coined"], frozenset({"Coined", "RomA"}))
        self.assertNotIn("RomB", aliases)

    def test_rom_factory_extends_a_coined_row(self):
        syms = [
            (0x1000, 0x10, "_ZN10Scuttlebug13OnTurnIntoEggER6Player"),
            (0x1010, 0x50, "daSpd_c_classInit"),
            (0x1060, 0x48, "_ZN8daEykn_cD1Ev"),
        ]
        start, end, absorbed = queue_audit.extend_over_factories(
            syms, ["Scuttlebug"], 0x1000, 0x1010,
            {"daSpd_c": SCUTTLE, "Scuttlebug": SCUTTLE})
        self.assertEqual(absorbed, ["daSpd_c_classInit"])
        self.assertEqual((start, end), (0x1000, 0x1060))

    def test_same_spelling_still_extends_without_an_alias(self):
        syms = [
            (0x1000, 0x10, "_ZN15daYurei_Mucho_c13OnYoshiTryEatEv"),
            (0x1010, 0x50, "daYurei_Mucho_c_classInit"),
        ]
        start, end, absorbed = queue_audit.extend_over_factories(
            syms, ["daYurei_Mucho_c"], 0x1000, 0x1010, {})
        self.assertEqual(absorbed, ["daYurei_Mucho_c_classInit"])
        self.assertEqual(end, 0x1060)

    def test_profile_suffix_chain_uses_the_other_spelling(self):
        syms = [
            (0x2000, 0x20, "_ZN9daSanbo_c16OnAimedAtWithEggEv"),
            (0x2020, 0x50, "daSanbo_c_classInit_SANBO_BODY"),
            (0x2070, 0x50, "daSanbo_c_classInit_SANBO"),
        ]
        start, end, absorbed = queue_audit.extend_over_factories(
            syms, ["Pokey"], 0x2000, 0x2020,
            {"Pokey": POKEY, "daSanbo_c": POKEY})
        self.assertEqual(absorbed, [
            "daSanbo_c_classInit_SANBO_BODY", "daSanbo_c_classInit_SANBO"])
        self.assertEqual(end, 0x20c0)

    def test_spawn_spelling_extends_a_rom_keyed_row(self):
        syms = [(0x3000, 0x30, "Scuttlebug_Spawn")]
        start, end, absorbed = queue_audit.extend_over_factories(
            syms, ["daSpd_c"], 0x3030, 0x3100,
            {"Scuttlebug": SCUTTLE, "daSpd_c": SCUTTLE})
        self.assertEqual(start, 0x3000)
        self.assertEqual(end, 0x3100)
        self.assertEqual(absorbed, ["Scuttlebug_Spawn"])

    def test_a_gap_is_not_absorbed(self):
        syms = [(0x1010, 0x50, "daSpd_c_classInit")]
        start, end, absorbed = queue_audit.extend_over_factories(
            syms, ["Scuttlebug"], 0x1000, 0x1008,
            {"daSpd_c": SCUTTLE, "Scuttlebug": SCUTTLE})
        self.assertEqual(absorbed, [])
        self.assertEqual((start, end), (0x1000, 0x1008))

    def test_the_next_class_factory_is_not_pulled_in(self):
        syms = [
            (0x1010, 0x50, "daSpd_c_classInit"),
            (0x1060, 0x48, "daEykn_c_classInit"),
        ]
        start, end, absorbed = queue_audit.extend_over_factories(
            syms, ["Scuttlebug"], 0x1000, 0x1010,
            {"daSpd_c": SCUTTLE, "Scuttlebug": SCUTTLE})
        self.assertEqual(absorbed, ["daSpd_c_classInit"])
        self.assertEqual(end, 0x1060)

    def test_a_factory_already_inside_the_run_is_not_recounted(self):
        syms = [(0x1004, 0x10, "daYurei_Mucho_c_classInit")]
        start, end, absorbed = queue_audit.extend_over_factories(
            syms, ["daYurei_Mucho_c"], 0x1000, 0x1020, {})
        self.assertEqual(absorbed, [])
        self.assertEqual((start, end), (0x1000, 0x1020))


if __name__ == "__main__":
    unittest.main()
