from __future__ import annotations

from pathlib import Path

from pytest_bdd import given, then, when
from support.database import query, require, scalar
from support.scenario import FactsToolContext


@given('a project with a Git root at "project"')
def given_a_git_rooted_project(context: FactsToolContext) -> None:
    context.start_git_rooted_project()


@given(
    '"project/generated/source.cpp" is a symlink '
    "to a file outside the project root"
)
def given_a_symlinked_generated_source(context: FactsToolContext) -> None:
    context.link_generated_source_outside_the_project()


@given(
    "a compilation database contains a valid command "
    'for "project/generated/source.cpp"'
)
def given_a_command_for_the_symlinked_source(context: FactsToolContext) -> None:
    context.write_symlinked_compilation_database()


@when("the real facts-tool imports the compilation database")
def when_the_symlinked_project_is_imported(context: FactsToolContext) -> None:
    context.import_symlinked_project()
    context.discard_symlink_target()


@when("the real facts-tool imports and extracts the symlinked source")
def when_the_symlinked_source_is_extracted(context: FactsToolContext) -> None:
    context.import_and_extract_symlinked_project()
    context.discard_symlink_target()


@then("import succeeds")
def then_import_succeeds(context: FactsToolContext) -> None:
    require(
        context.last_returncode == 0,
        f"import exited with {context.last_returncode}:\n{context.last_output}",
    )


@then("extraction succeeds")
def then_extraction_succeeds(context: FactsToolContext) -> None:
    require(
        context.last_returncode == 0,
        f"extraction exited with {context.last_returncode}:"
        f"\n{context.last_output}",
    )


@then("the stored active clone path is the project Git root")
def then_the_clone_path_is_the_git_root(context: FactsToolContext) -> None:
    stored = scalar(
        context.files_database_path,
        "SELECT clone.path FROM clone "
        "JOIN repository ON repository.active_clone_id=clone.id",
    )
    expected = context.symlink_project_root.resolve()
    require(
        Path(stored) == expected,
        f"active clone path is '{stored}', expected the Git root '{expected}'",
    )


@then("the stored repository name is not empty")
def then_the_repository_name_is_not_empty(context: FactsToolContext) -> None:
    names = [name for (name,) in query(
        context.files_database_path, "SELECT name FROM repository"
    )]
    require(names, "no repository was stored")
    require(
        all(name for name in names),
        f"a stored repository name is empty: {names}",
    )


@then("the generated source has a registered file identity")
def then_the_generated_source_is_registered(context: FactsToolContext) -> None:
    require(
        _in_project_file_id(context) is not None,
        "the symlinked source is missing from the file registry under its "
        f"in-project path:\n{_registry(context)}",
    )


@then("the extracted symbols belong to the in-project file identity")
def then_symbols_use_the_in_project_identity(context: FactsToolContext) -> None:
    file_id = _in_project_file_id(context)
    require(
        file_id is not None,
        "the symlinked source is missing from the file registry under its "
        f"in-project path:\n{_registry(context)}",
    )
    owners = {
        owner
        for (owner,) in query(
            context.facts_database_path,
            "SELECT DISTINCT ((id >> 32) & 4294967295) FROM symbol",
        )
    }
    require(owners, "extraction recorded no symbols")
    require(
        owners == {file_id},
        f"symbols belong to file ids {sorted(owners)}, "
        f"expected the in-project identity {file_id}",
    )


def _registry(context: FactsToolContext) -> str:
    rows = query(
        context.files_database_path,
        "SELECT file.id, component.path, directory.path, file.name "
        "FROM file JOIN directory ON directory.id=file.directory_id "
        "JOIN component ON component.id=directory.component_id "
        "ORDER BY file.id",
    )
    return "\n".join(str(row) for row in rows)


def _in_project_file_id(context: FactsToolContext) -> int | None:
    """The registry row that places the source where the project put it."""
    rows = query(
        context.files_database_path,
        "SELECT file.id FROM file "
        "JOIN directory ON directory.id=file.directory_id "
        "JOIN component ON component.id=directory.component_id "
        "WHERE file.name=? AND directory.path=? AND component.path='.'",
        ("source.cpp", "generated"),
    )
    return rows[0][0] if rows else None
