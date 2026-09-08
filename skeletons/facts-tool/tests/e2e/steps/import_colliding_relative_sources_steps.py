from __future__ import annotations

from collections import Counter

from pytest_bdd import given, parsers, then, when
from support.colliding_sources import (
    COMPONENTS,
    NULL_CHARACTER_DIAGNOSTIC,
    RELATIVE_HEADER,
    RELATIVE_SOURCE,
    absolute,
    break_component_include,
    registered_paths,
    run_import,
    write_compilation_database,
    write_component,
    write_headers,
)
from support.database import require, scalar
from support.scenario import FactsToolContext


@given('component "component-a" defines "src/generated/Same.cpp" of about 80 KiB')
def given_large_component(context: FactsToolContext) -> None:
    write_component(context, COMPONENTS[0], large=True)


@given('component "component-b" defines "src/generated/Same.cpp" of about 400 bytes')
def given_small_component(context: FactsToolContext) -> None:
    write_component(context, COMPONENTS[1], large=False)


@given(
    'each component has its own "include/common.h" with different content, '
    "included by its Same.cpp via -Iinclude"
)
def given_colliding_headers(context: FactsToolContext) -> None:
    write_headers(context)


@given(
    'a compilation database lists both sources as "src/generated/Same.cpp" '
    "relative to their component directory"
)
def given_relative_compilation_database(context: FactsToolContext) -> None:
    write_compilation_database(context)


@given('"component-b/src/generated/Same.cpp" includes a header that does not exist')
def given_component_b_is_unpreprocessable(context: FactsToolContext) -> None:
    break_component_include(context, f"{COMPONENTS[1]}/{RELATIVE_SOURCE}")


@when(parsers.parse('the real facts-tool imports "{first}" then "{second}"'))
def when_imported_in_order(context: FactsToolContext, first: str, second: str) -> None:
    run_import(context, first, second)


@when("the real facts-tool imports the whole compilation database twice")
def when_imported_twice(context: FactsToolContext) -> None:
    first = run_import(context)
    require(
        first.returncode == 0,
        f"the first import exited with {first.returncode}:\n{context.last_output}",
    )
    run_import(context)


@when("the real facts-tool imports the whole compilation database")
def when_imported_once(context: FactsToolContext) -> None:
    run_import(context)


@then('stderr contains no "null character ignored" diagnostic')
def then_no_null_character_diagnostic(context: FactsToolContext) -> None:
    count = context.last_output.count(NULL_CHARACTER_DIAGNOSTIC)
    require(
        count == 0,
        f"import emitted {count} '{NULL_CHARACTER_DIAGNOSTIC}' diagnostics: the "
        "second translation unit was read with the first one's size",
    )


@then("the file registry contains both absolute source identities")
def then_both_sources_registered(context: FactsToolContext) -> None:
    _require_registered(
        context, [f"{component}/{RELATIVE_SOURCE}" for component in COMPONENTS]
    )


@then(
    'the file registry contains "component-a/include/common.h" '
    'and "component-b/include/common.h"'
)
def then_both_headers_registered(context: FactsToolContext) -> None:
    _require_registered(
        context, [f"{component}/{RELATIVE_HEADER}" for component in COMPONENTS]
    )


@then("the file registry contains each source and header identity exactly once")
def then_registry_is_idempotent(context: FactsToolContext) -> None:
    require(
        context.last_returncode == 0,
        f"the second import exited with {context.last_returncode}:\n{context.last_output}",
    )
    expected = [
        f"{component}/{relative}"
        for component in COMPONENTS
        for relative in (RELATIVE_SOURCE, RELATIVE_HEADER)
    ]
    counts = Counter(registered_paths(context))
    for relative in expected:
        path = absolute(context, relative)
        require(
            counts[path] == 1,
            f"{path} is registered {counts[path]} time(s), expected exactly once:\n"
            + "\n".join(sorted(counts)),
        )
    require(
        len(counts) == len(expected),
        f"the registry holds {len(counts)} identities, expected {len(expected)}:\n"
        + "\n".join(sorted(counts)),
    )


@then(
    "import fails with a nonzero exit and the message names the absolute source path"
)
def then_import_fails_naming_the_source(context: FactsToolContext) -> None:
    require(
        context.last_returncode > 0,
        f"expected a controlled nonzero exit, got {context.last_returncode}:"
        f"\n{context.last_output}",
    )
    path = absolute(context, f"{COMPONENTS[1]}/{RELATIVE_SOURCE}")
    require(
        f"cannot enumerate included files: {path} failed to preprocess"
        in context.last_output,
        f"the failure does not name {path}:\n{context.last_output}",
    )


@then("the file registry is not marked complete")
def then_registry_is_incomplete(context: FactsToolContext) -> None:
    if not context.files_database_path.exists():
        return
    complete = scalar(
        context.files_database_path, "SELECT complete FROM project_registry WHERE id=1"
    )
    require(complete == 0, "a failed import marked the file registry complete")


def _require_registered(context: FactsToolContext, relatives: list[str]) -> None:
    registered = set(registered_paths(context))
    for relative in relatives:
        path = absolute(context, relative)
        require(
            path in registered,
            f"{path} is missing from the file registry:\n" + "\n".join(sorted(registered)),
        )
