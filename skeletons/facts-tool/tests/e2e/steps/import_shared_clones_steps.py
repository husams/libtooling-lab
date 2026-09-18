from __future__ import annotations

import json
import shutil
import sqlite3
import subprocess

from pytest_bdd import given, parsers, then, when
from steps.import_multiple_repositories_steps import (
    identities, import_repository, rows,
)


def no_owner(defaults):
    assert rows(defaults,
        "SELECT name FROM sqlite_master WHERE name='generated_conf_owner'") == []


def file_identities(defaults):
    return rows(defaults, "SELECT id,directory_id,name FROM file ORDER BY id")


@given("the shared catalog has an obsolete owner marker for an unrelated checkout")
def legacy_marker(defaults):
    with sqlite3.connect(defaults.shared_database) as connection:
        connection.execute(
            "CREATE TABLE generated_conf_owner(project_root TEXT PRIMARY KEY)")
        connection.execute("INSERT INTO generated_conf_owner VALUES(?)",
                           (str(defaults.root / "removed-checkout"),))


@when(parsers.parse('I import both repositories using "{selection}" database selection'))
def migrate_import(defaults, selection):
    if selection == "explicit":
        defaults.args = ["--conf", str(defaults.shared_database)]
    elif selection == "environment":
        defaults.env["FACTS_TOOL_CONF"] = str(defaults.shared_database)
    else:
        assert selection == "generated"
    for repository in reversed(defaults.repositories):
        import_repository(defaults, repository)
        no_owner(defaults)


@when("I register a second clone of each shared repository")
def register_second_clones(defaults):
    defaults.second_clones = []
    for repository in defaults.repositories:
        subprocess.run(
            ["git", "-C", str(repository), "remote", "add", "origin",
             f"https://example.invalid/{repository.name}.git"],
            check=True, capture_output=True, text=True,
        )
        clone = defaults.root / f"{repository.name}-second"
        shutil.copytree(repository, clone)
        database = clone / "compile_commands.json"
        commands = json.loads(database.read_text())
        for command in commands:
            command["directory"] = str(clone)
        database.write_text(json.dumps(commands))
        result = defaults.run("repo", "add-clone", repository.name, clone,
                              "--label", "second", cwd=repository)
        assert result.returncode == 0, result.stdout + result.stderr
        defaults.second_clones.append(clone)
        no_owner(defaults)
    defaults.clone_identities = identities(defaults)
    defaults.clone_file_ids = file_identities(defaults)


@then("each repository has two clones and one active local checkout")
def one_active_clone(defaults):
    catalog = rows(defaults,
        "SELECT r.name,count(c.id),active.path FROM repository r "
        "JOIN clone c ON c.repository_id=r.id "
        "JOIN clone active ON active.id=r.active_clone_id AND active.repository_id=r.id "
        "GROUP BY r.id ORDER BY r.name")
    assert catalog == [(repository.name, 2, str(repository))
                       for repository in defaults.repositories], catalog
    no_owner(defaults)


@when("I switch to both second clones and import from those checkouts")
def switch_and_import(defaults):
    for repository, clone in zip(defaults.repositories, defaults.second_clones):
        result = defaults.run("repo", "switch", repository.name, "second", cwd=clone)
        assert result.returncode == 0, result.stdout + result.stderr
        import_repository(defaults, clone, name=repository.name)
        no_owner(defaults)
        assert file_identities(defaults) == defaults.clone_file_ids


@then("the shared catalog resolves unchanged file identities through the active clones")
def active_clone_resolution(defaults):
    active = rows(defaults,
        "SELECT r.name,c.path FROM repository r "
        "JOIN clone c ON c.id=r.active_clone_id AND c.repository_id=r.id ORDER BY r.name")
    assert active == [(repository.name, str(clone))
                      for repository, clone in zip(defaults.repositories, defaults.second_clones)]
    assert file_identities(defaults) == defaults.clone_file_ids
    assert rows(defaults, "SELECT count(*) FROM clone") == [(4,)]
    for repository, clone in zip(defaults.repositories, defaults.second_clones):
        result = defaults.run("component", "compile-commands", repository.name, cwd=clone)
        assert result.returncode == 0, result.stdout + result.stderr
        commands = json.loads(result.stdout)
        assert len(commands) == 1, commands
        assert commands[0]["directory"] == str(clone), commands
        assert commands[0]["file"] == str(clone / "main.cpp"), commands
    assert rows(defaults, "PRAGMA foreign_key_check") == []
    no_owner(defaults)


@when("I switch back and reimport both original checkouts")
def switch_back(defaults):
    for repository in defaults.repositories:
        result = defaults.run("repo", "switch", repository.name, repository, cwd=repository)
        assert result.returncode == 0, result.stdout + result.stderr
        import_repository(defaults, repository)
        no_owner(defaults)


@then("all repository clone and file identities survive both switches")
def identities_preserved(defaults):
    assert identities(defaults) == defaults.clone_identities
    no_owner(defaults)
