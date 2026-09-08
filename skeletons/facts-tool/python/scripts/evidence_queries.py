import shutil
import sqlite3
from contextlib import suppress
from dataclasses import FrozenInstanceError
from pathlib import Path

from evidence_facades import assert_facade_matrix
from evidence_provenance import assert_provenance_outcomes

from facts_tool import FactsToolError, open_codebase


def run_evidence(facts: Path, project: Path) -> None:
    with open_codebase(facts_db=facts, project_db=project) as cb:
        source = Path(
            cb.executor.loader.facts.execute(
                "SELECT path FROM facts_project_provenance WHERE file_id=1"
            ).fetchone()[0]
        )
        original = source.read_bytes()
        expressions = cb.evidence.expressions()
        assert_facade_matrix(cb)
        assert cb.expressions().rows == expressions.rows
        assert cb.graph.expressions().rows == expressions.rows
        assert {row["access"] for row in expressions} >= {
            "read",
            "write",
            "read_write",
            "escape",
            "none",
            "unknown",
        }
        assert all(
            row["owner"] == "app::run" and row["target"] == "app::Box::value"
            for row in expressions
        )
        assert all(row["file_id"] == 1 and row["file"] for row in expressions)
        for access in ("read", "read_write", "escape", "none"):
            assert cb.field_accesses("app::Box::value", access=access).rows
        assert (
            cb.field_writers("app::Box::value").rows
            == cb.field_writes("app::Box::value").rows
        )
        assert [item.qualified_name for item in cb.graph.ancestors("app::Box")] == [
            "app::Base"
        ]
        page_ids, page = [], cb.expression_occurrences(limit=2)
        while True:
            page_ids.extend(row["id"] for row in page)
            if not page.truncated:
                break
            page = cb.expression_occurrences(after_id=page.cursor, limit=2)
        assert page_ids == [row["id"] for row in expressions]
        try:
            cb.expression_occurrences(owner="app::run")
        except FactsToolError as error:
            assert error.code == "E_IDENTITY"
        else:
            raise AssertionError("ambiguous owner must be typed")
        with suppress(FrozenInstanceError):
            expressions.truncated = True
        assert expressions.truncated is False
        assert expressions.provenance.facts.schema.user_version == 13
        first = cb.source_sections(include_text=True, max_bytes=30, limit=1)
        assert first.truncated and first.rows[0]["text"]
        try:
            cb.source_sections(include_text=True, max_bytes=30, after_id=1)
        except FactsToolError as error:
            assert error.code == "E_LIMIT"
        else:
            raise AssertionError("excluded oversized source must fail only on its page")
        assert {row["symbol_kind"] for row in cb.source_regions(after_id=1)} >= {
            "declaration",
            "macro",
            "implicit",
        }
        invalid = cb.source_regions(include_text=True, after_id=5)
        assert invalid.unknown and invalid.rows[0]["freshness"] == "unavailable"
        source.write_text("changed\n", encoding="utf-8")
        assert cb.expression_occurrences().partial
        source.unlink()
        assert cb.expression_occurrences().unknown
        source.write_bytes(original)
        alternate = source.parent.parent / "alternate-checkout" / "src"
        alternate.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, alternate / "main.cpp")
        cb.close()
    with sqlite3.connect(project) as database:
        database.execute("UPDATE clone SET path=? WHERE id=1", (str(alternate.parent),))
    with open_codebase(facts_db=facts, project_db=project) as switched:
        assert switched.source_regions(include_text=True).unknown
        assert switched.expression_occurrences().unknown
    assert_provenance_outcomes(facts, project, source)
