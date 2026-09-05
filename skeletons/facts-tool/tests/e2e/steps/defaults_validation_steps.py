from pytest_bdd import given, when, then, parsers
from support.defaults_invalid import INVALID_YAML

@given(parsers.parse('a selected YAML file with "{case}"'))
def invalid(defaults, case):
    defaults.files["project"].write_text(INVALID_YAML[case])

@given(parsers.parse('a "{kind}" direct database override'))
def override(defaults, kind):
    defaults.direct = defaults.root / "direct.db"
    if kind == "cli": defaults.args += ["--conf", str(defaults.direct)]
    if kind == "env": defaults.env["FACTS_TOOL_CONF"] = str(defaults.direct)

@when(parsers.parse('I invoke the "{family}" configuration consumer'))
def consume(defaults, family):
    commands = {
        "import": ["import", "source.cpp"],
        "extract": ["extract", "-o", str(defaults.root / "facts.db"), "source.cpp"],
        "dependency": ["analyse", "dependency", "-o", str(defaults.root / "facts.db"), "source.cpp"],
        "repo": ["repo", "list"], "component": ["component", "list"],
        "dir": ["dir", "list"], "file": ["file", "list"],
        "symbol": ["symbol", "list", "-f", str(defaults.root / "facts.db")],
    }
    defaults.before = defaults.snapshot()
    defaults.run(*commands[family], *defaults.args)

@then("the direct database is required without reading YAML")
def direct_only(defaults):
    assert defaults.last.returncode == 1, defaults.last.stderr
    assert defaults.last.stderr == ("facts-tool: project configuration database not found: "
                                   + str(defaults.direct) + "\n")
    assert defaults.snapshot() == defaults.before

@then("the generated database is required without creating files")
def generated_missing(defaults):
    assert defaults.last.returncode == 1, defaults.last.stderr
    assert defaults.last.stderr == ("facts-tool: project configuration database not found: "
                                   + str(defaults.expected("project")) + "\n")
    assert defaults.snapshot() == defaults.before

@given(parsers.parse('the selected YAML contains "{content}"'))
def contents(defaults, content):
    defaults.files["project"].write_text("" if content == "empty" else content)

@then("all absent settings retain built-in provenance")
def absent(defaults):
    assert defaults.last.returncode == 0, defaults.last.stderr
    assert defaults.value("conf_root_source") == "built-in"
    assert defaults.value("conf_template_source") == "built-in"
    assert defaults.value("extra_args_source") == "built-in"
    assert defaults.value("conf") == str(defaults.expected("builtin"))
    assert defaults.snapshot() == defaults.before
