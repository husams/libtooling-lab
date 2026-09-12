from __future__ import annotations

import re

from pytest_bdd import given, parsers, then, when
from support.database import query, require, symbol_snapshot
from support.entries import extract as entries_extract
from support.entries import lookup, prepare as entries_prepare, run, succeed
from support.scenario import FactsToolContext


def _file_rows(context: FactsToolContext) -> list[tuple]:
    return query(
        context.files_database_path,
        "SELECT name,indexed,indexed_at,mtime,facts_db,git_commit FROM file "
        "WHERE name IN ('alpha.cpp','beta.cpp','tracked.cpp')",
    )


@given("a project with two freshly imported, non-git sources")
def given_index_state_project(context: FactsToolContext) -> None:
    context.start_index_state_project()


@given("the project database predates the index-state columns")
def given_predates_index_state_columns(context: FactsToolContext) -> None:
    context.drop_index_state_columns()


@when("the real facts-tool extracts every source")
def when_extracts_every_source(context: FactsToolContext) -> None:
    context.extract_index_state_sources()
    context.initial_symbols = symbol_snapshot(context.facts_database_path)


@when("the real facts-tool extracts every source again")
def when_extracts_every_source_again(context: FactsToolContext) -> None:
    context.extract_index_state_sources()


@when("the real facts-tool force-extracts every source again")
def when_force_extracts_every_source_again(context: FactsToolContext) -> None:
    context.extract_index_state_sources(force=True)


@when("the first source's mtime moves into the future")
def when_first_source_mtime_moves_into_the_future(
    context: FactsToolContext,
) -> None:
    context.bump_first_index_state_source_mtime_into_the_future()


@then("every extracted file row records indexed=1 with a non-empty timestamp")
def then_every_row_records_indexed(context: FactsToolContext) -> None:
    rows = _file_rows(context)
    require(bool(rows), f"no matching file rows: {context.last_output}")
    for name, indexed, indexed_at, mtime, _facts_db, _git_commit in rows:
        require(indexed == 1, f"{name}: expected indexed=1, got {indexed}")
        require(bool(indexed_at), f"{name}: indexed_at must not be empty")
        require(
            re.fullmatch(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z", indexed_at)
            is not None,
            f"{name}: indexed_at is not ISO-8601 UTC: {indexed_at!r}",
        )
        require(mtime is not None, f"{name}: mtime must be recorded")


@then("every extracted file's facts_db matches the facts database path")
def then_every_row_records_facts_db(context: FactsToolContext) -> None:
    expected = str(context.facts_database_path.resolve())
    for name, _indexed, _indexed_at, _mtime, facts_db, _git_commit in _file_rows(
        context
    ):
        require(
            facts_db == expected,
            f"{name}: facts_db {facts_db!r} != {expected!r}",
        )


@then("every extracted file's git_commit is NULL")
def then_every_row_records_no_git_commit(context: FactsToolContext) -> None:
    for name, *_rest, git_commit in _file_rows(context):
        require(git_commit is None, f"{name}: expected NULL git_commit")


@then(parsers.parse('"file show" prints the index state for the first source'))
def then_file_show_prints_index_state(context: FactsToolContext) -> None:
    first, _second = context.index_state_sources
    completed = context.run(context.file_show_command(first))
    require(
        completed.returncode == 0,
        f"facts-tool file show exited with {completed.returncode}:"
        f"\n{context.last_output}",
    )
    require(
        "INDEXED AT: " in context.last_output,
        f"missing INDEXED AT line:\n{context.last_output}",
    )
    require(
        "FACTS DB: " in context.last_output,
        f"missing FACTS DB line:\n{context.last_output}",
    )
    require(
        "GIT COMMIT: " in context.last_output,
        f"missing GIT COMMIT line:\n{context.last_output}",
    )


@then(parsers.parse("the extract exits {code:d}"))
def then_extract_exits(context: FactsToolContext, code: int) -> None:
    require(
        context.last_returncode == code,
        f"expected exit {code}, got {context.last_returncode}:"
        f"\n{context.last_output}",
    )


@then("the extract reports up to date with nothing to extract")
def then_extract_reports_up_to_date(context: FactsToolContext) -> None:
    require(
        "up to date; nothing to extract" in context.last_output,
        f"missing up-to-date summary:\n{context.last_output}",
    )


@then("the symbol count is unchanged")
def then_symbol_count_unchanged(context: FactsToolContext) -> None:
    require(
        symbol_snapshot(context.facts_database_path) == context.initial_symbols,
        "symbol table changed even though nothing was stale",
    )


@then(
    parsers.parse(
        "the extract reports {stale:d} stale source(s) and "
        "{fresh:d} up-to-date source(s)"
    )
)
def then_extract_reports_freshness_summary(
    context: FactsToolContext, stale: int, fresh: int
) -> None:
    expected = f"up_to_date={fresh} stale={stale}"
    require(
        expected in context.last_output,
        f"missing {expected!r} in:\n{context.last_output}",
    )


@given("a git-rooted project with one committed source")
def given_git_rooted_project(context: FactsToolContext) -> None:
    context.start_git_rooted_index_state_project()


@when("the real facts-tool extracts the git-rooted project")
def when_extracts_git_rooted_project(context: FactsToolContext) -> None:
    context.extract_git_project()


@when("the real facts-tool extracts the git-rooted project again")
def when_extracts_git_rooted_project_again(context: FactsToolContext) -> None:
    context.extract_git_project()


@when("another commit is made to the git-rooted project")
def when_another_commit_is_made(context: FactsToolContext) -> None:
    context.commit_another_change_to_git_project()


@when("an unrelated file is committed to the git-rooted project")
def when_an_unrelated_file_is_committed(context: FactsToolContext) -> None:
    context.commit_an_unrelated_change_to_git_project()


@then("the extracted file's git_commit equals the repository's HEAD")
def then_git_commit_equals_head(context: FactsToolContext) -> None:
    rows = query(
        context.files_database_path,
        "SELECT git_commit FROM file WHERE name='tracked.cpp'",
    )
    require(len(rows) == 1, f"expected exactly one tracked.cpp row: {rows}")
    require(
        rows[0][0] == context.git_head(),
        f"recorded git_commit {rows[0][0]!r} != HEAD {context.git_head()!r}",
    )


# --- an included header makes its including source stale ------------------


@given("a project with a source that includes a header")
def given_header_index_state_project(context: FactsToolContext) -> None:
    context.start_header_index_state_project()


@when("the real facts-tool extracts that source")
def when_extracts_that_source(context: FactsToolContext) -> None:
    context.extract_header_index_state_source()


@when("only the included header is edited")
def when_only_the_included_header_is_edited(context: FactsToolContext) -> None:
    context.edit_header_index_state_header()


@when("the real facts-tool extracts that source again")
def when_extracts_that_source_again(context: FactsToolContext) -> None:
    context.extract_header_index_state_source()


@then("the header row's index state is updated too")
def then_header_row_index_state_updated(context: FactsToolContext) -> None:
    _source, header = context.header_index_state_sources
    rows = query(
        context.files_database_path,
        "SELECT indexed,mtime FROM file WHERE name=?",
        (header.name,),
    )
    require(len(rows) == 1, f"expected exactly one {header.name} row: {rows}")
    indexed, mtime = rows[0]
    require(indexed == 1, f"{header.name}: expected indexed=1, got {indexed}")
    expected_mtime = header.stat().st_mtime
    require(
        mtime is not None and abs(mtime - expected_mtime) < 1.0,
        f"{header.name}: recorded mtime {mtime} does not match the edited "
        f"file's current mtime {expected_mtime}",
    )


# --- a file extracted only into another facts database is still a --------
# --- recovery candidate ----------------------------------------------------


@given("an app that calls into a library extracted into one facts database")
def given_app_calling_library_extracted_into_one_facts_database(
    context: FactsToolContext,
) -> None:
    entries_prepare(context)
    succeed(
        run(
            context,
            "extract",
            "-v",
            "0",
            "--force",
            "--conf",
            context.files_database_path,
            "--output",
            context.facts_database_path,
            *context.entry_sources,
        )
    )


@when("the app alone is extracted into a second facts database")
def when_the_app_alone_is_extracted_into_a_second_facts_database(
    context: FactsToolContext,
) -> None:
    context.facts_database = context.run_root_path / "second-facts.sqlite"
    succeed(entries_extract(context))


@then("the library source is a recovery candidate against the second facts database")
def then_library_is_recovery_candidate(context: FactsToolContext) -> None:
    entry = lookup(context, "boundary")
    candidates = entry["extraction_coverage"]["recovery_candidates"]
    require(
        any(candidate.endswith("library.cpp") for candidate in candidates),
        f"expected library.cpp among recovery candidates: {candidates}",
    )


@when("the app and the library are both extracted into a second facts database")
def when_the_app_and_library_are_both_extracted_into_a_second_facts_database(
    context: FactsToolContext,
) -> None:
    context.facts_database = context.run_root_path / "second-facts.sqlite"
    for source in context.entry_sources:
        succeed(
            run(
                context,
                "extract",
                "-v",
                "0",
                "--force",
                "--conf",
                context.files_database_path,
                "--output",
                context.facts_database_path,
                source,
            )
        )


@then(
    "the library source is not a recovery candidate against the second facts "
    "database"
)
def then_library_is_not_recovery_candidate(context: FactsToolContext) -> None:
    entry = lookup(context, "boundary")
    candidates = entry["extraction_coverage"]["recovery_candidates"]
    require(
        not any(candidate.endswith("library.cpp") for candidate in candidates),
        "library.cpp should not be a recovery candidate once it is extracted "
        f"into this same facts database too: {candidates}",
    )


# --- a catalog mutation resets index state ---------------------------------


@when("a compile option is set on the first source")
def when_a_compile_option_is_set_on_the_first_source(
    context: FactsToolContext,
) -> None:
    completed = context.set_compile_option_on_first_index_state_source()
    require(
        completed.returncode == 0,
        f"facts-tool file set-option exited with {completed.returncode}:"
        f"\n{context.last_output}",
    )


# --- reimport leaves unchanged index state alone, resets only what changed -


@when("the project is reimported with unchanged compile commands")
def when_reimported_with_unchanged_compile_commands(
    context: FactsToolContext,
) -> None:
    context.reimport_index_state_sources_unchanged()


@when("the project is reimported with a changed compile option for the first source")
def when_reimported_with_a_changed_compile_option(
    context: FactsToolContext,
) -> None:
    context.reimport_index_state_sources_with_changed_flag_on_first_source()
