"""Exercise production recovery with C++ lifetime, construction, and dispatch."""
import sqlite3

from pytest_bdd import given, then
from support.recovery import extract, graph, run, seed_match, success
from support.recovery_facts import component_tu


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
    first, run_info = context.recovery_result, context.recovery_run
    success(first)
    failed = [row for row in run_info["recovery"] if row[1] == "failed"]
    assert not failed, run_info["recovery"]
    library_tu = component_tu(context, "library")
    assert any(row[0] == library_tu and row[1] == "attempted"
               for row in run_info["recovery"]), run_info["recovery"]
    assert first.stderr.count("warning: S021_FRONTEND") == 1, first.stderr
    for _ in range(3):
        result, run_info = graph(context, verbosity=1)
        success(result)
        attempted = [row for row in run_info["recovery"] if row[1] == "attempted"]
        assert not attempted, run_info["recovery"]
        assert any(row[1] == "reused" for row in run_info["recovery"]), run_info["recovery"]
        assert result.stderr.count("warning: S021_FRONTEND") == 1, result.stderr
        # Freshness validation re-parses on every invocation, so unsupported-
        # semantics coverage notices repeat on reuse too; only fact writes
        # ("symbol(s) recorded") distinguish a reuse from a real attempt.
        assert "symbol(s) recorded" not in result.stderr, result.stderr
    with sqlite3.connect(context.facts_database_path) as db:
        names = [row[0] for row in db.execute("SELECT name FROM s021_generation")]
    assert names.count("bridge") == 1, names
    with sqlite3.connect(context.files_database_path) as db:
        current_index = db.execute(
            "SELECT * FROM matched_symbol_index ORDER BY usr,file_id"
        ).fetchall()
    assert current_index == context.recovery_index_before
