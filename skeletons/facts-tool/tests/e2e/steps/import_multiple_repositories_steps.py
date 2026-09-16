from __future__ import annotations

import json
import sqlite3
import subprocess

from pytest_bdd import given, parsers, then, when
from support.database import file_snapshot


def rows(defaults, sql):
    uri = defaults.shared_database.as_uri() + "?mode=ro"
    with sqlite3.connect(uri, uri=True) as connection:
        return connection.execute(sql).fetchall()


def identities(defaults):
    return {
        table: rows(defaults, f'SELECT * FROM "{table}" ORDER BY id')
        for table in ("repository", "clone", "component", "directory")
    } | {"file": file_snapshot(defaults.shared_database)}


def import_repository(defaults, repository):
    result = defaults.run(
        "import", "-p", repository,
        "--component", f"{repository.name}={repository}", *defaults.args, cwd=repository,
    )
    assert result.returncode == 0, result.stdout + result.stderr


@given("two Git repositories share one generated project database")
def shared_repositories(defaults, context):
    defaults.shared_database = defaults.root / "cache/project.db"
    defaults.repositories = [defaults.root / name for name in ("repo-one", "repo-two")]
    configuration = {
        "conf_root": str(defaults.shared_database.parent),
        "conf_template": defaults.shared_database.name,
        "facts_template": str(defaults.root / "shared-facts.db"),
    }
    for value, repository in enumerate(defaults.repositories, 1):
        repository.mkdir()
        subprocess.run(["git", "init", "--quiet", str(repository)], check=True,
                       capture_output=True, text=True)
        (repository / ".facts-tool.yaml").write_text(json.dumps(configuration))
        (repository / "api.hpp").write_text("int repository_value();\n")
        (repository / "main.cpp").write_text(
            '#include "api.hpp"\nint repository_value() { return REPOSITORY_VALUE; }\n'
        )
        arguments = [str(context.compiler), "-std=c++23", f"-DREPOSITORY_VALUE={value}",
                     "-c", "main.cpp"]
        (repository / "compile_commands.json").write_text(json.dumps([{
            "directory": str(repository), "file": "main.cpp", "arguments": arguments,
        }]))


@given(parsers.parse('the shared database is selected by "{selection}"'))
def select_database(defaults, selection):
    if selection == "explicit":
        defaults.shared_database.parent.mkdir()
        defaults.args = ["--conf", str(defaults.shared_database)]
    else:
        assert selection == "generated"


@given(parsers.parse('the shared repositories are "{registration}"'))
def register_repositories(defaults, registration):
    if registration == "registered first":
        for repository in defaults.repositories:
            result = defaults.run("repo", "add", repository.name, repository,
                                  *defaults.args, cwd=defaults.repositories[0])
            assert result.returncode == 0, result.stdout + result.stderr
    else:
        assert registration == "discovered"


@when("I import each repository from its own checkout")
def import_both(defaults):
    for repository in defaults.repositories:
        import_repository(defaults, repository)
    defaults.imported_identities = identities(defaults)


@when("I extract both repository sources using their stored commands")
def extract_both(defaults):
    for repository in defaults.repositories:
        result = defaults.run(
            "extract", "--output", defaults.root / "shared-facts.db",
            repository / "main.cpp", *defaults.args, cwd=repository,
        )
        assert result.returncode == 0, result.stdout + result.stderr
    defaults.indexed_states = rows(defaults,
        "SELECT id,indexed,indexed_at,facts_db,md5,git_commit FROM file "
        "WHERE name='main.cpp' ORDER BY id")


@then("both repository sources are indexed")
def both_indexed(defaults):
    assert len(defaults.indexed_states) == 2, defaults.indexed_states
    assert all(row[1] == 1 for row in defaults.indexed_states), defaults.indexed_states


@when("I reimport both repositories in reverse order twice")
def reimport_both(defaults):
    for _ in range(2):
        for repository in reversed(defaults.repositories):
            import_repository(defaults, repository)


@then("both repositories retain their sources headers and compilation commands")
def retained_catalogs(defaults):
    expected_paths = {
        str(repository / name)
        for repository in defaults.repositories
        for name in ("main.cpp", "api.hpp")
    }
    actual_files = file_snapshot(defaults.shared_database)
    assert len(actual_files) == len(expected_paths), actual_files
    assert {path for _, path in actual_files} == expected_paths, actual_files
    assert rows(defaults, "SELECT count(*) FROM repository") == [(2,)]
    assert rows(defaults, "SELECT count(*) FROM clone") == [(2,)]
    if not defaults.args:
        assert set(rows(defaults, "SELECT project_root FROM generated_conf_owner")) == {
            (str(repository),) for repository in defaults.repositories
        }
    commands = rows(defaults,
        "SELECT r.name,f.driver,f.compile_options,f.working_directory FROM file f "
        "JOIN directory d ON d.id=f.directory_id "
        "JOIN component c ON c.id=d.component_id "
        "JOIN repository r ON r.id=c.repository_id WHERE f.name='main.cpp' "
        "ORDER BY r.name")
    assert len(commands) == 2, commands
    for value, (name, driver, options, working_directory) in enumerate(commands, 1):
        repository = defaults.repositories[value - 1]
        assert name == repository.name, commands
        assert driver and options, f"import erased compilation command for {name}: {commands}"
        assert working_directory == f"<{repository.name}>", commands
        assert f"-DREPOSITORY_VALUE={value}" in json.loads(options), commands
    assert rows(defaults, "PRAGMA foreign_key_check") == []


@then("every repository clone component directory and file keeps its identity")
def retained_identities(defaults):
    assert identities(defaults) == defaults.imported_identities


@then("neither repository loses its unchanged indexed state")
def retained_index_state(defaults):
    assert rows(defaults,
        "SELECT id,indexed,indexed_at,facts_db,md5,git_commit FROM file "
        "WHERE name='main.cpp' ORDER BY id") == defaults.indexed_states


@when("I export the stored compilation commands from each repository")
def export_commands(defaults):
    defaults.exported_commands = []
    for repository in defaults.repositories:
        result = defaults.run("component", "compile-commands", repository.name,
                              *defaults.args, cwd=repository)
        assert result.returncode == 0, result.stdout + result.stderr
        defaults.exported_commands.append(json.loads(result.stdout))


@then("each export contains only that repository's original compilation command")
def exported_commands(defaults):
    for value, (repository, commands) in enumerate(
        zip(defaults.repositories, defaults.exported_commands), 1
    ):
        assert len(commands) == 1, commands
        command = commands[0]
        assert command["directory"] == str(repository), command
        assert command["file"] == str(repository / "main.cpp"), command
        assert f"-DREPOSITORY_VALUE={value}" in command["arguments"], command
