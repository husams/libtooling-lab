from pytest_bdd import given, then, when

from steps.external_target_steps import prepare_compile_database, run
from support.database import file_snapshot, query, require


@given("an implicit allocation fixture", target_fixture="implicit_source")
def implicit_fixture(context):
    return prepare_compile_database(
        context, "implicit_allocation_target.cpp", "implicit-allocation"
    )


def extract_implicit(context):
    imported = run([str(context.facts_tool), "import", "--conf",
                    str(context.files_database_path), "-p", str(context.run_root_path)])
    require(imported.returncode == 0, imported.stdout + imported.stderr)
    result = run([str(context.facts_tool), "extract", "--conf",
                  str(context.files_database_path), "--output",
                  str(context.facts_database_path), "--verbose", "3"])
    context.last_returncode = result.returncode
    context.last_output = result.stdout + result.stderr
    (context.run_root_path / "implicit-extract.log").write_text(context.last_output)


@when("implicit allocation extraction runs")
def extract(context, implicit_source):
    extract_implicit(context)


@then("implicit allocation commits with a real canonical call site")
def committed(context, implicit_source):
    require(context.last_returncode == 0, context.last_output)
    require("indexing incomplete" not in context.last_output, context.last_output)
    require("rollback output transaction" not in context.last_output, context.last_output)
    rows = query(context.facts_database_path,
                 "SELECT id,usr,kind,is_external,is_implicit,line,col,offset "
                 "FROM symbol WHERE qualified_name='operator new'")
    require(len(rows) == 1, str(rows))
    target, usr, kind, external, implicit, line, col, offset = rows[0]
    require(usr.startswith("c:@F@operator new#") and kind == 13, str(rows))
    require(target >> 32 == 0 and (external, implicit, line, col, offset) ==
            (1, 1, 0, 0, 0), str(rows))
    sites = query(context.facts_database_path,
                  "SELECT s.qualified_name,r.file_id,r.line,r.col FROM relation_site r "
                  "JOIN symbol s ON s.id=r.source_id WHERE r.destination_id=? AND r.kind=1",
                  (target,))
    expected_site = next((number, line.index("::operator new") + 1)
                         for number, line in enumerate(implicit_source.read_text().splitlines(), 1)
                         if "::operator new" in line)
    require(len(sites) == 1 and sites[0][0] == "explicit_new" and
            sites[0][2:] == expected_site, str(sites))
    files = dict(file_snapshot(context.files_database_path))
    require(files[sites[0][1]] == str(implicit_source), str(files))
    siblings = query(context.facts_database_path,
                     "SELECT qualified_name FROM symbol WHERE is_definition=1 "
                     "AND qualified_name IN ('seed_new','explicit_new') ORDER BY qualified_name")
    require(siblings == [("explicit_new",), ("seed_new",)], str(siblings))
    graph = run([str(context.facts_tool), "analyse", "call-graph", "--facts",
                 str(context.facts_database_path), "--function", "explicit_new"])
    require(graph.returncode == 0 and "operator new" in graph.stdout,
            graph.stdout + graph.stderr)
