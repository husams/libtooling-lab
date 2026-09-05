from pytest_bdd import given, when, then, parsers
from steps.cli_yaml_paths_steps import _project


@given("a symbol path precedence fixture")
def symbol_fixture(defaults):
    root, source = _project(defaults)
    yaml_path = root / "yaml-facts.db"
    first = defaults.run("extract", "-o", yaml_path, source)
    assert first.returncode == 0, first.stderr
    source.write_text("struct ExplicitSymbol {};\n")
    explicit = root / "explicit-facts.db"
    second = defaults.run("extract", "-o", explicit, source)
    assert second.returncode == 0, second.stderr
    defaults.path_project = (root, source)
    defaults.yaml_path, defaults.explicit_path = yaml_path, explicit


@when(parsers.parse('I run symbol "{action}" with "{placement}" and "{alias}" facts'))
def run_symbol(defaults, action, placement, alias):
    args = ["symbol"]
    if placement == "group": args += [alias, defaults.explicit_path]
    args += [action]
    if placement == "leaf": args += [alias, defaults.explicit_path]
    if action == "show": args += ["ExplicitSymbol"]
    defaults.last = defaults.run(*args)


@then("the explicit facts path wins")
def assert_facts(defaults):
    assert defaults.last.returncode == 0, defaults.last.stderr
    assert "ExplicitSymbol" in defaults.last.stdout
    assert "YamlSymbol" not in defaults.last.stdout


@when(parsers.parse('I run symbol "{action}" with an empty leaf --facts'))
def run_empty_facts(defaults, action):
    args = ["symbol", action, "--facts", ""]
    if action == "show": args.append("ExplicitSymbol")
    defaults.last = defaults.run(*args)


@then("the explicit empty facts path is rejected")
def assert_empty_facts(defaults):
    assert defaults.last.returncode != 0
    assert "facts" in (defaults.last.stdout + defaults.last.stderr).lower()


@when("I run symbol browser with a missing --facts path")
def run_missing_browser_facts(defaults):
    root, _ = defaults.path_project
    defaults.last = defaults.run("symbol", "browser", "--facts",
                                 root / "missing-facts.db")


@then("the explicit missing facts path is rejected")
def assert_missing_browser_facts(defaults):
    assert defaults.last.returncode != 0
    assert "facts" in (defaults.last.stdout + defaults.last.stderr).lower()
