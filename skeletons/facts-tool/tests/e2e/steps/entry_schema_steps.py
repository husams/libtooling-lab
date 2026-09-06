import sqlite3

from pytest_bdd import then

from support.database import query, require
from support.entries import extract, lookup, match, run, succeed


@then("S-027 version ten migration preserves graph identities without inferring entries")
def migration(context):
    database = context.facts_database_path
    before = query(database, "SELECT id,usr FROM symbol ORDER BY id")
    sites = query(database, "SELECT * FROM relation_site ORDER BY source_id,destination_id")
    with sqlite3.connect(database) as connection:
        connection.execute("DROP TABLE callgraph_external_reference")
        connection.execute("DROP TABLE callgraph_entry")
        connection.execute("DROP TABLE callgraph_unresolved_site")
        connection.execute("DROP TABLE facts_project_provenance")
        connection.execute("PRAGMA user_version=10")
    succeed(match(context, 'functionDecl(hasName("absent_s027")).bind("symbol")'))
    require(query(database, "PRAGMA user_version") == [(11,)], "wrong migration version")
    require(query(database, "SELECT * FROM callgraph_entry") == [], "migration inferred entries")
    require(query(database, "SELECT * FROM callgraph_external_reference") == [],
            "migration inferred external references")
    require(query(database, "SELECT id,usr FROM symbol ORDER BY id") == before, "IDs changed")
    require(query(database, "SELECT * FROM relation_site ORDER BY source_id,destination_id") == sites,
            "migration changed sites")
    require(not lookup(context)["entry_available"], "lookup fabricated migrated entry")


@then("S-027 future facts versions reject extraction without writes")
def future(context):
    database = context.facts_database_path
    with sqlite3.connect(database) as connection:
        connection.execute("PRAGMA user_version=999")
    before = database.read_bytes()
    result = extract(context)
    require(result.returncode != 0, result.stdout + result.stderr)
    require(database.read_bytes() == before, "future schema was mutated")


@then("S-027 lookup preserves database bytes and rejects invalid selectors")
def readonly(context):
    before = [path.read_bytes() for path in
              (context.files_database_path, context.facts_database_path)]
    lookup(context)
    for selector, diagnostic in (("missing", "root-not-found"),
                                 ("s027::overloaded", "ambiguous-root")):
        result = run(context, "analyse", "call-graph-entry", "-v", "0", "--conf",
                     context.files_database_path, "--facts", context.facts_database_path,
                     "--function", selector, "--format", "json")
        require(result.returncode == 2 and diagnostic in result.stderr,
                result.stdout + result.stderr)
    require([path.read_bytes() for path in
             (context.files_database_path, context.facts_database_path)] == before,
            "entry lookup changed database bytes")


@then("S-027 tables have exactly their contracted columns and cascading references")
def schema(context):
    database = context.facts_database_path
    require(query(database, "PRAGMA user_version") == [(11,)], "wrong facts version")
    for table, columns in (
        ("callgraph_entry", ["symbol_id", "graph_node_ref"]),
        ("callgraph_external_reference", ["source_id", "destination_id", "kind",
                                        "position", "file_id", "offset", "external_symbol_id"])):
        actual = query(database, f"SELECT name FROM pragma_table_info('{table}')")
        require(actual == [(name,) for name in columns], str(actual))
        foreign = query(database, f"SELECT on_delete FROM pragma_foreign_key_list('{table}')")
        require(foreign and all(row == ("CASCADE",) for row in foreign), str(foreign))
    with sqlite3.connect(database) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        try:
            connection.execute("UPDATE callgraph_entry SET graph_node_ref=graph_node_ref+1")
        except sqlite3.IntegrityError:
            pass
        else:
            raise AssertionError("entry accepted an unrelated graph node")
        connection.execute("DELETE FROM relation_site WHERE destination_id IN "
                           "(SELECT external_symbol_id FROM callgraph_external_reference)")
        require(connection.execute("SELECT * FROM callgraph_external_reference").fetchall() == [],
                "external references did not cascade with their sites")
