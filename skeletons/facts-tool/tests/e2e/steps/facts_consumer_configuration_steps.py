import json

from pytest_bdd import given, parsers, then, when


@given(parsers.parse('a facts consumer project configured through "{tier}"'))
def consumer_project(defaults, context, tier):
    root = defaults.cwd
    source = root / "configured.cpp"
    source.write_text("int configured_leaf() { return 7; }\n"
                      "int configured_root() { return configured_leaf(); }\n")
    (root / "compile_commands.json").write_text(json.dumps([{
        "directory": str(root), "file": str(source),
        "arguments": [str(context.compiler), "-std=c++23", "-c", str(source)]}]))
    defaults.consumer_project = (defaults.expected("builtin") if tier == "builtin"
                                  else root / "project.db")
    defaults.consumer_facts = root / "facts.db"
    defaults.consumer_source = source
    location = "config-file" if tier == "environment" else tier
    if tier != "builtin":
        defaults.write(location, conf_root=str(root), conf_template="project.db",
                       facts_template="{project_root}/facts.db")
    if tier == "environment":
        defaults.env["FACTS_TOOL_CONFIG"] = str(defaults.files[location])
    for args in (("import", "-p", root),
                 ("extract", "-o", defaults.consumer_facts, source)):
        result = defaults.run(*args)
        assert result.returncode == 0, result.stdout + result.stderr
    assert defaults.consumer_project.exists()
    assert defaults.consumer_facts.exists()
    # Discovery must also work from descendants of the project root.
    defaults.cwd = root / "nested"
    defaults.cwd.mkdir()


def arguments(defaults, command, output):
    commands = {
        "match": ["match", "--matcher",
                  'functionDecl(hasName("configured_root")).bind("symbol")'],
        "call-graph": ["analyse", "call-graph", "--function", "configured_root"],
        "call-graph-entry": ["analyse", "call-graph-entry", "--function", "configured_root"],
        "recovery": ["analyse", "call-graph", "--function", "configured_root", "--recover-missing"],
        "symbol": ["symbol", "show", "configured_root"],
    }
    args = commands[command]
    if output in ("explicit", "empty"):
        args += ["--facts", "" if output == "empty" else defaults.consumer_facts]
    return args


@when(parsers.parse('configured consumer "{command}" runs with "{output}" facts'))
def run_consumer(defaults, command, output):
    defaults.consumer_command = command
    defaults.before = defaults.snapshot()
    defaults.run(*arguments(defaults, command, output))


@then("the consumer uses the configured project and selected facts")
def configured_success(defaults):
    result = defaults.last
    assert result.returncode == 0, result.stdout + result.stderr
    if defaults.consumer_command in ("match", "symbol", "call-graph-entry"):
        assert "configured_root" in result.stdout
    else:
        assert "call graph run" in result.stdout
    if defaults.consumer_command == "symbol":
        assert str(defaults.consumer_source) in result.stdout


@given("its configured project database is missing")
def remove_project(defaults):
    defaults.consumer_project.unlink()


@then("the consumer reports the missing configured project without mutation")
def missing_project(defaults):
    assert defaults.last.returncode != 0
    assert str(defaults.consumer_project) in defaults.last.stderr
    assert defaults.snapshot() == defaults.before


@then("the consumer rejects the empty facts override without mutation")
def empty_override(defaults):
    assert defaults.last.returncode == 2, defaults.last.stderr
    assert "--facts must not be empty" in defaults.last.stderr
    assert defaults.snapshot() == defaults.before


@given("its YAML project path is replaced with a missing database")
def wrong_yaml_project(defaults):
    defaults.write(conf_root=str(defaults.consumer_project.parent),
                   conf_template="missing-project.db",
                   facts_template="{project_root}/facts.db")


@when(parsers.parse('configured consumer "{command}" runs with its original project override'))
def override_project(defaults, command):
    defaults.consumer_command = command
    defaults.run(*arguments(defaults, command, "default"),
                 "--conf", defaults.consumer_project)
