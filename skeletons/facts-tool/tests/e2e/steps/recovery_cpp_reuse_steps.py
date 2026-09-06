"""Exercise production recovery with C++ lifetime, construction, and dispatch."""
import sqlite3

from pytest_bdd import given, then
from support.recovery import extract, graph, run, seed_match, success


def _variant(context, source: str) -> None:
    context.recovery_sources[1].write_text(source)
    root = context.recovery_sources[0].parent.parent
    success(run(context, "import", "--conf", context.files_database_path,
                "--facts", context.facts_database_path, "-p", root,
                "--component", f"app={context.recovery_sources[0].parent}",
                "--component", f"library={context.recovery_sources[1].parent}"))
    success(extract(context, 0))
    success(seed_match(context))


@given("the S-021 library uses an automatic Guard destructor")
def guard_variant(context):
    _variant(context, '#pragma message("S021_FRONTEND")\n'
             "struct Guard { ~Guard() {} };\n"
             "int bridge() { Guard guard; return 7; }\n")


@given("the S-021 library constructs a std::string")
def string_variant(context):
    _variant(context, '#pragma message("S021_FRONTEND")\n'
             "#include <string>\n"
             'int bridge() { std::string value = "s021"; return value.size(); }\n')


@given("the S-021 library uses virtual Base and Derived dispatch")
def virtual_variant(context):
    _variant(context, '#pragma message("S021_FRONTEND")\n'
             "struct Base { virtual ~Base() = default;"
             " virtual int value() const { return 0; } };\n"
             "struct Derived : Base { int value() const override { return 7; } };\n"
             "int bridge() { Derived value; Base *base = &value;"
             " return base->value(); }\n")


@then("S-021 recovers and reuses the C++ variant three times")
def recover_and_repeat(context):
    first, data = context.recovery_result, context.recovery_graph
    success(first)
    recovery = data["recovery"]
    assert not recovery["failed"], recovery
    assert any(item["component"] == "library" for item in recovery["attempted"]), recovery
    assert first.stderr.count("warning: S021_FRONTEND") == 1, first.stderr
    for _ in range(3):
        result, data = graph(context)
        success(result)
        assert data["recovery"]["attempted"] == [], data["recovery"]
        assert data["recovery"]["reused"], data["recovery"]
        assert result.stderr.count("warning: S021_FRONTEND") == 1, result.stderr
    with sqlite3.connect(context.facts_database_path) as db:
        names = [row[0] for row in db.execute("SELECT name FROM s021_generation")]
    assert names.count("bridge") == 1, names
    with sqlite3.connect(context.files_database_path) as db:
        current_index = db.execute(
            "SELECT * FROM matched_symbol_index ORDER BY usr,file_id"
        ).fetchall()
    assert current_index == context.recovery_index_before
