from pathlib import Path

import facts_tool
from facts_tool.catalog import RELATION_NAMES

ROOT = Path(__file__).parents[1]


def test_runtime_exports_no_indexing_or_mutation_api() -> None:
    forbidden = {
        "index",
        "import_project",
        "extract",
        "resolve",
        "migrate",
        "backfill",
        "write",
        "save",
    }
    assert forbidden.isdisjoint(facts_tool.__all__)


def test_relation_catalog_is_complete_and_unique() -> None:
    assert len(RELATION_NAMES) == len(set(RELATION_NAMES)) == 23


def test_all_hand_authored_files_have_at_most_100_lines() -> None:
    completed = __import__("subprocess").run(
        [__import__("sys").executable, str(ROOT / "scripts/check_file_sizes.py")],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    assert completed.returncode == 0, completed.stdout + completed.stderr


def test_runtime_import_graph_has_no_cycles() -> None:
    completed = __import__("subprocess").run(
        [
            __import__("sys").executable,
            str(ROOT / "scripts/check_import_boundaries.py"),
        ],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    assert completed.returncode == 0, completed.stdout + completed.stderr
