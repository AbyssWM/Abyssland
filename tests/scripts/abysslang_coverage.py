#!/usr/bin/env python3
"""Coverage regressions; run with python3 tests/scripts/abysslang_coverage.py."""
import importlib.util
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("coverage", ROOT / "scripts/abysslang-coverage.py")
coverage = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(coverage)


class GroupLockCoverage(unittest.TestCase):
    def test_baseline_global_lock_is_preserved(self):
        items = []
        coverage.check_mapping("dispatchers", ["group.lock"], coverage.DISPATCHERS,
                               "Action lockGroups(std::string mode);", items)
        self.assertEqual(items[0].status, "ok")

    def test_new_overloads_are_not_claimed_by_global_symbol(self):
        items = []
        coverage.check_mapping("dispatchers", ["group.lock[all]", "group.lock[?]"],
                               coverage.DISPATCHERS, "Action lockGroups(std::string mode);", items)
        self.assertEqual([item.status for item in items], ["missing", "missing"])

    def test_baseline_marks_new_overloads_as_newer(self):
        if coverage.git(str(ROOT), "rev-parse", "--verify", "v0.56.2^{commit}") is None:
            self.skipTest("upstream tag v0.56.2 is not available")
        items = [coverage.Item("dispatchers", key, "missing")
                 for key in ("group.lock[all]", "group.lock[?]")]
        self.assertTrue(coverage.mark_newer_tag(items, str(ROOT), "v0.56.2"))
        self.assertEqual([item.status for item in items], ["newer", "newer"])

    def test_new_signature_is_not_marked_newer(self):
        source = '''static int hlGroupLock(lua_State* L) {
    Internal::tableOptBool(L, 1, "all");
    Internal::pushWindowUpval(L, 1);
}
'''
        with patch.object(coverage, "git", side_effect=lambda *args: source if "show" in args else "commit"):
            for key in ("group.lock[all]", "group.lock[?]"):
                self.assertTrue(coverage.tag_has(str(ROOT), "new-tag", coverage.Item("dispatchers", key, "missing")))


if __name__ == "__main__":
    unittest.main()
