import json
import shutil

from pytest_bdd import given, when, then
from steps.external_target_steps import prepare_compile_database
from steps.implicit_allocation_target_steps import extract_implicit
from support.database import query, require


def calls(context):
    return query(context.facts_database_path,
                 "SELECT s.qualified_name,t.qualified_name FROM relation_site r "
                 "JOIN symbol s ON s.id=r.source_id JOIN symbol t ON t.id=r.destination_id "
                 "WHERE r.kind=1 AND s.qualified_name='traversal_runtime'")


@given("fresh and populated traversal fixtures", target_fixture="traversal")
def fixtures(context):
    source = prepare_compile_database(context, "implicit_traversal.cpp", "fresh-traversal")
    return source


@when("the same traversal source is extracted against both catalog states")
def compare(context, traversal):
    extract_implicit(context)
    require(context.last_returncode == 0, context.last_output)
    context.initial_symbols = calls(context)
    (context.run_root_path / "fresh.log").write_text(context.last_output)
    context.facts_database = context.run_root_path / "populated-traversal.sqlite"
    path = context.run_root_path / "compile_commands.json"
    original = json.loads(path.read_text())
    enroll = context.fixture_root / "implicit_enroll.cpp"
    commands = [{"file": str(enroll), "directory": str(enroll.parent),
                 "arguments": [str(context.compiler), "-std=c++23", "-c", str(enroll)]}]
    path.write_text(json.dumps(commands))
    extract_implicit(context)
    require(context.last_returncode == 0, context.last_output)
    require(query(context.facts_database_path,
                  "SELECT COUNT(*) FROM symbol WHERE qualified_name='traversal_runtime'")
            == [(1,)], "caller was not enrolled")
    path.write_text(json.dumps(original))
    # Consistent backup after the writer exits; preserve the pre-run snapshot.
    shutil.copy2(context.facts_database_path, context.run_root_path / "populated-before.sqlite")
    extract_implicit(context)
    (context.run_root_path / "populated.log").write_text(context.last_output)


@then("excluded header sites neither resolve targets nor contribute runtime calls")
def expected(context):
    require(context.last_returncode == 0, context.last_output)
    require(context.initial_symbols == [], str(context.initial_symbols))
    require(calls(context) == [], str(calls(context)))
    require("c:@F@operator new#" not in context.last_output, context.last_output)
    require(not query(context.facts_database_path,
                      "SELECT id FROM symbol WHERE qualified_name='operator new'"),
            "excluded site materialized a target")
    require("indexing incomplete" not in context.last_output, context.last_output)
