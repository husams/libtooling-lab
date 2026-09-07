import json

from pytest_bdd import given, parsers, then, when
from steps.external_target_steps import prepare_compile_database, run
from steps.implicit_allocation_target_steps import extract_implicit
from support import callgraph_run as cg
from support.database import file_snapshot, query, require


@given(parsers.parse('a compiler callable matrix "{fixture}" with {count:d} explicit calls'),
       target_fixture="matrix")
def matrix_fixture(context, fixture, count):
    source = prepare_compile_database(context, fixture, "matrix")
    path = context.run_root_path / "compile_commands.json"
    commands = json.loads(path.read_text())
    commands[0]["arguments"].insert(1, "-fsized-deallocation")
    path.write_text(json.dumps(commands))
    probe = run([str(context.facts_tool.with_name("implicit-target-probe")), str(source),
                 str(context.clang_driver or context.compiler)])
    (context.run_root_path / "matrix-probe.json").write_text(probe.stdout)
    (context.run_root_path / "matrix-probe.stderr").write_text(probe.stderr)
    require(probe.returncode == 0, probe.stderr)
    rows = json.loads(probe.stdout)
    require(len(rows) == count, str(rows))
    if fixture == "implicit_allocation_matrix.cpp":
        require(all(r["compiler_provided"] and r["usr"] for r in rows), str(rows))
    else:
        require(all(not r["compiler_provided"] for r in rows), str(rows))
    return source, rows


def identities(context):
    return query(context.facts_database_path, "SELECT id,usr FROM symbol ORDER BY id")


@when("the callable matrix is extracted and reopened")
def extract_matrix(context, matrix):
    extract_implicit(context)
    require(context.last_returncode == 0, context.last_output)
    context.first_identities = identities(context)
    extract_implicit(context)
    require(context.last_returncode == 0, context.last_output)
    require(identities(context) == context.first_identities, "matrix IDs changed on reopen")


@then("every matrix call follows its observed USR and declaration provenance")
def matrix_calls(context, matrix):
    source, rows = matrix
    require("indexing incomplete" not in context.last_output, context.last_output)
    files = dict(file_snapshot(context.files_database_path))
    ids = []
    for cell in rows:
        calls = query(context.facts_database_path,
                      "SELECT t.id,t.usr,t.kind,t.is_external,t.is_implicit,t.line,t.col,"
                      "r.file_id,r.line,r.col FROM relation_site r "
                      "JOIN symbol s ON s.id=r.source_id JOIN symbol t ON t.id=r.destination_id "
                      "WHERE s.qualified_name=? AND r.kind=1", (cell["caller"],))
        if not cell["usr"]:
            require(not calls and "invalid USR" in context.last_output, str(cell))
            require(not query(context.facts_database_path,
                              "SELECT id FROM symbol WHERE qualified_name=?",
                              (cell["target"],)), str(cell))
            continue
        require(len(calls) == 1, str((cell, calls)))
        row = calls[0]
        require(row[1:3] == (cell["usr"], cell["kind"]), str((cell, row)))
        require(files[row[7]] == str(source) and row[8:] ==
                (cell["line"], cell["column"]), str((cell, row)))
        if cell["compiler_provided"]:
            require(row[0] >> 32 == 0 and row[3:7] == (1, 1, 0, 0), str(row))
        else:
            require(row[0] >> 32 != 0, str(row))
        if cell["target"] == "__builtin_memcpy":
            properties = query(
                context.facts_database_path,
                "SELECT is_external,is_implicit,is_noexcept,is_variadic "
                "FROM symbol WHERE id=?", (row[0],))
            require(properties == [(1, 1, 1, 0)], str(properties))
        require(query(context.facts_database_path,
                      "SELECT COUNT(*) FROM symbol WHERE usr=?", (cell["usr"],)) == [(1,)], str(row))
        graph = cg.run_graph(context, "--function", cell["caller"], conf=False)
        graph_id, _ = cg.completion(graph)
        require(graph.returncode == 0, graph.stderr)
        edges = cg.edge_names(context.facts_database_path, graph_id)
        require((cell["caller"], cell["target"]) in edges, str(edges))
        ids.append(row[0])
    require(len(ids) == len(set(ids)), "overloads collapsed")
