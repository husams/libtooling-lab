from pytest_bdd import then

from support.database import query, require
from support.entries import extract, graph, lookup, succeed


@then("S-027 regenerated bodies replace obsolete calls and pointer sites")
def replace_evidence(context):
    before = lookup(context, "left")
    require(lookup(context, "indirect")["coverage"]["pointer_calls"],
            "fixture has no observed pointer call")
    source = context.entry_sources[0]
    source.write_text(source.read_text().replace(
        "int left() { return shared(); }", "int left() { return leaf(); }").replace(
        "return callback();", "return 0;"))
    succeed(extract(context))
    entry = lookup(context, "left")
    require(entry["entry_available"] and entry["symbol_id"] == before["symbol_id"],
            str(entry))
    names = graph(context, "left")["nodes"]
    require(names == {"s027::left", "s027::leaf"}, str(names))
    require(not lookup(context, "indirect")["coverage"]["pointer_calls"],
            "obsolete pointer call survived regeneration")
    require(not query(context.facts_database_path,
                      "SELECT 1 FROM callgraph_pointer_call_site AS site "
                      "JOIN symbol AS source ON source.id=site.source_id "
                      "WHERE source.qualified_name='s027::indirect'"),
            "obsolete pointer site survived regeneration")
    require(not query(context.facts_database_path,
                      "SELECT 1 FROM relation AS edge "
                      "JOIN symbol AS source ON source.id=edge.source_id "
                      "WHERE source.qualified_name='s027::indirect' AND edge.kind=24"),
            "obsolete pointer relation survived regeneration")
