from __future__ import annotations

import sqlite3
import subprocess
import shutil
from pathlib import Path

import pytest
from pytest_bdd import given, then
from support.database import require
from support.scenario import FactsToolContext


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True, check=False)


def extract(context: FactsToolContext, names: tuple[str, ...]) -> None:
    context.prepare()
    sources = [str((context.fixture_root / name).resolve(strict=True)) for name in names]
    imported = run([str(context.facts_tool), "import", "-v", "0", "-c",
                    str(context.files_database_path), "--extra-arg=-std=c++23",
                    *sources])
    require(imported.returncode == 0, imported.stdout + imported.stderr)
    completed = run([str(context.facts_tool), "extract", "-v", "0", "-o",
                     str(context.facts_database_path), "-c",
                     str(context.files_database_path), *sources])
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr
    require(completed.returncode == 0, context.last_output)


def rows(context: FactsToolContext, sql: str) -> list[tuple]:
    with sqlite3.connect(context.facts_database_path) as database:
        return database.execute(sql).fetchall()


def cli(context: FactsToolContext, *arguments: str) -> subprocess.CompletedProcess[str]:
    return run([str(context.facts_tool), "analyse", "call-graph", "-v", "0",
                "-f", str(context.facts_database_path), *arguments])


@given("the exact contextual call graph corpus is extracted")
def exact_corpus(context: FactsToolContext) -> None:
    extract(context, ("call_graph_one.cpp", "call_graph_two.cpp"))


@given("the possible-receiver contextual call graph corpus is extracted")
def possible_corpus(context: FactsToolContext) -> None:
    extract(context, ("call_graph_possible.cpp",))


@given("the template contextual call graph corpus is extracted")
def template_corpus(context: FactsToolContext) -> None:
    extract(context, ("call_graph_template.cpp",))


@then("direct method lambda and constructor-body Calls are recorded once")
def direct_calls(context: FactsToolContext) -> None:
    found = rows(context, "SELECT s.qualified_name,d.qualified_name FROM relation r "
        "JOIN symbol s ON s.id=r.source_id JOIN symbol d ON d.id=r.destination_id "
        "WHERE r.kind=1 AND d.qualified_name='call_graph_fixture::helper'")
    names = [source for source, _ in found]
    require("call_graph_fixture::directMethodLambdaAndConstructor" in names,
            f"missing direct call: {found}")
    require("call_graph_fixture::Owner::method" in names and
            "call_graph_fixture::Owner::Owner" in names,
            f"missing method or constructor-body call: {found}")
    require(any("lambda" in name for name in names), f"missing lambda call: {found}")
    constructor_edges = rows(context, "SELECT COUNT(*) FROM relation r JOIN symbol d "
        "ON d.id=r.destination_id WHERE r.kind=1 AND d.qualified_name="
        "'call_graph_fixture::Owner::Owner'")
    require(constructor_edges == [(0,)], f"constructor invocation duplicated: {constructor_edges}")


@then("the cross-TU declaration-only callee resolves to its definition")
def cross_tu(context: FactsToolContext) -> None:
    found = rows(context, "SELECT d.is_definition,d.is_external FROM relation r "
        "JOIN symbol s ON s.id=r.source_id JOIN symbol d ON d.id=r.destination_id "
        "WHERE r.kind=1 AND s.qualified_name='call_graph_fixture::exactCalls' "
        "AND d.qualified_name='call_graph_fixture::declarationOnly'")
    require(found == [(1, 0)], f"callee did not resolve globally: {found}")


@then("inherited calls and overrides retain canonical owners")
def ownership(context: FactsToolContext) -> None:
    found = rows(context, "SELECT s.qualified_name,d.qualified_name,r.kind FROM relation r "
        "JOIN symbol s ON s.id=r.source_id JOIN symbol d ON d.id=r.destination_id "
        "WHERE r.kind IN (1,6)")
    require(("call_graph_fixture::exactCalls", "call_graph_fixture::Base::log", 1) in found,
            f"missing inherited call owner: {found}")
    require(("call_graph_fixture::X::toString", "call_graph_fixture::Base::toString", 6) in found,
            f"missing override: {found}")
    counts = rows(context, "SELECT r.count,COUNT(site.offset) FROM relation r "
        "JOIN symbol s ON s.id=r.source_id JOIN symbol d ON d.id=r.destination_id "
        "LEFT JOIN relation_site site ON site.source_id=r.source_id AND "
        "site.destination_id=r.destination_id AND site.kind=r.kind AND "
        "site.position=r.position WHERE r.kind=1 AND ((s.qualified_name="
        "'call_graph_fixture::exactCalls' AND d.qualified_name="
        "'call_graph_fixture::Base::log') OR (s.qualified_name="
        "'call_graph_fixture::Base::log' AND d.qualified_name="
        "'call_graph_fixture::Base::toString')) GROUP BY r.source_id,r.destination_id "
        "ORDER BY s.qualified_name")
    require(counts == [(1, 1), (2, 2)], f"unexpected Calls site counts: {counts}")
    overrides = rows(context, "SELECT s.qualified_name FROM relation r JOIN symbol s "
        "ON s.id=r.source_id WHERE r.kind=6 AND s.qualified_name LIKE "
        "'%::toString' ORDER BY s.qualified_name")
    require(overrides == [("call_graph_fixture::X::toString",),
                          ("call_graph_fixture::Y::toString",)], str(overrides))
    synthetic = rows(context, "SELECT qualified_name FROM symbol WHERE qualified_name IN "
        "('call_graph_fixture::X::log','call_graph_fixture::Y::log')")
    require(not synthetic, f"synthetic methods were created: {synthetic}")


def assert_exact_dispatch(context: FactsToolContext, receiver: str, target: str) -> None:
    stored = rows(context, "SELECT receiver.qualified_name,site.certainty FROM relation_site site "
        "JOIN symbol d ON d.id=site.destination_id LEFT JOIN symbol receiver ON "
        "receiver.id=site.receiver_type_id WHERE site.kind=18 AND d.qualified_name="
        f"'call_graph_fixture::{target}::toString'")
    require(stored == [(f"call_graph_fixture::{receiver}", 1)], f"bad exact dispatch: {stored}")
    output = cli(context, "--function", "call_graph_fixture::exactCalls")
    require(output.returncode == 0 and f"target=call_graph_fixture::{target}::toString" in output.stdout,
            output.stdout + output.stderr)
    other = "Y" if target == "X" else "X"
    section = output.stdout.split(f"receiver=call_graph_fixture::{receiver} certainty=exact", 1)[1]
    section = section.split("depth=1 relation=Calls", 1)[0]
    require(f"target=call_graph_fixture::{other}::toString" not in section, output.stdout)


@then("MessageX dispatch is exact in storage and text")
def exact_x(context: FactsToolContext) -> None:
    assert_exact_dispatch(context, "X", "X")


@then("MessageY dispatch is exact in storage and text")
def exact_y(context: FactsToolContext) -> None:
    assert_exact_dispatch(context, "Y", "Y")


@then("unproven receiver dispatch is possible and conservative")
def possible_dispatch(context: FactsToolContext) -> None:
    found = rows(context, "SELECT d.qualified_name,site.receiver_type_id,site.certainty "
        "FROM relation_site site JOIN symbol s ON s.id=site.source_id JOIN symbol d "
        "ON d.id=site.destination_id WHERE site.kind=18 AND s.qualified_name="
        "'call_graph_fixture::Base::log' ORDER BY d.qualified_name")
    require(found == [("call_graph_fixture::X::toString", None, 2),
                      ("call_graph_fixture::Y::toString", None, 2)], str(found))
    transitive = rows(context, "SELECT s.qualified_name,d.qualified_name,site.certainty "
        "FROM relation_site site JOIN symbol s ON s.id=site.source_id JOIN symbol d "
        "ON d.id=site.destination_id WHERE site.kind=18 AND s.qualified_name IN "
        "('call_graph_fixture::PossibleRoot::call','call_graph_fixture::ExactRoot::call',"
        "'call_graph_fixture::FallbackRoot::call') ORDER BY s.qualified_name")
    require(transitive == [
        ("call_graph_fixture::ExactRoot::call", "call_graph_fixture::ExactLeaf::value", 1),
        ("call_graph_fixture::FallbackRoot::call", "call_graph_fixture::FallbackRoot::value", 1),
        ("call_graph_fixture::PossibleRoot::call", "call_graph_fixture::PossibleLeaf::value", 2),
    ], str(transitive))
    output = cli(context, "--function", "call_graph_fixture::possibleCall")
    require("receiver=* certainty=possible" in output.stdout, output.stdout)


@then("instantiated callers normalize to the written pattern")
def normalized_template(context: FactsToolContext) -> None:
    found = rows(context, "SELECT s.qualified_name,d.qualified_name,r.count,COUNT(site.offset) "
        "FROM relation r JOIN symbol s ON s.id=r.source_id JOIN symbol d ON "
        "d.id=r.destination_id LEFT JOIN relation_site site ON site.source_id=r.source_id "
        "AND site.destination_id=r.destination_id AND site.kind=r.kind AND "
        "site.position=r.position WHERE r.kind=1 AND s.qualified_name LIKE "
        "'call_graph_fixture::invoke%' GROUP BY s.id,d.id")
    require(found == [("call_graph_fixture::invoke", "call_graph_fixture::Base::log", 1, 1)], str(found))


@given("the call graph migration regression is run", target_fixture="migration_result")
def migration_regression(context: FactsToolContext) -> subprocess.CompletedProcess[str]:
    executable = context.facts_tool.parent / "storage-schema-test"
    return run([str(executable), str(context.output_root / "cg-fresh.sqlite"),
                str(context.output_root / "cg-legacy.sqlite")])


@then("the call graph migration regression passes")
def migration_passes(migration_result: subprocess.CompletedProcess[str]) -> None:
    require(migration_result.returncode == 0, migration_result.stdout + migration_result.stderr)


@then("name USR and positive-depth root selection agree")
def selectors(context: FactsToolContext) -> None:
    usr = rows(context, "SELECT usr FROM symbol WHERE qualified_name="
        "'call_graph_fixture::exactCalls'")[0][0]
    by_name = cli(context, "--function", "call_graph_fixture::exactCalls")
    by_usr = cli(context, "--function", usr)
    require(by_name.returncode == by_usr.returncode == 0 and by_name.stdout == by_usr.stdout,
            by_name.stdout + by_usr.stdout)
    bounded = cli(context, "--function", "call_graph_fixture::exactCalls", "--max-depth", "1")
    require("depth-truncated=true" in bounded.stdout, bounded.stdout)


@then("all-mode output is byte stable canonically ordered and excludes the virtual root")
def all_mode(context: FactsToolContext) -> None:
    first, second = cli(context, "--all"), cli(context, "--all")
    roots = [line for line in first.stdout.splitlines() if line.startswith("root=")]
    require(first.returncode == second.returncode == 0 and first.stdout == second.stdout,
            first.stdout + second.stdout)
    expected = [f"root={name} usr={usr}" for name, usr in rows(
        context, "SELECT qualified_name,usr FROM symbol WHERE node=1 AND is_definition=1 "
                 "ORDER BY qualified_name,usr,id")]
    require(roots == expected and "virtual root" not in first.stdout.lower(), str(roots))


@then("invalid graph state database and depth requests are diagnosed")
def invalid_requests(context: FactsToolContext) -> None:
    no_calls = context.run_root_path / "no-call-facts.sqlite"
    shutil.copy2(context.facts_database_path, no_calls)
    with sqlite3.connect(no_calls) as database:
        database.execute("DELETE FROM relation_site WHERE kind IN (1,18)")
    absent = run([str(context.facts_tool), "analyse", "call-graph", "-v", "0",
                  "-f", str(no_calls), "--all"])
    require(absent.returncode == 1 and "no call facts" in absent.stderr,
            absent.stdout + absent.stderr)
    broken = context.run_root_path / "broken.cpp"
    broken.write_text("int broken( {\n", encoding="utf-8")
    broken_conf = context.run_root_path / "broken-files.sqlite"
    broken_facts = context.run_root_path / "broken-facts.sqlite"
    imported = run([str(context.facts_tool), "import", "-v", "0", "-c",
                    str(broken_conf), "--extra-arg=-std=c++23", str(broken)])
    require(imported.returncode == 0, imported.stdout + imported.stderr)
    incomplete = run([str(context.facts_tool), "extract", "-v", "0", "-o",
                      str(broken_facts), "-c", str(broken_conf), str(broken)])
    require(incomplete.returncode == 1 and
            ("error:" in incomplete.stderr or "incomplete" in incomplete.stderr),
            incomplete.stdout + incomplete.stderr)
    with sqlite3.connect(context.facts_database_path) as database:
        database.execute("UPDATE relation_site SET receiver_type_id=NULL,certainty=1 "
                         "WHERE kind=18 AND destination_id=(SELECT id FROM symbol "
                         "WHERE qualified_name='call_graph_fixture::X::toString')")
    invalid = cli(context, "--function", "call_graph_fixture::exactCalls")
    require(invalid.returncode == 1 and "invalid relation-site receiver context" in invalid.stderr,
            invalid.stdout + invalid.stderr)
    missing = run([str(context.facts_tool), "analyse", "call-graph", "-v", "0", "-f",
                   str(context.run_root_path / "missing.sqlite"), "--all"])
    require(missing.returncode == 1 and "cannot open facts database" in missing.stderr,
            missing.stdout + missing.stderr)
    for value in ("0", "-1", "abc"):
        depth = cli(context, "--all", "--max-depth", value)
        require(depth.returncode != 0 and "--max-depth" in depth.stderr,
                depth.stdout + depth.stderr)


@given("the call graph architecture regression is run", target_fixture="architecture_result")
def architecture_regression(context: FactsToolContext) -> subprocess.CompletedProcess[str]:
    root = context.fixture_root.parents[2]
    return run([str(Path(__import__("sys").executable)),
                str(root / "tests/call_graph_architecture_test.py"), str(root)])


@then("the call graph architecture regression passes")
def architecture_passes(architecture_result: subprocess.CompletedProcess[str]) -> None:
    require(architecture_result.returncode == 0,
            architecture_result.stdout + architecture_result.stderr)


@then("representative deterministic text fields are present")
def text_fields(context: FactsToolContext) -> None:
    output = cli(context, "--function", "call_graph_fixture::exactCalls")
    for field in ("relation=Calls", "receiver=", "certainty=", "target=",
                  "cycle=", "depth-truncated=", "complete=true"):
        require(field in output.stdout, f"missing {field}:\n{output.stdout}")


@then("template receiver contexts collapse while dispatch targets remain")
def template_contexts(context: FactsToolContext) -> None:
    sites = rows(context, "SELECT receiver_type_id,certainty,COUNT(*) FROM relation_site "
        "WHERE kind=1 AND source_id=(SELECT id FROM symbol WHERE qualified_name="
        "'call_graph_fixture::invoke') GROUP BY receiver_type_id,certainty")
    targets = rows(context, "SELECT d.qualified_name FROM relation_site site JOIN symbol d "
        "ON d.id=site.destination_id WHERE site.kind=18 ORDER BY d.qualified_name")
    require(sites == [(None, 2, 1)], str(sites))
    require(targets == [("call_graph_fixture::X::toString",),
                        ("call_graph_fixture::Y::toString",)], str(targets))


@then("default traversal reports a complete external boundary")
def external_boundary(context: FactsToolContext) -> None:
    output = cli(context, "--function", "call_graph_fixture::externalRoot")
    require(output.returncode == 0 and "external-boundary=true" in output.stdout and
            "depth-truncated=false" in output.stdout and "complete=true truncated=0" in output.stdout,
            output.stdout + output.stderr)


@then("explicit depth truncation is distinct from an external boundary")
def depth_boundary(context: FactsToolContext) -> None:
    output = cli(context, "--function", "call_graph_fixture::depthRoot", "--max-depth", "1")
    require(output.returncode == 0 and "depth-truncated=true" in output.stdout and
            "external-boundary=false" in output.stdout and "complete=false" in output.stdout,
            output.stdout + output.stderr)
