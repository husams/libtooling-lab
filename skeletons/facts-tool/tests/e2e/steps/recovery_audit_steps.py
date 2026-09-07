"""Observe committed body generation, rather than trusting progress counters."""
import sqlite3
from pytest_bdd import given, then
from support.recovery import extract, success


@given("S-021 body generation is recorded by the fixture")
def audit(context):
    with sqlite3.connect(context.facts_database_path) as db:
        db.execute("CREATE TABLE s021_generation (name TEXT)")
        for event in ("INSERT", "UPDATE"):
            db.execute(f"CREATE TRIGGER s021_generated_{event} AFTER {event} ON definition "
                       "BEGIN INSERT INTO s021_generation SELECT qualified_name FROM "
                       "symbol WHERE id=NEW.symbol_id; END")


@then("S-021 generates the library once and never regenerates the app")
def generated(context):
    with sqlite3.connect(context.facts_database_path) as db:
        names = [row[0] for row in db.execute("SELECT name FROM s021_generation")]
    assert names.count("bridge") == names.count("leaf") == 1, names
    assert "root" not in names and "unused" not in names, names


@then("S-021 produces no new body generation")
def none_generated(context):
    with sqlite3.connect(context.facts_database_path) as db:
        assert db.execute("SELECT * FROM s021_generation").fetchall() == []


@given("the S-021 library contains no requested definition")
def absent(context):
    context.recovery_sources[1].write_text("int unrelated() { return 3; }\n")


@then("S-021 keeps an honest unavailable definition boundary")
def unavailable(context):
    success(context.recovery_result)
    data = context.recovery_graph
    assert not data["recovery"]["failed"], data
    bridge = next(node for node in data["nodes"] if node["name"] == "bridge")
    assert bridge["definition_availability"] != "available", bridge
    attempts = data["recovery"]["attempted"]
    assert any(entry["component"] == "library" for entry in attempts), attempts
    assert len({entry["tu_file_id"] for entry in attempts}) == len(attempts), attempts


@given("the S-021 app needs two definitions in the same library TU")
def two_targets(context):
    context.recovery_sources[0].write_text(
        "int bridge(); int second(); int root() { return bridge() + second(); }\n")
    context.recovery_sources[1].write_text(
        context.recovery_library_body + "int second() { return leaf(); }\n")
    success(extract(context, 0))


@given("the S-021 missing definition is in another TU of the app component")
def same_component(context):
    context.recovery_sources[1].write_text("int unrelated() { return 0; }\n")
    context.recovery_alternative.write_text(
        "int leaf() { return 7; } int bridge() { return leaf(); }\n")


@then("S-021 finds the same-component definition without extracting unrelated TUs")
def same_component_found(context):
    from support.recovery import edge_names
    success(context.recovery_result)
    assert ("bridge", "leaf") in edge_names(context.recovery_graph)
    with sqlite3.connect(context.facts_database_path) as db:
        names = [row[0] for row in db.execute("SELECT name FROM s021_generation")]
    assert sorted(names) == ["bridge", "leaf"], names


@given("S-021 also stores an unrelated root with a missing definition")
def unrelated_root(context):
    from support.recovery import run
    context.recovery_alternative.write_text(
        "int absent(); int unused() { return absent(); }\n")
    success(run(context, "extract", "-v", "0", "--conf", context.files_database_path,
                "--output", context.facts_database_path, context.recovery_alternative))
