import sqlite3
from pytest_bdd import given, when, then, parsers
from support.extra_args_retention import Retention
from support.retention_assertions import commands, effects


@given(parsers.parse('an independent requirements project with "{representation}" entries and "{driver}" driver'))
def fixture(defaults, representation, driver):
    defaults.retention = Retention(defaults, representation, driver)


@given(parsers.parse('retention YAML extra_args are "{mode}"'))
def configure(defaults, mode):
    defaults.retention.configure(mode)


@when(parsers.parse('I exercise retention for "{family}" {repeats:d} times with "{cli}" CLI additions'))
def exercise(defaults, family, repeats, cli):
    defaults.retention.execute(family, repeats, cli == "explicit")


@then('every run proves independent compiler effects and preserves both stored base commands')
def verify(defaults):
    c = defaults.retention
    assert c.results
    assert all(result.returncode == 0 for _, result in c.results)
    commands(c)


@when('I change retention YAML and run both consumers without reimport')
def changed(defaults):
    c = defaults.retention
    c.execute("extract")
    c.run("dependency")
    effects(c, "dependency")
    c.configure(version="NEW")
    # Fresh facts outputs ensure a previous extraction cannot mask missing
    # symbols and an old header's persisted edges cannot mask new effects.
    for family in ("extract", "dependency"):
        (c.d.root / (family + ".db")).unlink()
        c.run(family)
        effects(c, family)
        commands(c)


@given('retention JSON defines VALUE=1 and YAML appends VALUE=2')
def conflict(defaults):
    defaults.retention.configure(conflict=True)


@then('VALUE=2 takes effect while VALUE=1 remains in both stored commands')
def conflicting_value(defaults):
    c = defaults.retention
    with sqlite3.connect(c.d.root / "extract.db") as db:
        names = {row[0] for row in db.execute("SELECT qualified_name FROM symbol")}
    assert {"ValueTwoA", "ValueTwoB"} <= names
    for entry in c.stored():
        assert entry["arguments"].count("-DVALUE=1") == 1
        assert "-DVALUE=2" not in entry["arguments"]


@when(parsers.parse('I append runtime CLI arguments to "{family}" after import'))
def runtime_cli(defaults, family):
    c = defaults.retention
    c.execute("import")
    c.run(family, runtime=True)
    effects(c, family)
    commands(c)
    with sqlite3.connect(c.d.root / (family + ".db")) as db:
        if family == "extract":
            names = {row[0] for row in db.execute("SELECT qualified_name FROM symbol")}
            assert {"RuntimeA", "RuntimeB"} <= names
        else:
            with sqlite3.connect(c.db) as project:
                markers = {row[0] for row in project.execute(
                    "SELECT id FROM file WHERE name='runtime words.hpp'")}
            targets = {row[0] for row in db.execute("SELECT dst_file_id FROM include_dependency")}
            assert len(markers) == 2 and markers <= targets
