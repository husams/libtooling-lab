from pytest_bdd import given, when, then, parsers
from support.defaults_compiler import CompileDefaults

@given(parsers.parse('a compile fixture using "{override}" override and "{input}" commands'))
def fixture(defaults, override, input):
    defaults.compiler = CompileDefaults(defaults, override, input)

@when(parsers.parse('I run "{family}" with ordered YAML and CLI tokens twice'))
def compile_twice(defaults, family):
    c = defaults.compiler
    c.require_effect(3)
    for _ in range(2):
        result = c.run(family)
        assert result.returncode == 0, result.stdout + result.stderr
    c.family = family

@then("compiler effects and stored base arguments prove ordering without accumulation")
def correct_order(defaults):
    defaults.compiler.verify()

@when("I change YAML defaults between extractions")
def change(defaults):
    c = defaults.compiler
    for value in (2, 4):
        defaults.write(extra_args=[f"-DVALUE={value}", "-include", str(c.header)])
        c.require_effect(value)
        result = c.run("extract", cli=False)
        assert result.returncode == 0, result.stdout + result.stderr

@then("changed defaults apply once without changing stored arguments")
def unchanged(defaults):
    defaults.compiler.verify()

@when(parsers.parse('I run configuration-independent "{command}"'))
def independent(defaults, command):
    defaults.env.pop("HOME", None)
    defaults.before = defaults.snapshot()
    if command == "help": defaults.run("extract", "--help")
    else: defaults.run("symbol", "list", "--facts", str(defaults.root / "absent-facts.db"))

@then("no configuration discovery occurs")
def no_discovery(defaults):
    assert defaults.last.returncode in (0, 1), defaults.last.stderr
    assert "configuration error" not in defaults.last.stderr
    assert defaults.snapshot() == defaults.before
