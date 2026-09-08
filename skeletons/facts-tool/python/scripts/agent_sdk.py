from __future__ import annotations

from pathlib import Path

from agent_queries import forwarding, names, regions

import facts_tool
from facts_tool import open_codebase

METHODS = (
    "expression_occurrences",
    "expressions",
    "field_accesses",
    "field_writers",
    "field_writes",
    "source_regions",
    "source_sections",
    "definition_regions",
)


def run_sdk(
    facts: Path, project: Path, run_id: int, setup_calls: int, setup_output_chars: int
) -> int:
    package_path = str(Path(facts_tool.__file__).resolve())
    assert "site-packages" in package_path
    lines = [f"package: installed site-packages ({package_path})"]
    with open_codebase(facts_db=facts, project_db=project) as cb:
        before = tuple(item.run_id for item in cb.callgraphs.list())
        graph = cb.callgraphs.get(run_id)
        assert graph.status == "complete"
        assert any(
            edge.source.qualified_name == "app::run"
            and edge.target.qualified_name == "app::save"
            for edge in graph.edges
        )
        lines.append(f"callgraphs.get({run_id}): complete, app::run -> app::save")
        lines.append(
            f"symbols: callees={names(cb.find('app::run').callees())}, "
            f"callers={names(cb.find('app::save').callers())}"
        )
        expressions = cb.expression_occurrences(owner="app::run")
        assert expressions.rows
        target = next(
            row["target"]
            for row in expressions.rows
            if row["target"].endswith("::value")
        )
        lines.append(
            f"expressions: {len(expressions.rows)} rows, "
            f"partial={expressions.partial}, unknown={expressions.unknown}"
        )
        writers = cb.field_writers(target)
        assert writers.rows
        lines.append(f"field writers: {len(writers.rows)} direct write/read-write rows")
        regions(cb, lines)
        for facade in (cb, cb.evidence, cb.graph):
            for name in METHODS:
                assert forwarding(facade, name, target) is not None
                lines.append(f"{type(facade).__name__}.{name}: public forwarding call")
        for facade in (cb, cb.evidence, cb.graph):
            try:
                lines.append(
                    f"{type(facade).__name__}.ancestors: "
                    f"{names(facade.ancestors('app::Box'))}"
                )
            except facts_tool.FactsToolError as exc:
                assert exc.code in {"E_IDENTITY", "E_SOURCE"}
                lines.append(
                    f"{type(facade).__name__}.ancestors: typed {exc.code} outcome"
                )
        after = tuple(item.run_id for item in cb.callgraphs.list())
        assert after == before and run_id in after
        lines.append(f"call graph reuse: run ids unchanged {list(after)}")
    for line in lines:
        print(line)
    chars = sum(len(line) + 1 for line in lines)
    print(
        f"acceptance metrics: setup_calls={setup_calls}, sdk_queries={len(lines)}, "
        f"native_output_chars={setup_output_chars}, sdk_output_chars={chars}, "
        f"sdk_output_tokens_estimate={chars // 4}"
    )
    return len(lines)
