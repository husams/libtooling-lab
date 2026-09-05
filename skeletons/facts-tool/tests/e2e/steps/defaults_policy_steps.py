import shlex
import sqlite3
from pytest_bdd import given, when, then, parsers
from steps.defaults_validation_steps import consume

@given("an isolated defaults project")
def isolated(defaults):
    assert defaults.cwd.is_dir()

@when(parsers.parse('"{family}" receives empty configuration options'))
def empty_options(defaults, family):
    defaults.before = defaults.snapshot()
    defaults.empty_results = []
    for option in ("--conf", "--config"):
        defaults.args = [option, ""]
        if family == "config":
            defaults.run("config", "show", *defaults.args)
        else:
            consume(defaults, family)
        defaults.empty_results.append(defaults.last)

@then("all empty options fail before filesystem mutation")
def empty_results(defaults):
    assert all(r.returncode == 2 and "must not be empty" in r.stderr
               for r in defaults.empty_results), defaults.empty_results
    assert defaults.snapshot() == defaults.before

@when(parsers.parse('configuration inspection receives empty "{variable}"'))
def empty_environment(defaults, variable):
    defaults.env[variable] = ""
    defaults.before = defaults.snapshot()
    defaults.show()

@when(parsers.parse('missing storage receives "{operation}"'))
def write_missing(defaults, operation):
    defaults.write(conf_root=str(defaults.root / "store"), conf_template="catalog.db")
    defaults.database = defaults.root / "store/catalog.db"
    defaults.run(*shlex.split(operation))

@then("storage has one owner and normal catalog validation ran")
def initialized(defaults):
    assert defaults.last.returncode == 1, defaults.last
    assert "configuration error" not in defaults.last.stderr
    assert "database not found" not in defaults.last.stderr
    assert defaults.database.is_file()
    with sqlite3.connect(defaults.database) as database:
        assert database.execute("SELECT project_root FROM generated_conf_owner").fetchall() == [
            (str(defaults.cwd),)]
        assert database.execute("SELECT count(*) FROM file").fetchone()[0] == 0

@when(parsers.parse('"{family}" runs with invalid unused path settings'))
def unused(defaults, family):
    import json
    path = defaults.files["project"]
    values = json.loads(path.read_text())
    values.update(conf_root=None, conf_template={"invalid": "../outside"})
    path.write_text(json.dumps(values))
    defaults.compiler.require_effect(3)
    for _ in range(2):
        result = defaults.compiler.run(family)
        assert result.returncode == 0, result.stderr

@then("partial provenance identifies the selected file")
def partial(defaults):
    assert "parser: YAML / yaml-cpp 0.9.0" in defaults.last.stdout
    assert str(defaults.files["project"]) + " [found]" in defaults.last.stdout
    assert "configuration in " + str(defaults.files["project"]) in defaults.last.stderr
