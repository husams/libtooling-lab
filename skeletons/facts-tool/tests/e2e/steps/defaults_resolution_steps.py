import pytest
from pytest_bdd import given, when, then, parsers
from support.defaults_fixture import Defaults

@pytest.fixture
def defaults(context, tmp_path):
    return Defaults(context.facts_tool, tmp_path)

@given(parsers.parse('defaults exist at tiers "{tiers}"'))
def tiers(defaults, tiers):
    defaults.tiers(tiers)

@when("I inspect the effective configuration twice")
def inspect_twice(defaults):
    defaults.before = defaults.snapshot()
    first = defaults.show()
    assert first.returncode == 0, first.stderr
    second = defaults.show()
    assert (second.returncode, second.stdout) == (first.returncode, first.stdout)

@then(parsers.parse('the selected database comes from "{tier}"'))
def selected(defaults, tier):
    assert defaults.value("conf") == str(defaults.expected(tier)), defaults.last.stdout
    assert defaults.value("project_root") == str(defaults.cwd.resolve())
    assert "yaml-cpp 0.9.0" in defaults.last.stdout
    assert defaults.snapshot() == defaults.before

@given("every YAML tier contains a different value")
def all_tiers(defaults):
    defaults.tiers("config-file,project,user")

@then("extra_args concatenates user, then project, then config-file")
def merged_order(defaults):
    assert defaults.value("extra_args") == "[-DTIER=user] [-DTIER=project] [-DTIER=config-file]"
    for tier in ("user", "project", "config-file"):
        assert str(defaults.files[tier]) in defaults.value("extra_args_source")
    assert defaults.snapshot() == defaults.before

@given("a malformed lower-priority file")
def malformed_lower(defaults):
    defaults.files["user"].parent.mkdir(parents=True, exist_ok=True)
    defaults.files["user"].write_text("extra_args: [bad")

@given("an unreadable selected YAML directory")
def unreadable(defaults):
    defaults.files["project"].mkdir()

@when("I attempt configuration inspection")
def attempt(defaults):
    defaults.before = defaults.snapshot()
    defaults.show()

@then(parsers.parse('configuration fails with "{message}"'))
def failure(defaults, message):
    assert defaults.last.returncode == 3, defaults.last.stderr
    assert message in defaults.last.stderr, defaults.last.stderr
    assert defaults.snapshot() == defaults.before
    assert not list(defaults.root.rglob("*.db"))

@given("an explicit missing configuration selector")
def missing(defaults):
    defaults.args = ["--config", str(defaults.root / "missing.yaml")]

@then("search diagnostics include setting path outcomes and remedy")
def diagnostics(defaults):
    output = defaults.last.stderr
    assert "configuration in " in output and "; searched:" in output, output
    assert "[absent]" in output, output
    assert "; set --config or FACTS_TOOL_CONFIG" in output, output
