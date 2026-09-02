from __future__ import annotations

import sqlite3
import subprocess

from pytest_bdd import given, parsers, then, when

from support.database import require
from support.scenario import FactsToolContext


KINDS = {"Calls": 1, "Inherits": 2, "Contains": 3, "Overrides": 6,
         "Uses": 7, "FieldOf": 8, "MethodOf": 9}


def run(arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(arguments, capture_output=True, text=True, check=False)


def rows(context: FactsToolContext, sql: str) -> list[tuple]:
    with sqlite3.connect(context.facts_database_path) as database:
        return database.execute(sql).fetchall()


def invoke(context: FactsToolContext, matcher: str,
           relation: str | None = None, sources: bool = True) -> None:
    arguments = [str(context.facts_tool), "match", "-v", "0", "-f",
                 str(context.facts_database_path), "--matcher", matcher]
    if relation:
        arguments.extend(("--relation-kind", relation))
    if sources:
        arguments.append(str(context.targeted_match_source))
    completed = run(arguments)
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr


def schema(context: FactsToolContext) -> list[tuple]:
    return rows(context, "SELECT 'version',user_version FROM pragma_user_version "
                        "UNION ALL SELECT type,name FROM sqlite_master "
                        "WHERE name NOT LIKE 'sqlite_%' ORDER BY 1,2")


@given("the targeted matcher corpus is imported")
def imported(context: FactsToolContext) -> None:
    context.prepare()
    context.targeted_match_source = (
        context.fixture_root / "targeted_match.cpp").resolve(strict=True)
    context.files_database = context.run_root_path / "targeted-match.sqlite"
    context.facts_database = context.files_database
    completed = run([str(context.facts_tool), "import", "-v", "0", "-c",
                     str(context.files_database_path), "--extra-arg=-std=c++23",
                     str(context.targeted_match_source)])
    require(completed.returncode == 0, completed.stdout + completed.stderr)


def import_sources(context: FactsToolContext, names: tuple[str, ...]) -> None:
    context.prepare()
    sources = tuple((context.fixture_root / name).resolve(strict=True) for name in names)
    context.targeted_match_source = sources[0]
    context.files_database = context.run_root_path / "targeted-match.sqlite"
    context.facts_database = context.files_database
    completed = run([str(context.facts_tool), "import", "-v", "0", "-c",
                     str(context.files_database_path), "--extra-arg=-std=c++23",
                     *(str(source) for source in sources)])
    require(completed.returncode == 0, completed.stdout + completed.stderr)


@given("two targeted matcher translation units are imported")
def two_sources(context: FactsToolContext) -> None:
    import_sources(context, ("targeted_match.cpp", "targeted_match_two.cpp"))


@given("a valid then invalid targeted translation unit are imported")
def broken_sources(context: FactsToolContext) -> None:
    import_sources(context, ("targeted_match.cpp", "targeted_match_broken.cpp"))


@given("the targeted matcher corpus is imported and its facts schema exists")
def imported_schema(context: FactsToolContext) -> None:
    imported(context)
    invoke(context, 'functionDecl(hasName("__no_match__")).bind("symbol")')
    require(context.last_returncode == 0, context.last_output)
    context.schema_before = schema(context)


@when(parsers.parse('match runs with matcher "{matcher}"'))
def match(context: FactsToolContext, matcher: str) -> None:
    invoke(context, matcher.replace(r'\"', '"'))


@when(parsers.parse('match runs with relation "{relation}" and matcher "{matcher}"'))
def relation_match(context: FactsToolContext, relation: str, matcher: str) -> None:
    invoke(context, matcher, relation)


@when("match runs without source arguments")
def match_all(context: FactsToolContext) -> None:
    invoke(context,
           'functionDecl(matchesName("targeted_match::(caller|second)")).bind("symbol")',
           sources=False)


@when("the Record symbol matcher runs twice")
def record_twice(context: FactsToolContext) -> None:
    matcher = ('cxxRecordDecl(hasName("targeted_match::Record"),'
               'isDefinition()).bind("symbol")')
    invoke(context, matcher)
    require(context.last_returncode == 0, context.last_output)
    invoke(context, matcher)


@then(parsers.parse('match succeeds and reports symbol kind "{kind}"'))
def symbol_success(context: FactsToolContext, kind: str) -> None:
    require(context.last_returncode == 0, context.last_output)
    require(f"symbol kind={kind}" in context.last_output, context.last_output)


@then(parsers.parse('the selected symbol "{name}" exists once'))
def symbol_once(context: FactsToolContext, name: str) -> None:
    found = rows(context, "SELECT COUNT(*) FROM symbol WHERE qualified_name=?"
                 .replace("?", "'" + name + "'"))
    require(found == [(1,)], str(found))


@then(parsers.parse('match succeeds and stores one "{relation}" relation'))
def relation_once(context: FactsToolContext, relation: str) -> None:
    require(context.last_returncode == 0, context.last_output)
    require(rows(context, f"SELECT COUNT(*) FROM relation WHERE kind={KINDS[relation]}")
            == [(1,)], context.last_output)


@then(parsers.parse('"{count:d}" relation sites are stored'))
def site_count(context: FactsToolContext, count: int) -> None:
    require(rows(context, "SELECT COUNT(*) FROM relation_site") == [(count,)],
            context.last_output)


@then("one Calls relation site is stored")
def call_site(context: FactsToolContext) -> None:
    require(rows(context, "SELECT COUNT(*) FROM relation_site WHERE kind=1") == [(1,)],
            context.last_output)


@then("the string lvalue argument is reported")
def argument(context: FactsToolContext) -> None:
    require("source='value'" in context.last_output and
            "type='std::string' category=lvalue value='unknown'" in context.last_output,
            context.last_output)


@then("function method constructor and lambda callers are stored")
def callable_owners(context: FactsToolContext) -> None:
    require(context.last_returncode == 0, context.last_output)
    found = [row[0] for row in rows(
        context, "SELECT source.qualified_name FROM relation edge "
                 "JOIN symbol source ON source.id=edge.source_id "
                 "JOIN symbol target ON target.id=edge.destination_id "
                 "WHERE edge.kind=1 AND target.qualified_name="
                 "'targeted_match::sink' ORDER BY source.qualified_name")]
    require("targeted_match::functionOwner" in found, str(found))
    require("targeted_match::OwnerCalls::method" in found, str(found))
    require("targeted_match::OwnerCalls::OwnerCalls" in found, str(found))
    require(len(found) == 4 and any("lambda" in name for name in found), str(found))
    require(rows(context, "SELECT COUNT(*) FROM relation_site WHERE kind=1") == [(4,)],
            str(found))


@then("the constant argument value is reported")
def constant_argument(context: FactsToolContext) -> None:
    require(context.last_returncode == 0 and "source='42'" in context.last_output and
            "type='int' category=prvalue value='42'" in context.last_output,
            context.last_output)


@then(parsers.parse('match fails with "{message}"'))
def failure(context: FactsToolContext, message: str) -> None:
    require(context.last_returncode == 1 and message in context.last_output,
            context.last_output)


@then("no targeted facts are stored")
def no_facts(context: FactsToolContext) -> None:
    with sqlite3.connect(context.facts_database_path) as database:
        tables = {row[0] for row in database.execute(
            "SELECT name FROM sqlite_master WHERE type='table'")}
    if "symbol" not in tables:
        return
    require(rows(context, "SELECT COUNT(*) FROM symbol") == [(0,)], "symbols remain")
    require(rows(context, "SELECT COUNT(*) FROM relation") == [(0,)], "relations remain")


@then("both translation units match in stored order")
def stored_order(context: FactsToolContext) -> None:
    require(context.last_returncode == 0, context.last_output)
    names = [line.split("name=", 1)[1] for line in context.last_output.splitlines()
             if line.startswith("symbol kind=")]
    require(names == ["targeted_match::caller", "targeted_match::second"], str(names))


@then("translation unit failure rolls back every fact")
def atomic_failure(context: FactsToolContext) -> None:
    require(context.last_returncode == 1 and "error:" in context.last_output,
            context.last_output)
    no_facts(context)


@then("the database schema is unchanged")
def unchanged_schema(context: FactsToolContext) -> None:
    require(schema(context) == context.schema_before, str(schema(context)))
