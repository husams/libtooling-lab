import json
import sqlite3

from pytest_bdd import given, when, then, parsers


LEAVES = (
    ("import",), ("extract",), ("match",),
    ("analyse", "dependency"), ("analyse", "call-graph"),
    ("analyse", "call-graph-entry"), ("analyse", "variable-flow"),
    ("config", "show"),
    *(("repo", name) for name in
      ("list", "ls", "show", "add", "add-clone", "switch", "rm-clone",
       "remove-clone", "rm")),
    *(("component", name) for name in
      ("list", "ls", "show", "add", "set-version", "compile-commands", "rm")),
    *(("dir", name) for name in ("list", "ls", "rm")),
    *(("file", name) for name in
      ("list", "ls", "show", "add", "rm", "remove", "set-option", "clear-option")),
    *(("symbol", name) for name in ("list", "ls", "show", "browser", "find")),
    ("symbol", "index", "clear"),
)


def succeed(defaults, *arguments):
    result = defaults.run(*arguments)
    assert result.returncode == 0, (arguments, result.stdout, result.stderr)
    return result.stdout


def rows(defaults, query):
    with sqlite3.connect(defaults.catalog_database) as database:
        return database.execute(query).fetchall()


@given(parsers.parse('a catalog configured through "{tier}" without command-line database options'))
def configured_catalog(defaults, tier, context):
    defaults.catalog_database = defaults.root / "automatic/project.db"
    defaults.catalog_database.parent.mkdir()
    yaml_tier = "config-file" if tier == "env" else (
        "project" if tier == "db-env" else tier)
    path = defaults.write(yaml_tier,
                          conf_root=str(defaults.catalog_database.parent),
                          conf_template=defaults.catalog_database.name,
                          facts_template="{project_root}/facts.db")
    if tier == "env":
        defaults.env["FACTS_TOOL_CONFIG"] = str(path)
    if tier == "db-env":
        defaults.env["FACTS_TOOL_CONF"] = str(defaults.catalog_database)
    defaults.checkouts = [defaults.root / name for name in ("first", "second")]
    for checkout in defaults.checkouts:
        checkout.mkdir()
    defaults.catalog_component = defaults.cwd / "catalog-component"
    defaults.catalog_component.mkdir()
    defaults.catalog_source = defaults.catalog_component / "src/source.cpp"
    defaults.catalog_source.parent.mkdir()
    defaults.catalog_driver = context.compiler
    defaults.catalog_source.write_text("int optional_configuration() { return 1; }\n")
    defaults.configuration_bytes = path.read_bytes()
    defaults.configuration_path = path


def repository_lifecycle(defaults):
    first, second = defaults.checkouts
    succeed(defaults, "repo", "add", "automatic", first, "--label", "first")
    for command in ("list", "ls"):
        assert "automatic" in succeed(defaults, "repo", command)
    assert str(first) in succeed(defaults, "repo", "show", "automatic")
    succeed(defaults, "repo", "add-clone", "automatic", second, "--label", "second")
    succeed(defaults, "repo", "switch", "automatic", "second")
    assert rows(defaults, "SELECT c.path FROM repository r JOIN clone c "
                "ON c.id=r.active_clone_id WHERE r.name='automatic'") == [(str(second),)]
    succeed(defaults, "repo", "rm-clone", "automatic", "first")
    succeed(defaults, "repo", "add-clone", "automatic", first, "--label", "first")
    succeed(defaults, "repo", "remove-clone", "automatic", "first")
    succeed(defaults, "repo", "rm", "automatic", "--dry-run")
    succeed(defaults, "repo", "rm", "automatic")
    assert rows(defaults, "SELECT name FROM repository") == []


def component_setup(defaults):
    succeed(defaults, "component", "add", "--name", "automatic", "--path",
            defaults.catalog_component, "--kind", "external", "--no-git")
    for command in ("list", "ls"):
        assert "automatic" in succeed(defaults, "component", command)
    assert "automatic" in succeed(defaults, "component", "show", "automatic")
    succeed(defaults, "component", "set-version", "automatic", "1.0.0")
    assert rows(defaults, "SELECT version FROM component WHERE name='automatic'") == [("1.0.0",)]
    succeed(defaults, "component", "set-version", "automatic")
    assert rows(defaults, "SELECT version FROM component WHERE name='automatic'") == [(None,)]


def file_and_index_lifecycle(defaults):
    source = defaults.catalog_source
    # Directory registration belongs to import. Supply that prerequisite so
    # each catalog leaf can be exercised independently of AST parsing.
    with sqlite3.connect(defaults.catalog_database) as database:
        database.execute("INSERT INTO directory(component_id,path) "
                         "SELECT id,'src' FROM component WHERE name='automatic'")
    add = ("file", "add", source, "--driver", defaults.catalog_driver, "--arg=-std=c++23")
    succeed(defaults, *add)
    for command in ("list", "ls"):
        assert source.name in succeed(defaults, "file", command)
    assert str(source) in succeed(defaults, "file", "show", source)
    succeed(defaults, "file", "set-option", "--match", "source.cpp", "--arg=-DOPTIONAL_CONFIG")
    assert "-DOPTIONAL_CONFIG" in succeed(defaults, "file", "show", source)
    succeed(defaults, "file", "clear-option", "--match", "source.cpp", "--arg=-DOPTIONAL_CONFIG")
    assert "-DOPTIONAL_CONFIG" not in succeed(defaults, "file", "show", source)
    exported = json.loads(succeed(defaults, "component", "compile-commands", "automatic"))
    assert len(exported) == 1 and exported[0]["file"] == str(source)
    file_id = rows(defaults, "SELECT id FROM file WHERE name='source.cpp'")[0][0]
    with sqlite3.connect(defaults.catalog_database) as database:
        database.execute("INSERT INTO matched_symbol_index(usr,qualified_name,file_id,kind) "
                         "VALUES ('c:@F@optional_configuration#','optional_configuration',?,12)",
                         (file_id,))
    found = succeed(defaults, "symbol", "find", "--name", "optional_configuration")
    assert "optional_configuration" in found
    succeed(defaults, "symbol", "index", "clear", "--file-id", file_id)
    assert rows(defaults, "SELECT usr FROM matched_symbol_index") == []
    succeed(defaults, "file", "rm", source)
    succeed(defaults, *add)
    succeed(defaults, "file", "remove", source)
    succeed(defaults, *add)


def directory_and_component_removal(defaults):
    for command in ("list", "ls"):
        assert "automatic" in succeed(defaults, "dir", command)
    directory_id = rows(defaults, "SELECT id FROM directory")[0][0]
    succeed(defaults, "dir", "rm", "--id", directory_id, "--dry-run")
    succeed(defaults, "dir", "rm", "--id", directory_id)
    assert rows(defaults, "SELECT id FROM file") == []
    succeed(defaults, "component", "rm", "--name", "automatic", "--dry-run")
    succeed(defaults, "component", "rm", "--name", "automatic")
    assert rows(defaults, "SELECT id FROM component WHERE name='automatic'") == []


@when("I manage repositories components directories files and matched indexes without database options")
def all_catalog_operations(defaults):
    succeed(defaults, "config", "show")
    assert defaults.value("conf") == str(defaults.catalog_database)
    repository_lifecycle(defaults)
    component_setup(defaults)
    file_and_index_lifecycle(defaults)
    directory_and_component_removal(defaults)


@then("every catalog operation used the automatically configured project database")
def configured_database_reused(defaults):
    assert list(defaults.root.rglob("*.db")) == [defaults.catalog_database]
    assert defaults.configuration_path.read_bytes() == defaults.configuration_bytes
    succeed(defaults, "config", "show")
    assert defaults.value("conf") == str(defaults.catalog_database)


@when("I inspect configuration options on every command leaf")
def inspect_leaf_options(defaults):
    defaults.leaf_checks = []
    for command in LEAVES:
        help_text = succeed(defaults, *command, "--help")
        for option in ("--conf", "--config"):
            option_line = next(line for line in help_text.splitlines()
                               if option + " " in line)
            assert "REQUIRED" not in option_line, (command, option_line)
        for option in ("--conf", "-c", "--config"):
            result = defaults.run(*command, option, "")
            defaults.leaf_checks.append((command, option, result))


@then("every leaf accepts omitted configuration and rejects explicitly empty overrides")
def optional_configuration_contract(defaults):
    for command, option, result in defaults.leaf_checks:
        assert result.returncode == 2, (command, option, result.stderr)
        assert "must not be empty" in result.stderr, (command, option, result.stderr)
    assert not list(defaults.root.rglob("*.db"))
