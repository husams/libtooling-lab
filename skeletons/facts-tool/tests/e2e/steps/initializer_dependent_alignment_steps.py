from __future__ import annotations

import json
from pathlib import Path

from pytest_bdd import given, then, when
from steps.b019_extraction_completeness_steps import extract_once
from support.database import query, require
from support.scenario import FactsToolContext


@given(
    "a compile database for the dependent-alignment fixture",
    target_fixture="dependent_alignment_fixture",
)
def given_dependent_alignment_compile_database(
    context: FactsToolContext,
) -> Path:
    context.prepare()
    source = (
        context.fixture_root / "initializer_dependent_alignment.cpp"
    ).resolve(strict=True)
    context.facts_database = context.run_root_path / "b020-facts.sqlite"
    context.files_database = context.run_root_path / "b020-project.sqlite"
    (context.run_root_path / "compile_commands.json").write_text(
        json.dumps(
            [
                {
                    "directory": str(context.fixture_root),
                    "file": str(source),
                    "arguments": [
                        str(context.compiler),
                        "-std=c++17",
                        "-c",
                        str(source),
                    ],
                }
            ],
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return source


@when("the real extraction command indexes the dependent-alignment fixture")
def when_extract_indexes_dependent_alignment(
    context: FactsToolContext, dependent_alignment_fixture: Path
) -> None:
    extract_once(context, dependent_alignment_fixture)


@when("the real extraction command indexes the dependent-alignment fixture twice")
def when_extract_indexes_dependent_alignment_twice(
    context: FactsToolContext, dependent_alignment_fixture: Path
) -> None:
    extract_once(context, dependent_alignment_fixture)
    context.first_identities = query(
        context.facts_database_path,
        "SELECT usr,qualified_name FROM symbol ORDER BY usr,qualified_name",
    )
    context.facts_database_path.unlink()
    extract_once(context, dependent_alignment_fixture)


@then(
    "the dependent-alignment extraction exits successfully without incomplete diagnostics"
)
def then_dependent_alignment_extraction_succeeds(
    context: FactsToolContext,
) -> None:
    require(
        context.last_returncode == 0,
        f"expected extract exit code 0, got {context.last_returncode}:\n"
        + context.last_output,
    )
    require(
        "indexing incomplete" not in context.last_output,
        f"unexpected incomplete diagnostic:\n{context.last_output}",
    )
