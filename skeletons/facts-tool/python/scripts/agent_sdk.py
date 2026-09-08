from __future__ import annotations

from pathlib import Path

import facts_tool
from facts_tool import open_codebase


def run_sdk(
    native_facts: Path,
    native_project: Path,
    schema_facts: Path,
    schema_project: Path,
) -> int:
    assert "site-packages" in Path(facts_tool.__file__).parts
    with open_codebase(facts_db=native_facts, project_db=native_project) as cb:
        run = cb.callgraphs.latest()
        assert run is not None and run.status == "complete"
        assert any(
            edge.source.qualified_name == "app::run"
            and edge.target.qualified_name == "app::save"
            for edge in run.edges
        )
        assert [item.qualified_name for item in cb.find("app::run").callees()] == [
            "app::save"
        ]
        assert [item.qualified_name for item in cb.find("app::save").callers()] == [
            "app::run"
        ]
        assert [item.qualified_name for item in cb.find("app::Box").bases()] == [
            "app::Base"
        ]
        queries = 4
    with open_codebase(facts_db=schema_facts, project_db=schema_project) as cb:
        assert cb.expression_occurrences("app::run").rows
        assert cb.field_writers("app::Box::value").rows
        assert [item.qualified_name for item in cb.ancestors("app::Box")] == [
            "app::Base"
        ]
        for ref in ("app::run", "app::Box::flush()", "app::Box"):
            section = cb.definition_regions(ref, include_text=True, max_bytes=32000)
            assert section.rows and section.rows[0]["text"]
            queries += 1
        queries += 3
    return queries
