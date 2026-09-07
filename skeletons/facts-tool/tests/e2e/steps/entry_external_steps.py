from pytest_bdd import then, when

from support.database import query, require
from support.entries import extract, graph, lookup, match, run, succeed


@then("the S-027 external call retains its identity and site")
def external(context):
    entry = lookup(context, "boundary")
    target = lookup(context, "external")
    require(entry["entry_available"] and entry["external_targets"], str(entry))
    require(not target["entry_available"] and target["is_leaf"] is None, str(target))
    context.entry_external_id = target["symbol_id"]
    context.entry_external_graph = graph(context, "boundary")
    context.entry_external_sites = query(context.facts_database_path,
        "SELECT source_id,destination_id,kind,position,file_id,offset "
        "FROM relation_site WHERE destination_id=?", (int(target["symbol_id"]),))
    references = query(context.facts_database_path,
        "SELECT source_id,destination_id,kind,position,file_id,offset,external_symbol_id "
        "FROM callgraph_external_reference WHERE external_symbol_id=?",
        (int(target["symbol_id"]),))
    require(references and all(row[1] == row[-1] for row in references), str(references))
    require([row[:6] for row in references] == context.entry_external_sites, str(references))


@when("the S-027 library component is extracted")
def library(context):
    succeed(extract(context, library=True))
    require(not lookup(context, "boundary")["entry_available"],
            "library-only extraction must invalidate existing caller entries")


@then("the S-027 external identity resolves without losing callers or sites")
def resolved(context):
    target = lookup(context, "external")
    require(target["symbol_id"] == context.entry_external_id and
            target["entry_available"] and target["is_leaf"], str(target))
    require(lookup(context, "boundary")["external_targets"] == [], "boundary unresolved")
    sites = query(context.facts_database_path,
        "SELECT source_id,destination_id,kind,position,file_id,offset "
        "FROM relation_site WHERE destination_id=?", (int(target["symbol_id"]),))
    require(sites == context.entry_external_sites, "resolution changed sites")
    after = graph(context, "boundary")
    def identities(value):
        return [(edge["source_id"], edge["target_id"], edge["kind"], edge["file_id"],
                 edge["offset"]) for edge in value["edges"]]
    require(identities(after) == identities(context.entry_external_graph), str(after))


@then("S-027 multi-source extraction retains shared callers and resolved targets")
def multisource(context):
    for sources in (context.entry_sources, list(reversed(context.entry_sources))):
        succeed(run(context, "extract", "-v", "0", "--conf", context.files_database_path,
                    "--output", context.facts_database_path, *sources))
        left = graph(context, "left")
        require({"s027::left", "s027::shared", "s027::leaf"}.issubset(left["nodes"]), str(left))
        require(lookup(context, "external")["entry_available"], "library entry lost")
        require(lookup(context, "boundary")["external_targets"] == [], "resolved target lost")


@then("S-027 narrow external calls retain references without certifying a body")
def narrow_external(context):
    before = lookup(context, "boundary")["external_targets"]
    succeed(match(context, 'callExpr(callee(functionDecl(hasName("s027::external"))'
                           '.bind("callee"))).bind("call")'))
    entry = lookup(context, "boundary")
    require(not entry["entry_available"] and entry["external_targets"] == before,
            str(entry))
