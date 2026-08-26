from __future__ import annotations

from pathlib import Path

from pytest_bdd import then, when
from support.database import file_snapshot, query, require
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("the file registry contains these canonical fixture paths")
def then_registry_contains_canonical_fixture_paths(
    context: FactsToolContext, datatable: Table
) -> None:
    files = file_snapshot(context.files_database_path)
    expected_paths = {
        str((context.fixture_root / row["fixture"]).resolve(strict=True))
        for row in table_records(datatable)
    }
    actual_paths = {path for _, path in files}
    require(actual_paths == expected_paths, f"unexpected file registry: {files}")
    require(
        all(
            Path(path).is_absolute() and Path(path).resolve(strict=True) == Path(path)
            for _, path in files
        ),
        f"non-canonical source path: {files}",
    )


@then("every registered FileId is greater than zero")
def then_registered_file_ids_are_nonzero(context: FactsToolContext) -> None:
    files = file_snapshot(context.files_database_path)
    require(all(file_id > 0 for file_id, _ in files), "FileId 0 is reserved")


@then("every captured symbol uses a registered nonzero FileId")
def then_symbols_use_registered_file_ids(context: FactsToolContext) -> None:
    imported_ids = {
        file_id for file_id, _ in file_snapshot(context.files_database_path)
    }
    symbol_file_ids = {
        file_id
        for (file_id,) in query(
            context.facts_database_path,
            "SELECT DISTINCT ((id >> 32) & 4294967295) FROM symbol",
        )
    }
    require(0 not in symbol_file_ids, "captured symbols must not use builtin FileId 0")
    require(
        symbol_file_ids <= imported_ids,
        f"symbols reference non-preimported FileIds: {symbol_file_ids - imported_ids}",
    )


# The C and C++ inputs extraction can resolve. The C++ standard library spells
# its headers without a suffix, so an extension-less name is one too.
COMPILABLE_SUFFIXES = frozenset(
    {
        ".c",
        ".cc",
        ".cp",
        ".cpp",
        ".cxx",
        ".c++",
        ".m",
        ".mm",
        ".h",
        ".hh",
        ".hp",
        ".hpp",
        ".hxx",
        ".h++",
        ".def",
        ".inc",
        ".inl",
        ".ipp",
        ".tcc",
        ".tpp",
        ".cu",
        ".cuh",
        ".ixx",
        ".cppm",
        ".ccm",
        ".cxxm",
        ".mpp",
    }
)


# Documentation keeps its spelling wherever it is found, so a suffix-less name
# is a header only when it is not one of these.
PROJECT_METADATA = frozenset(
    {
        "AUTHORS",
        "CHANGELOG",
        "CHANGES",
        "CONTRIBUTING",
        "COPYING",
        "CREDITS",
        "Dockerfile",
        "Doxyfile",
        "GNUmakefile",
        "INSTALL",
        "LICENCE",
        "LICENSE",
        "MANIFEST",
        "Makefile",
        "NEWS",
        "NOTICE",
        "README",
        "TODO",
        "VERSION",
        "makefile",
    }
)


def _compilable(path: str) -> bool:
    name = Path(path).name
    suffix = Path(path).suffix
    if not suffix:
        return not name.startswith(".") and name not in PROJECT_METADATA
    return suffix.lower() in COMPILABLE_SUFFIXES


@then("the file registry excludes these fixture paths")
def then_registry_excludes_fixture_paths(
    context: FactsToolContext, datatable: Table
) -> None:
    excluded = {
        str((context.fixture_root / row["fixture"]).resolve(strict=True))
        for row in table_records(datatable)
    }
    registered = {path for _, path in file_snapshot(context.files_database_path)}
    require(
        not (registered & excluded),
        f"registry holds files that are not C or C++ inputs: {registered & excluded}",
    )


@then("every registered path is a C or C++ input")
def then_every_registered_path_is_compilable(context: FactsToolContext) -> None:
    registered = [path for _, path in file_snapshot(context.files_database_path)]
    rejected = [path for path in registered if not _compilable(path)]
    require(not rejected, f"registry holds non-C/C++ paths: {rejected}")


@when("the same project is imported again")
def when_the_same_project_is_imported_again(context: FactsToolContext) -> None:
    context.run_import()


@then("the import reports no newly registered files")
def then_import_reports_no_new_files(context: FactsToolContext) -> None:
    require(
        "Registered 0 file(s)" in context.import_output,
        f"a repeated import must add nothing:\n{context.import_output}",
    )
