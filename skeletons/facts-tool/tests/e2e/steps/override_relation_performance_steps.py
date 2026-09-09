from __future__ import annotations

import subprocess
from pathlib import Path

from pytest_bdd import given, then, when
from steps.external_target_steps import prepare_compile_database
from support.database import require
from support.scenario import FactsToolContext

FIXTURE = "override_relation_perf.cpp"
HEADER = "override_relation_perf_system.hpp"


@given(
    "a compile database for the override relation performance fixture",
    target_fixture="override_perf_source",
)
def given_override_perf_fixture(context: FactsToolContext) -> Path:
    return prepare_compile_database(context, FIXTURE, "override-relation-perf")


@when("verbose override relation extraction runs")
def when_verbose_override_extraction_runs(
    context: FactsToolContext, override_perf_source: Path
) -> None:
    imported = subprocess.run(
        context.import_command((override_perf_source,)),
        capture_output=True,
        text=True,
        check=False,
    )
    require(imported.returncode == 0, imported.stdout + imported.stderr)
    extracted = subprocess.run(
        [
            str(context.facts_tool),
            "extract",
            "-v",
            "3",
            "--output",
            str(context.facts_database_path),
            "--conf",
            str(context.files_database_path),
            str(override_perf_source),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    context.last_returncode = extracted.returncode
    context.last_output = extracted.stdout + extracted.stderr


@then("system-header override resolution is skipped and project extraction commits")
def then_override_resolution_is_bounded(
    context: FactsToolContext, override_perf_source: Path
) -> None:
    require(context.last_returncode == 0, context.last_output)
    require("rollback output transaction" not in context.last_output,
            context.last_output)
    start = "facts-tool: extract: Clang parse and AST extraction"
    end = "facts-tool: extract: commit output transaction"
    require(start in context.last_output and end in context.last_output,
            context.last_output)
    extraction_log = context.last_output.partition(start)[2].partition(end)[0]
    header = str(override_perf_source.with_name(HEADER))
    require(f"file resolve requested='{header}'" not in extraction_log,
            extraction_log)
    shown = subprocess.run(
        [str(context.facts_tool), "symbol", "show", "-c",
         str(context.files_database_path), "-f", str(context.facts_database_path),
         "override_relation_perf::ordinary"],
        capture_output=True,
        text=True,
        check=False,
    )
    require(shown.returncode == 0, shown.stdout + shown.stderr)
    require("override_relation_perf::ordinary" in shown.stdout, shown.stdout)
