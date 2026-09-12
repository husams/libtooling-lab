import json

from pytest_bdd import then

from support.database import require
from support.s022 import graph, named_edges, run


def entry(context, name):
    result = run([str(context.facts_tool), "analyse", "call-graph-entry",
                  "--conf", str(context.files_database_path),
                  "--facts", str(context.facts_database_path),
                  "--function", f"s022_fixture::{name}", "--format", "json"])
    require(result.returncode == 0, result.stdout + result.stderr)
    return json.loads(result.stdout)


@then("S-027 entries preserve S-022 invocations and unresolved diagnostics")
def callable_entries(context):
    before = graph(context, "s022_fixture::run")
    kinds = {edge["semantic_kind"] for edge, _ in named_edges(before)
             if edge["depth"] == 1}
    require({"constructor", "destructor", "lambda"}.issubset(kinds), str(kinds))
    generated = entry(context, "run")
    require(generated["entry_available"] and generated["is_leaf"] is False,
            str(generated))
    unresolved = entry(context, "unresolved")
    require(unresolved["entry_available"] and unresolved["is_leaf"] is False and
            unresolved["coverage"]["unresolved_targets"] == 1, str(unresolved))
    root = context.fixture_root / "s022"
    result = run([str(context.facts_tool), "extract", "-v", "0", "--force",
                  "--conf", str(context.files_database_path), "--output",
                  str(context.facts_database_path), str(root / "beta/service.cpp"),
                  str(root / "alpha/entry.cpp")])
    require(result.returncode == 0, result.stdout + result.stderr)
    require("coverage.unsupported_semantics kind=indirect-call" in result.stderr,
            result.stderr)
    after = graph(context, "s022_fixture::run")
    require(after["edges"] == before["edges"], "entry regeneration changed invocation edges")
    require(entry(context, "run")["symbol_id"] == generated["symbol_id"] and
            entry(context, "run")["entry_available"], "repeated extraction lost the entry")
