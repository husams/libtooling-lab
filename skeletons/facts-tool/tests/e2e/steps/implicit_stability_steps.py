import json
import sqlite3

from pytest_bdd import given, then, when
from steps.implicit_allocation_target_steps import extract_implicit, implicit_fixture
from steps.implicit_matrix_steps import identities
from support.database import query, require


@given("two implicit allocation translation units", target_fixture="implicit_source")
def two_sources(context):
    original = implicit_fixture(context)
    second = context.run_root_path / "second.cpp"
    second.write_text(original.read_text().replace("seed_new", "second_seed")
                      .replace("explicit_new", "second_explicit"))
    path = context.run_root_path / "compile_commands.json"
    commands = json.loads(path.read_text())
    other = dict(commands[0])
    other["file"] = str(second)
    other["arguments"] = [str(second) if a == str(original) else a
                          for a in other["arguments"]]
    path.write_text(json.dumps(commands + [other]))
    return original


@when("both implicit translation units are repeatedly extracted")
def repeated(context):
    extract_implicit(context)
    require(context.last_returncode == 0, context.last_output)
    context.first_identities = identities(context)
    context.initial_symbols = query(context.facts_database_path,
                                    "SELECT * FROM relation_site ORDER BY source_id")
    extract_implicit(context)


@then("the target identity and both real call sites remain stable")
def stable(context):
    require(context.last_returncode == 0, context.last_output)
    require(identities(context) == context.first_identities, "IDs changed")
    require(query(context.facts_database_path, "SELECT * FROM relation_site ORDER BY source_id")
            == context.initial_symbols, "sites changed")
    calls = query(context.facts_database_path,
                  "SELECT s.qualified_name,COUNT(*) FROM relation_site r "
                  "JOIN symbol s ON s.id=r.source_id JOIN symbol t ON t.id=r.destination_id "
                  "WHERE r.kind=1 AND t.qualified_name='operator new' GROUP BY s.qualified_name")
    require(calls == [("explicit_new", 1), ("second_explicit", 1)], str(calls))


def snapshot(context):
    with sqlite3.connect(context.facts_database_path) as db:
        tables = db.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall()
        return {name: sorted(db.execute(f'SELECT * FROM "{name}"').fetchall(), key=repr)
                for name, in tables}


@when("a new implicit target write fails after a committed baseline")
def failure(context, implicit_source):
    extract_implicit(context)
    require(context.last_returncode == 0, context.last_output)
    path = context.run_root_path / "failing.cpp"
    path.write_text("int* another_seed() { return new int[3]; }\n"
                    "void* another_call(decltype(sizeof(0)) n) { return ::operator new[](n); }\n")
    commands = [{"file": str(path), "directory": str(path.parent), "arguments":
                 [str(context.compiler), "-std=c++23", "-c", str(path)]}]
    (context.run_root_path / "compile_commands.json").write_text(json.dumps(commands))
    with sqlite3.connect(context.facts_database_path) as db:
        db.execute("CREATE TRIGGER fail_implicit BEFORE INSERT ON symbol "
                   "WHEN NEW.qualified_name='operator new[]' BEGIN "
                   "SELECT RAISE(ABORT,'forced compiler symbol failure'); END")
    context.first_identities = snapshot(context)
    extract_implicit(context)


@then("the target failure reports its identity and rolls back every facts table")
def rolled_back(context):
    require(context.last_returncode != 0, context.last_output)
    for text in ("operator new[]", "c:@F@operator new[]#", "constraint", "rollback"):
        require(text in context.last_output, context.last_output)
    require(snapshot(context) == context.first_identities, "failed extraction changed facts")
