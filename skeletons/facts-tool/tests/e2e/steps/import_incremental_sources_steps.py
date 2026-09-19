from __future__ import annotations

import json

from pytest_bdd import given, parsers, then, when
from support.ast_cache_git import initialize_repository
from support.database import file_snapshot, query


def command_rows(defaults):
    return query(
        defaults.incremental_database,
        "SELECT id,name,driver,working_directory,compile_options FROM file "
        "WHERE name IN ('one.cpp','two.cpp') ORDER BY name",
    )


def indexed_state(defaults, source):
    return query(
        defaults.incremental_database,
        "SELECT id,indexed,indexed_at,facts_db,md5,git_commit FROM file WHERE name=?",
        (source,),
    )


def successful(defaults, *arguments):
    result = defaults.run(*arguments, cwd=defaults.incremental_root)
    assert result.returncode == 0, result.stdout + result.stderr
    return result


def write_commands(defaults, directory, sources):
    directory.mkdir(parents=True, exist_ok=True)
    commands = [
        {
            "directory": str(defaults.incremental_root),
            "file": source,
            "arguments": [
                defaults.incremental_compiler, "-std=c++23",
                f"-D{defaults.incremental_macros[source]}", "-c", source,
            ],
        }
        for source in sources
    ]
    (directory / "compile_commands.json").write_text(json.dumps(commands))


def import_source(defaults, source):
    root = defaults.incremental_root
    if defaults.incremental_mode == "filtered JSON":
        write_commands(defaults, root, ("one.cpp", "two.cpp"))
        arguments = ["-p", root, root / source]
    elif defaults.incremental_mode == "single-command JSON":
        directory = root / ("build-" + source.removesuffix(".cpp"))
        write_commands(defaults, directory, (source,))
        arguments = ["-p", directory]
    else:
        assert defaults.incremental_mode == "fixed arguments"
        arguments = ["--extra-arg=-std=c++23",
                     "--extra-arg=-D" + defaults.incremental_macros[source], root / source]
    successful(defaults, "import", *arguments)


def extract_source(defaults, source):
    successful(defaults, "extract", defaults.incremental_root / source)


@given(parsers.parse(
    'an incremental import project using "{mode}" with AST caching "{cache}"'
))
def incremental_project(defaults, context, mode, cache):
    root = defaults.root / "incremental-repo"
    root.mkdir()
    defaults.incremental_root = root
    defaults.incremental_database = defaults.root / "incremental.db"
    defaults.incremental_facts = defaults.root / "incremental-facts.db"
    defaults.incremental_compiler = str(context.compiler)
    defaults.incremental_mode = mode
    defaults.incremental_macros = {"one.cpp": "FIRST_VALUE=1", "two.cpp": "SECOND_VALUE=2"}
    assert cache in ("enabled", "disabled")
    (root / ".facts-tool.yaml").write_text(json.dumps({
        "conf_root": str(defaults.root),
        "conf_template": defaults.incremental_database.name,
        "facts_template": str(defaults.incremental_facts),
        "ast_cache": cache == "enabled",
    }))
    (root / "nested.hpp").write_text("#pragma once\nstruct IncrementalNested {};\n")
    (root / "shared.hpp").write_text('#pragma once\n#include "nested.hpp"\n')
    for source, name, macro in (("one.cpp", "incremental_one", "FIRST_VALUE"),
                                ("two.cpp", "incremental_two", "SECOND_VALUE")):
        (root / source).write_text(
            '#include "shared.hpp"\n'
            f"#ifndef {macro}\n#error Required compiler option was lost\n#endif\n"
            f"int {name}() {{ return {macro}; }}\n"
        )
    initialize_repository(root, defaults.env)


@when("I import the first incremental source and extract it")
def first_import(defaults):
    import_source(defaults, "one.cpp")
    extract_source(defaults, "one.cpp")
    defaults.incremental_first_command = command_rows(defaults)[0]
    defaults.incremental_first_state = indexed_state(defaults, "one.cpp")
    assert defaults.incremental_first_state[0][1] == 1
    defaults.incremental_first_files = file_snapshot(defaults.incremental_database)


@when("I import the second incremental source")
def second_import(defaults):
    import_source(defaults, "two.cpp")


@then("both incremental sources retain their compiler commands and dependencies")
def commands_and_dependencies_retained(defaults):
    commands = command_rows(defaults)
    assert len(commands) == 2, commands
    for _, source, driver, directory, options in commands:
        assert driver and directory and options, f"import erased {source}'s command: {commands}"
        assert "-D" + defaults.incremental_macros[source] in json.loads(options), commands
    assert commands[0] == defaults.incremental_first_command, commands
    files = file_snapshot(defaults.incremental_database)
    expected_paths = {str(defaults.incremental_root / name)
                      for name in ("one.cpp", "two.cpp", "shared.hpp", "nested.hpp")}
    assert {path for _, path in files} == expected_paths, files
    assert len(files) == len(expected_paths), files
    assert set(defaults.incremental_first_files).issubset(files)
    assert query(defaults.incremental_database, "PRAGMA foreign_key_check") == []
    defaults.incremental_files = files


@then("the first incremental source keeps its indexed state")
def first_index_preserved(defaults):
    assert indexed_state(defaults, "one.cpp") == defaults.incremental_first_state


@when("I extract both incremental sources using only stored commands")
def extract_both(defaults):
    for path in defaults.incremental_root.rglob("compile_commands.json"):
        path.unlink()
    extract_source(defaults, "one.cpp")
    extract_source(defaults, "two.cpp")
    defaults.incremental_second_state = indexed_state(defaults, "two.cpp")


@then("both incremental sources are indexed")
def both_indexed(defaults):
    for source in ("one.cpp", "two.cpp"):
        state = indexed_state(defaults, source)
        assert len(state) == 1 and state[0][1] == 1, state
    names = query(defaults.incremental_facts,
                  "SELECT qualified_name FROM symbol WHERE qualified_name IN "
                  "('incremental_one','incremental_two') ORDER BY qualified_name")
    assert names == [("incremental_one",), ("incremental_two",)], names


@when("I change the first source compiler options and reimport it twice")
def change_first(defaults):
    defaults.incremental_commands = command_rows(defaults)
    defaults.incremental_macros["one.cpp"] = "FIRST_VALUE=7"
    for _ in range(2):
        import_source(defaults, "one.cpp")


@then("only the first incremental command changes without duplicate identities")
def only_first_command_changes(defaults):
    commands = command_rows(defaults)
    assert len(commands) == 2, commands
    assert commands[0][:4] == defaults.incremental_commands[0][:4], commands
    assert "-DFIRST_VALUE=7" in json.loads(commands[0][4]), commands
    assert "-DFIRST_VALUE=1" not in json.loads(commands[0][4]), commands
    assert commands[1] == defaults.incremental_commands[1], commands
    assert file_snapshot(defaults.incremental_database) == defaults.incremental_files
    assert indexed_state(defaults, "one.cpp")[0][1] == 0
    assert query(defaults.incremental_database, "PRAGMA foreign_key_check") == []


@then("the second incremental source keeps its indexed state")
def second_index_preserved(defaults):
    assert indexed_state(defaults, "two.cpp") == defaults.incremental_second_state


@when("I import the complete incremental compilation database and extract its sources")
def import_complete(defaults):
    write_commands(defaults, defaults.incremental_root, ("one.cpp", "two.cpp"))
    successful(defaults, "import", "-p", defaults.incremental_root)
    extract_both(defaults)
    defaults.incremental_commands = command_rows(defaults)
    defaults.incremental_files = file_snapshot(defaults.incremental_database)


@when("I import a replacement compilation database containing only the changed first source")
def import_replacement(defaults):
    defaults.incremental_macros["one.cpp"] = "FIRST_VALUE=7"
    write_commands(defaults, defaults.incremental_root, ("one.cpp",))
    successful(defaults, "import", "-p", defaults.incremental_root)
