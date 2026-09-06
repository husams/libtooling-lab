from __future__ import annotations

import json

from pytest_bdd import given, then, when

from steps.native_matcher_workflow_steps import prepare_pair, run
from support.catalog import Catalog
from support.database import query, require
from support.scenario import FactsToolContext


def match_caller(context: FactsToolContext):
    return run([str(context.facts_tool), "match", "-v", "0", "--conf",
                str(context.files_database), "--facts", str(context.facts_database),
                "--matcher", 'functionDecl(hasName("targeted_match::caller")).bind("symbol")',
                str(context.targeted_match_source)])


def find(context: FactsToolContext, *selector: str) -> dict:
    result = run([str(context.facts_tool), "symbol", "find", "-v", "0", "--conf",
                  str(context.files_database), *selector, "--format", "json"])
    require(result.returncode == 0, result.stdout + result.stderr)
    return json.loads(result.stdout)


@given("an S-026 matcher project pair")
def s026_pair(context: FactsToolContext) -> None:
    prepare_pair(context)


@then("the project has version one and the exact four-field matched index")
def exact_schema(context: FactsToolContext) -> None:
    columns = query(context.files_database, "SELECT name FROM pragma_table_info('matched_symbol_index')")
    require(columns == [("usr",), ("qualified_name",), ("file_id",), ("kind",)], str(columns))
    require(query(context.files_database, "SELECT schema_version FROM project_registry") == [(1,)], "wrong version")
    require(query(context.files_database, "SELECT * FROM matched_symbol_index") == [], "migration backfilled index")


@when("the S-026 caller symbol is matched twice")
def caller_twice(context: FactsToolContext) -> None:
    for _ in range(2):
        result = match_caller(context)
        require(result.returncode == 0, result.stdout + result.stderr)


@given("the S-026 caller symbol is already indexed")
def caller_indexed(context: FactsToolContext) -> None:
    result = match_caller(context)
    require(result.returncode == 0, result.stdout + result.stderr)


@then("matched-symbol lookup returns one matched-only candidate")
def one_candidate(context: FactsToolContext) -> None:
    output = find(context, "--name", "targeted_match::caller")
    require(len(output["matches"]) == 1, str(output))
    require(output["coverage"] == {"index_scope": "matched-only", "source_complete": None}, str(output))
    require(find(context, "--name", "%")["matches"] == [], "wildcard was not literal")
    require(find(context, "--name", "TARGETED_MATCH")["matches"] == [], "name lookup ignored case")


@when("symbols are matched in two independent catalog components")
def match_catalog_components(catalog: Catalog) -> None:
    catalog.context.files_database = catalog.context.files_database_path
    catalog.context.facts_database = catalog.context.run_root_path / "s026-catalog-facts.sqlite"
    for source in (catalog.checkout / "core/src/one.cpp", catalog.checkout / "neighbor/other-only/four.cpp"):
        result = run([str(catalog.context.facts_tool), "match", "-v", "0", "--conf",
                      str(catalog.context.files_database), "--facts", str(catalog.context.facts_database),
                      "--matcher", 'functionDecl(matchesName("catalog_value_")).bind("symbol")', str(source)])
        require(result.returncode == 0, result.stdout + result.stderr)


@then("matched-symbol lookup returns both repositories deterministically")
def cross_repository(catalog: Catalog) -> None:
    output = find(catalog.context, "--name", "catalog_value_")
    require([row["repository"] for row in output["matches"]] == ["demo", "independent"], str(output))


@when("relation and direct-call symbol bindings are matched")
def match_explicit_bindings(context: FactsToolContext) -> None:
    base = [str(context.facts_tool), "match", "-v", "0", "--conf", str(context.files_database),
            "--facts", str(context.facts_database)]
    commands = [
        [*base, "--relation-kind", "FieldOf", "--matcher",
         'fieldDecl(hasName("targeted_match::Record::field"),hasParent(cxxRecordDecl().bind("target"))).bind("source")'],
        [*base, "--matcher",
         'callExpr(callee(functionDecl(hasName("targeted_match::print")).bind("callee"))).bind("call")'],
    ]
    for command in commands:
        result = run([*command, str(context.targeted_match_source)])
        require(result.returncode == 0, result.stdout + result.stderr)


@then("explicit relation and callee bindings are indexed without the derived caller")
def explicit_bindings_only(context: FactsToolContext) -> None:
    names = [row["qualified_name"] for row in find(context, "--name", "targeted_match::")["matches"]]
    require(names == ["targeted_match::print", "targeted_match::Record", "targeted_match::Record::field"], str(names))
    require("targeted_match::caller" not in names, str(names))
