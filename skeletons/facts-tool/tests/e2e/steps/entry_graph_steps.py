from pytest_bdd import given, then, when

from support.database import query, require
from support.entries import extract, graph, lookup, match, prepare, succeed


@given("an extracted S-027 application component")
def application(context):
    prepare(context)
    succeed(extract(context))


def _shared_target_ids(document):
    return {edge["target_id"] for edge in document["edges"] if edge["target"] == "s027::shared"}


@then("S-027 entries reuse shared nodes and preserve generated leaves")
def shared(context):
    left, right = graph(context, "left"), graph(context, "right")
    shared_ids = [_shared_target_ids(value) for value in (left, right)]
    require(shared_ids[0] == shared_ids[1] and len(shared_ids[0]) == 1, str(shared_ids))
    leaf = lookup(context, "leaf")
    require(leaf["entry_available"] and leaf["is_leaf"] is True, str(leaf))
    require(isinstance(leaf["symbol_id"], str) and
            leaf["symbol_id"] == leaf["graph_node_ref"], str(leaf))
    require(lookup(context, leaf["usr"]) == leaf, "USR lookup differs")
    require(graph(context, "leaf")["edges"] == [], "fabricated leaf call")
    cycle = graph(context, "cycle_a")
    require({"s027::cycle_a", "s027::cycle_b"}.issubset(cycle["nodes"]), str(cycle))


@then("repeated S-027 extraction is idempotent")
def idempotent(context):
    before = graph(context, "left")
    entry = lookup(context)
    succeed(extract(context))
    after = graph(context, "left")
    require(after["edges"] == before["edges"] and after["nodes"] == before["nodes"] and
            lookup(context) == entry, "repeated generation changed shared identity or relations")
    rows = query(context.facts_database_path,
                 "SELECT symbol_id,graph_node_ref FROM callgraph_entry")
    require(len(rows) == len(set(rows)), str(rows))


@when("an S-027 narrow call match runs")
def narrow(context):
    succeed(match(context, 'callExpr(callee(functionDecl(hasName("s027::shared"))'
                           '.bind("callee"))).bind("call")'))


@then("the S-027 caller entry is missing until full regeneration")
def missing(context):
    entry = lookup(context)
    require(entry["entry_available"] is False and entry["is_leaf"] is None and
            entry["graph_node_ref"] is None, str(entry))
    succeed(extract(context))
    regenerated = lookup(context)
    require(regenerated["entry_available"] and regenerated["is_leaf"] is False,
            str(regenerated))


@then("S-027 indirect calls have no guessed external identity")
def indirect(context):
    entry = lookup(context, "indirect")
    require(entry["entry_available"] and entry["external_targets"] == [], str(entry))
    require(entry["coverage"]["unresolved_targets"], str(entry))
    require(entry["is_leaf"] is False, "unresolved calls were reported as a leaf")


@then("S-027 freshness and graph truncation remain separate from entries")
def coverage(context):
    entry = lookup(context, "left")
    require(entry["entry_available"] and entry["coverage"]["freshness"] == "unknown",
            str(entry))
    partial = graph(context, "left", "--max-depth", "1")
    require(partial["row"]["status"] == "truncated" and
            lookup(context, "left") == entry, str(partial))
