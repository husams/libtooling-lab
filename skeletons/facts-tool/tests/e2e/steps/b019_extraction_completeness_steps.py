from __future__ import annotations

import json
import sqlite3
import subprocess
from pathlib import Path

from pytest_bdd import given, parsers, then, when
from support.database import query, require
from support.scenario import FactsToolContext

SPECIALIZES = 4
INSTANTIATES = 5
FIELD_OF = 8
METHOD_OF = 9
TEMPLATE_ARGUMENT_TYPE = 23

PRIMARY_USR = "c:@N@b0xx@ST>1#T@Holder"
UNDECLARED_USRS = {
    "Holder<Widget>": "c:@N@b0xx@S@Holder>#$@N@b0xx@S@Widget",
    "Holder<int>": "c:@N@b0xx@S@Holder>#I",
    "Holder<double>": "c:@N@b0xx@S@Holder>#d",
    "Holder<char>": "c:@N@b0xx@S@Holder>#C",
}
INSTANTIATED_USR = "c:@N@b0xx@S@Holder>#$@N@b0xx@S@Policy"

# The owners relation_resolution.hpp declares are filtered out of the store, so
# each of these relations has to persist its destination on demand. Exact USR
# pairs, because a LIKE match cannot tell a correctly wired owner from a swapped
# one.
RESOLVED_OWNER_RELATIONS = [
    (
        FIELD_OF,
        "c:@N@regression@S@Box@value",
        "c:@N@regression@S@Box",
        1,
    ),
    (
        FIELD_OF,
        "c:@N@regression@S@BoxedPair@value",
        "c:@N@regression@S@BoxedPair",
        1,
    ),
    (
        METHOD_OF,
        "c:@N@std@S@hash>#$@N@regression@S@Hashable@F@operator()#&1S0_#1",
        "c:@N@std@S@hash>#$@N@regression@S@Hashable",
        1,
    ),
]

FIXTURES = {
    "undeclared-template": ("undeclared_template_instances.cpp", "b019a"),
    "invalid-usr": ("invalid_usr_declarations.cpp", "b019b"),
    "relation-resolution": ("relation_resolution.cpp", "b019c"),
}


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True, check=False)


def symbol_id(context: FactsToolContext, usr: str) -> int:
    rows = query(
        context.facts_database_path,
        "SELECT id FROM symbol WHERE usr=?",
        (usr,),
    )
    require(len(rows) == 1, f"expected one symbol for usr {usr!r}, got {rows}")
    return rows[0][0]


@given(
    parsers.parse("a compile database for the {fixture} fixture"),
    target_fixture="b019_fixture",
)
def given_b019_compile_database(context: FactsToolContext, fixture: str) -> Path:
    context.prepare()
    name, slug = FIXTURES[fixture]
    source = (context.fixture_root / name).resolve(strict=True)
    context.facts_database = context.run_root_path / f"{slug}-facts.sqlite"
    context.files_database = context.run_root_path / f"{slug}-project.sqlite"
    (context.run_root_path / "compile_commands.json").write_text(
        json.dumps(
            [
                {
                    "directory": str(context.fixture_root),
                    "file": str(source),
                    "arguments": [
                        str(context.compiler),
                        "-std=c++17",
                        "-c",
                        str(source),
                    ],
                }
            ],
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return source


def extract_once(context: FactsToolContext, source: Path, verbosity: int = 1) -> None:
    imported = run(
        [
            str(context.facts_tool),
            "import",
            "--conf",
            str(context.files_database_path),
            "--compilation-database",
            str(context.run_root_path),
            str(source),
        ]
    )
    require(
        imported.returncode == 0,
        f"expected import exit code 0, got {imported.returncode}:\n"
        + imported.stdout
        + imported.stderr,
    )
    completed = run(
        [
            str(context.facts_tool),
            "extract",
            "--output",
            str(context.facts_database_path),
            "--conf",
            str(context.files_database_path),
            "--verbose",
            str(verbosity),
            str(source),
        ]
    )
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr


@when(parsers.parse("the real extraction command indexes the {fixture} fixture"))
def when_extract_indexes(
    context: FactsToolContext, b019_fixture: Path, fixture: str
) -> None:
    extract_once(context, b019_fixture)


@when(
    parsers.parse(
        "the real extraction command indexes the {fixture} fixture at verbosity 3"
    )
)
def when_extract_indexes_verbose(
    context: FactsToolContext, b019_fixture: Path, fixture: str
) -> None:
    extract_once(context, b019_fixture, verbosity=3)


@when(parsers.parse("the real extraction command indexes the {fixture} fixture twice"))
def when_extract_indexes_twice(
    context: FactsToolContext, b019_fixture: Path, fixture: str
) -> None:
    extract_once(context, b019_fixture)
    context.first_identities = query(
        context.facts_database_path,
        "SELECT usr,qualified_name FROM symbol ORDER BY usr,qualified_name",
    )
    context.facts_database_path.unlink()
    extract_once(context, b019_fixture)


@when("template argument relation persistence is forced to fail on a rerun")
def when_template_relation_persistence_fails(
    context: FactsToolContext, b019_fixture: Path
) -> None:
    extract_once(context, b019_fixture)
    with sqlite3.connect(context.facts_database_path) as connection:
        connection.execute(
            "CREATE TRIGGER force_template_argument_relation_failure "
            "BEFORE INSERT ON relation WHEN NEW.kind=23 AND NEW.position=0 BEGIN "
            "SELECT RAISE(ABORT, 'forced template argument relation failure'); "
            "END"
        )
    extract_once(context, b019_fixture)


@then(
    parsers.parse(
        "the {fixture} extraction exits successfully without incomplete diagnostics"
    )
)
def then_extraction_succeeds(context: FactsToolContext, fixture: str) -> None:
    require(
        context.last_returncode == 0,
        f"expected extract exit code 0, got {context.last_returncode}:\n"
        + context.last_output,
    )
    require(
        "indexing incomplete" not in context.last_output,
        f"unexpected incomplete diagnostic:\n{context.last_output}",
    )


@then("no template_instance relation failure is reported")
def then_no_template_instance_failure(context: FactsToolContext) -> None:
    require(
        "relation=template_instance" not in context.last_output,
        f"template_instance relation failure reported:\n{context.last_output}",
    )
    require(
        "rollback output transaction" not in context.last_output,
        f"extraction rolled back the translation unit:\n{context.last_output}",
    )


@then("the undeclared-template canary and owners are committed")
def then_unit_is_committed(context: FactsToolContext) -> None:
    symbols = {
        row[0]
        for row in query(
            context.facts_database_path,
            "SELECT qualified_name FROM symbol WHERE qualified_name LIKE 'b0xx::%'",
        )
    }
    expected = {
        "b0xx::Canary",
        "b0xx::canary",
        "b0xx::Widget",
        "b0xx::Policy",
        "b0xx::Holder",
        "b0xx::Owner",
        "b0xx::Owner::field",
        "b0xx::pointerOnly",
        "b0xx::referenceOnly",
        "b0xx::AliasOnly",
        "b0xx::instantiated",
    }
    require(expected <= symbols, f"missing committed facts: {expected - symbols}")
    holders = query(
        context.facts_database_path,
        "SELECT usr FROM symbol WHERE qualified_name='b0xx::Holder' ORDER BY usr",
    )
    expected_usrs = sorted([PRIMARY_USR, INSTANTIATED_USR, *UNDECLARED_USRS.values()])
    require(
        [usr for (usr,) in holders] == expected_usrs,
        f"unexpected Holder specialization set: {holders}",
    )


@then("each undeclared specialization carries no specializes and no instantiates relation")
def then_undeclared_have_no_provenance(context: FactsToolContext) -> None:
    offenders = {}
    for label, usr in UNDECLARED_USRS.items():
        sid = symbol_id(context, usr)
        rows = query(
            context.facts_database_path,
            "SELECT kind,destination_id FROM relation "
            "WHERE source_id=? AND kind IN (?,?)",
            (sid, SPECIALIZES, INSTANTIATES),
        )
        if rows:
            offenders[label] = rows
    require(offenders == {}, f"undeclared provenance relations: {offenders}")


@then(
    "the undeclared record-argument specialization still carries its template argument type edge"
)
def then_undeclared_keeps_argument_edge(context: FactsToolContext) -> None:
    sid = symbol_id(context, UNDECLARED_USRS["Holder<Widget>"])
    widget = symbol_id(context, "c:@N@b0xx@S@Widget")
    rows = query(
        context.facts_database_path,
        "SELECT destination_id,position FROM relation WHERE source_id=? AND kind=?",
        (sid, TEMPLATE_ARGUMENT_TYPE),
    )
    require(rows == [(widget, 0)], f"Holder<Widget> argument edge: {rows}")


@then("the implicitly instantiated specialization points to the primary template")
def then_instantiation_keeps_provenance(context: FactsToolContext) -> None:
    sid = symbol_id(context, INSTANTIATED_USR)
    primary = symbol_id(context, PRIMARY_USR)
    rows = query(
        context.facts_database_path,
        "SELECT kind,destination_id FROM relation "
        "WHERE source_id=? AND kind IN (?,?)",
        (sid, SPECIALIZES, INSTANTIATES),
    )
    require(rows == [(INSTANTIATES, primary)], f"instantiation provenance: {rows}")


@then("the implicitly instantiated specialization carries its template argument type edge")
def then_instantiation_keeps_argument_edge(context: FactsToolContext) -> None:
    sid = symbol_id(context, INSTANTIATED_USR)
    policy = symbol_id(context, "c:@N@b0xx@S@Policy")
    rows = query(
        context.facts_database_path,
        "SELECT destination_id,position FROM relation WHERE source_id=? AND kind=?",
        (sid, TEMPLATE_ARGUMENT_TYPE),
    )
    require(rows == [(policy, 0)], f"Holder<Policy> argument edge: {rows}")


@then("no symbol row exists for the un-USR-able declaration")
def then_no_symbol_row_for_skipped(context: FactsToolContext) -> None:
    rows = query(
        context.facts_database_path,
        "SELECT id,usr,qualified_name FROM symbol "
        "WHERE qualified_name LIKE 'probe::Bits::%'",
    )
    require(
        [(usr, name) for _, usr, name in rows]
        == [("c:@N@probe@S@Bits@FI@named", "probe::Bits::named")],
        f"un-USR-able declaration persisted or sibling lost: {rows}",
    )


@then("no relation references the un-USR-able declaration")
def then_no_relation_references_skipped(context: FactsToolContext) -> None:
    record = symbol_id(context, "c:@N@probe@S@Bits")
    named = symbol_id(context, "c:@N@probe@S@Bits@FI@named")
    rows = query(
        context.facts_database_path,
        "SELECT source_id,destination_id,kind FROM relation "
        "WHERE destination_id=? AND kind=?",
        (record, FIELD_OF),
    )
    require(rows == [(named, record, FIELD_OF)], f"unexpected field_of edges: {rows}")


@then("no identity was synthesized for the un-USR-able declaration")
def then_no_synthesized_identity(context: FactsToolContext) -> None:
    unnamed = query(
        context.facts_database_path,
        "SELECT usr,qualified_name FROM symbol "
        "WHERE qualified_name LIKE '%(anonymous)%' "
        "OR qualified_name LIKE '%(unnamed)%'",
    )
    require(unnamed == [], f"an unnamed entity was persisted: {unnamed}")
    fabricated = query(
        context.facts_database_path,
        "SELECT usr,qualified_name FROM symbol WHERE usr NOT LIKE 'c:%'",
    )
    require(fabricated == [], f"a non-Clang identity was synthesized: {fabricated}")
    members = query(
        context.facts_database_path,
        "SELECT usr FROM symbol WHERE usr LIKE 'c:@N@probe@S@Bits@%' ORDER BY usr",
    )
    require(
        members == [("c:@N@probe@S@Bits@FI@named",)],
        f"unexpected probe::Bits members: {members}",
    )


@then("the trace records the skipped declaration with reason invalid USR")
def then_trace_records_skip(context: FactsToolContext) -> None:
    expected = (
        "facts-tool: trace: node extraction kind='Field' "
        "name='probe::Bits::(anonymous)' result=filtered reason='invalid USR'"
    )
    require(expected in context.last_output, f"missing filtered trace:\n{context.last_output}")


@then("every named sibling declaration in the same record is committed")
def then_named_siblings_survive(context: FactsToolContext) -> None:
    symbols = {
        row[0]
        for row in query(
            context.facts_database_path,
            "SELECT qualified_name FROM symbol WHERE qualified_name LIKE 'probe::%'",
        )
    }
    expected = {
        "probe::Bits",
        "probe::Bits::named",
        "probe::Canary",
        "probe::Canary::seen",
        "probe::bits",
        "probe::canary",
    }
    require(expected <= symbols, f"named siblings were lost: {expected - symbols}")


@then(
    "the relation-resolution extraction exits successfully without incomplete diagnostics"
)
def then_relation_resolution_succeeds(context: FactsToolContext) -> None:
    require(
        context.last_returncode == 0,
        f"expected extract exit code 0, got {context.last_returncode}:\n"
        + context.last_output,
    )
    require(
        "indexing incomplete" not in context.last_output,
        f"unexpected incomplete diagnostic:\n{context.last_output}",
    )
    require(
        "target symbol is not persisted" not in context.last_output,
        f"owner relation still failed to resolve its target:\n{context.last_output}",
    )
    require(
        "invalid USR" not in context.last_output,
        f"unexpected invalid-USR diagnostic:\n{context.last_output}",
    )


@then("the specialization owner relation is committed")
def then_specialization_owner_relation_is_committed(
    context: FactsToolContext,
) -> None:
    rows = query(
        context.facts_database_path,
        "SELECT relation.kind,source.usr,destination.usr,destination.is_external "
        "FROM relation "
        "JOIN symbol source ON source.id=relation.source_id "
        "JOIN symbol destination ON destination.id=relation.destination_id "
        "WHERE relation.kind IN (?,?) AND destination.is_external=1 "
        "ORDER BY relation.kind,source.usr",
        (FIELD_OF, METHOD_OF),
    )
    require(
        rows == RESOLVED_OWNER_RELATIONS,
        f"unexpected on-demand owner relations: {rows}",
    )


@then("the template relation insertion exits unsuccessfully")
def then_template_relation_insertion_fails(context: FactsToolContext) -> None:
    require(
        context.last_returncode is not None and context.last_returncode != 0,
        f"expected a nonzero exit code: {context.last_returncode}",
    )


@then("the template relation diagnostic names its kind, SymbolIds, and position")
def then_template_relation_diagnostic_is_specific(
    context: FactsToolContext,
) -> None:
    required = (
        "indexing incomplete",
        "relation=template_argument_type",
        "source_symbol_id='",
        "destination_symbol_id='",
        "position=0",
    )
    require(
        all(fragment in context.last_output for fragment in required),
        f"incomplete template relation diagnostic:\n{context.last_output}",
    )


@then("both runs produce identical symbol identities")
def then_identities_are_stable(context: FactsToolContext) -> None:
    second = query(
        context.facts_database_path,
        "SELECT usr,qualified_name FROM symbol ORDER BY usr,qualified_name",
    )
    require(context.first_identities == second, "symbol identities changed between runs")


@then("the output database passes PRAGMA foreign_key_check")
def then_foreign_keys_are_intact(context: FactsToolContext) -> None:
    violations = query(context.facts_database_path, "PRAGMA foreign_key_check")
    require(violations == [], f"foreign-key violations: {violations}")
