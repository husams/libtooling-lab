from pytest_bdd import then

from support.database import require
from support.entries import extract, graph, lookup, succeed


@then("S-027 regenerated bodies replace obsolete calls and unresolved sites")
def replace_evidence(context):
    before = lookup(context, "left")
    require(lookup(context, "indirect")["coverage"]["unresolved_targets"],
            "fixture has no observed unresolved call")
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
    require(not lookup(context, "indirect")["coverage"]["unresolved_targets"],
            "obsolete unresolved call survived regeneration")
