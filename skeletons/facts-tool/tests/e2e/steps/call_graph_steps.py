from __future__ import annotations

import shutil
import sqlite3
import subprocess
import tempfile
from pathlib import Path

from pytest_bdd import given, then
from support import callgraph_run as cg
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


@then("direct method lambda constructor-body and constructor-invocation Calls are recorded once")
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
    require(constructor_edges == [(1,)], f"constructor invocation count: {constructor_edges}")


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
    facts = context.facts_database_path
    stored = rows(context, "SELECT receiver.qualified_name,site.certainty FROM relation_site site "
        "JOIN symbol d ON d.id=site.destination_id LEFT JOIN symbol receiver ON "
        "receiver.id=site.receiver_type_id WHERE site.kind=18 AND d.qualified_name="
        f"'call_graph_fixture::{target}::toString'")
    require(stored == [(f"call_graph_fixture::{receiver}", 1)], f"bad exact dispatch: {stored}")
    result = cg.run_graph(context, "--function", "call_graph_fixture::exactCalls")
    run_id, status = cg.completion(result)
    require(result.returncode == 0 and status == "complete", result.stdout + result.stderr)
    dispatch_edges = [edge for edge in cg.edges(facts, run_id) if edge["kind"] == 18 and
                      edge["source"] == "call_graph_fixture::Base::log"]
    matching = [edge for edge in dispatch_edges
                if edge["target"] == f"call_graph_fixture::{target}::toString"]
    require(matching, f"missing dispatch edge to {target}: {dispatch_edges}")
    edge = matching[0]
    context_rows = cg.query(facts, "SELECT receiver.qualified_name,site.certainty FROM "
        "relation_site site LEFT JOIN symbol receiver ON receiver.id=site.receiver_type_id "
        "WHERE site.source_id=? AND site.destination_id=? AND site.kind=? AND "
        "site.position=? AND site.file_id=? AND site.offset=?",
        (edge["source_id"], edge["target_id"], edge["kind"], edge["position"],
         edge["file_id"], edge["offset"]))
    require(context_rows == [(f"call_graph_fixture::{receiver}", 1)],
            f"dispatch edge receiver mismatch: {context_rows}")


@then("MessageX dispatch is exact in storage and in the persisted run")
def exact_x(context: FactsToolContext) -> None:
    assert_exact_dispatch(context, "X", "X")


@then("MessageY dispatch is exact in storage and in the persisted run")
def exact_y(context: FactsToolContext) -> None:
    assert_exact_dispatch(context, "Y", "Y")


@then("unproven receiver dispatch is possible and conservative")
def possible_dispatch(context: FactsToolContext) -> None:
    facts = context.facts_database_path
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
        "'call_graph_fixture::FallbackRoot::call') ORDER BY s.qualified_name,d.qualified_name")
    require(transitive == [
        ("call_graph_fixture::ExactRoot::call", "call_graph_fixture::ExactLeaf::value", 1),
        ("call_graph_fixture::FallbackRoot::call", "call_graph_fixture::FallbackRoot::value", 1),
        ("call_graph_fixture::PossibleRoot::call", "call_graph_fixture::PossibleLeaf::value", 2),
        ("call_graph_fixture::PossibleRoot::call", "call_graph_fixture::PossibleMid::value", 2),
    ], str(transitive))
    result = cg.run_graph(context, "--function", "call_graph_fixture::possibleCall")
    run_id, status = cg.completion(result)
    require(result.returncode == 0 and status == "complete", result.stdout + result.stderr)
    dispatch_edge = next(edge for edge in cg.edges(facts, run_id) if edge["kind"] == 18)
    context_rows = cg.query(facts, "SELECT receiver_type_id,certainty FROM relation_site "
        "WHERE source_id=? AND destination_id=? AND kind=? AND position=? AND "
        "file_id=? AND offset=?", (dispatch_edge["source_id"], dispatch_edge["target_id"],
        dispatch_edge["kind"], dispatch_edge["position"], dispatch_edge["file_id"],
        dispatch_edge["offset"]))
    require(context_rows == [(None, 2)], str(context_rows))
    transitive_result = cg.run_graph(context, "--function", "call_graph_fixture::transitivePossible")
    transitive_id, transitive_status = cg.completion(transitive_result)
    require(transitive_status == "complete", transitive_status)
    targets = {edge["target"] for edge in cg.edges(facts, transitive_id)}
    for target in ("call_graph_fixture::PossibleMid::value",
                   "call_graph_fixture::PossibleLeaf::value"):
        require(target in targets, str(targets))


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
    facts = context.facts_database_path
    usr = rows(context, "SELECT usr FROM symbol WHERE qualified_name="
        "'call_graph_fixture::exactCalls'")[0][0]
    by_name = cg.run_graph(context, "--function", "call_graph_fixture::exactCalls")
    by_usr = cg.run_graph(context, "--function", usr)
    name_id, name_status = cg.completion(by_name)
    usr_id, usr_status = cg.completion(by_usr)
    require(by_name.returncode == by_usr.returncode == 0, by_name.stderr + by_usr.stderr)
    require(name_status == usr_status, (name_status, usr_status))
    require(cg.roots(facts, name_id) == cg.roots(facts, usr_id), "root selection differs")
    require(cg.edges(facts, name_id) == cg.edges(facts, usr_id), "edge sets differ")
    bounded = cg.run_graph(context, "--function", "call_graph_fixture::exactCalls",
                           "--max-depth", "1")
    bounded_id, bounded_status = cg.completion(bounded)
    require(bounded_status == "truncated", bounded_status)
    require(cg.run_row(facts, bounded_id)["truncation_reason"] == "max_depth", bounded_id)
    require(cg.frontier(facts, bounded_id), "expected a frontier row")


@then("all-mode output is byte stable canonically ordered and excludes the virtual root")
def all_mode(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    first = cg.run_graph(context, "--all")
    second = cg.run_graph(context, "--all")
    first_id, _ = cg.completion(first)
    second_id, _ = cg.completion(second)
    require(first.returncode == second.returncode == 0, first.stderr + second.stderr)
    expected_roots = rows(context, "SELECT qualified_name,usr FROM symbol WHERE node=1 "
        "AND is_definition=1 ORDER BY qualified_name,usr")
    require(cg.roots(facts, first_id) == cg.roots(facts, second_id) == expected_roots,
            str((cg.roots(facts, first_id), expected_roots)))
    require(cg.edges(facts, first_id) == cg.edges(facts, second_id),
            "edge sets differ between identical --all runs")


@then("invalid graph state database and depth requests are diagnosed")
def invalid_requests(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    no_calls = context.run_root_path / "no-call-facts.sqlite"
    shutil.copy2(facts, no_calls)
    with sqlite3.connect(no_calls) as database:
        database.execute("DELETE FROM relation_site WHERE kind IN (1,18)")
    absent = run([str(context.facts_tool), "analyse", "call-graph", "-v", "0",
                  "-f", str(no_calls), "--all"])
    require(absent.returncode == 1 and absent.stdout == "" and
            len(cg.stderr_lines(absent)) == 1 and "no call facts" in absent.stderr,
            absent.stdout + absent.stderr)
    require(cg.run_count(no_calls) == 0, "usage error left a run")
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
    before_run_count = cg.run_count(facts)
    with sqlite3.connect(facts) as database:
        database.execute("UPDATE relation_site SET receiver_type_id=NULL,certainty=1 "
                         "WHERE kind=18 AND destination_id=(SELECT id FROM symbol "
                         "WHERE qualified_name='call_graph_fixture::X::toString')")
    invalid = cli(context, "--function", "call_graph_fixture::exactCalls")
    require(invalid.returncode == 1 and invalid.stdout == "" and
            len(cg.stderr_lines(invalid)) == 1 and
            "invalid relation-site receiver context" in invalid.stderr,
            invalid.stdout + invalid.stderr)
    require(cg.run_count(facts) == before_run_count, "operational error left a run")
    missing = run([str(context.facts_tool), "analyse", "call-graph", "-v", "0", "-f",
                   str(context.run_root_path / "missing.sqlite"), "--all"])
    require(missing.returncode == 1 and missing.stdout == "" and
            len(cg.stderr_lines(missing)) == 1 and
            "cannot open facts database" in missing.stderr,
            missing.stdout + missing.stderr)
    for value in ("0", "-1", "abc"):
        depth = cli(context, "--all", "--max-depth", value)
        require(depth.returncode != 0 and depth.stdout == "" and
                len(cg.stderr_lines(depth)) == 1 and
                "--max-depth" in depth.stderr, depth.stdout + depth.stderr)
    require(cg.run_count(facts) == before_run_count, "invalid depth requests left a run")


@given("the call graph architecture regression is run", target_fixture="architecture_result")
def architecture_regression(context: FactsToolContext) -> subprocess.CompletedProcess[str]:
    root = context.fixture_root.parents[2]
    return run([str(Path(__import__("sys").executable)),
                str(root / "tests/call_graph_architecture_test.py"), str(root)])


@then("the call graph architecture regression passes")
def architecture_passes(architecture_result: subprocess.CompletedProcess[str]) -> None:
    require(architecture_result.returncode == 0,
            architecture_result.stdout + architecture_result.stderr)


@then("representative deterministic run fields are present")
def text_fields(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    result = cg.run_graph(context, "--function", "call_graph_fixture::exactCalls")
    run_id, status = cg.completion(result)
    require(result.returncode == 0 and status == "complete", result.stdout + result.stderr)
    row = cg.run_row(facts, run_id)
    require(row["mode"] == "callees" and row["status"] == "complete" and
            row["truncation_reason"] is None, str(row))
    edges = cg.edges(facts, run_id)
    require(any(edge["kind"] == 1 and edge["depth"] >= 1 for edge in edges), str(edges))


@then("template receiver contexts collapse while dispatch targets remain")
def template_contexts(context: FactsToolContext) -> None:
    sites = rows(context, "SELECT receiver_type_id,certainty,COUNT(*) FROM relation_site "
        "WHERE kind=1 AND source_id=(SELECT id FROM symbol WHERE qualified_name="
        "'call_graph_fixture::invoke') GROUP BY receiver_type_id,certainty")
    targets = rows(context, "SELECT d.qualified_name FROM relation_site site JOIN symbol d "
        "ON d.id=site.destination_id WHERE site.kind=18 AND d.qualified_name LIKE "
        "'call_graph_fixture::%::toString' ORDER BY d.qualified_name")
    require(sites == [(None, 2, 1)], str(sites))
    require(targets == [("call_graph_fixture::X::toString",),
                        ("call_graph_fixture::Y::toString",)], str(targets))


@then("default traversal reports a complete external boundary")
def external_boundary(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    result = cg.run_graph(context, "--function", "call_graph_fixture::externalRoot")
    run_id, status = cg.completion(result)
    require(result.returncode == 0 and status == "complete", result.stdout + result.stderr)
    require(cg.run_row(facts, run_id)["truncation_reason"] is None, run_id)
    edge_pairs = cg.edge_names(facts, run_id)
    require(("call_graph_fixture::externalRoot", "call_graph_fixture::externalOnly")
            in edge_pairs, str(edge_pairs))
    require(not cg.frontier(facts, run_id), "expected no frontier for a complete run")


@then("explicit depth truncation is distinct from an external boundary")
def depth_boundary(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    result = cg.run_graph(context, "--function", "call_graph_fixture::depthRoot",
                          "--max-depth", "1")
    run_id, status = cg.completion(result)
    require(result.returncode == 0 and status == "truncated", result.stdout + result.stderr)
    require(cg.run_row(facts, run_id)["truncation_reason"] == "max_depth", run_id)
    require(cg.frontier(facts, run_id), "expected a frontier row")


def b040_sources(context: FactsToolContext) -> dict[str, Path]:
    root = context.fixture_root.parents[1] / "e2e" / "fixtures"
    names = ("b040_root.cpp", "b040_known.cpp", "b040_leaf.cpp",
             "b040_missing.cpp")
    return {name: (root / name).resolve(strict=True) for name in names}


def prepare_b040(context: FactsToolContext, include_missing: bool,
                 metadata: bool) -> None:
    context.prepare()
    sources = b040_sources(context)
    context.b040_external_directory = tempfile.TemporaryDirectory(
        prefix="b040-external-")
    external_root = Path(context.b040_external_directory.name)
    shutil.copy(context.fixture_root.parents[1] / "e2e" / "fixtures" /
                "external" / "b040_external.hpp", external_root)
    imported = run([str(context.facts_tool), "import", "-v", "0", "-c",
                    str(context.files_database_path), "--extra-arg=-std=c++23",
                    f"--extra-arg=-I{external_root}",
                    *(str(path) for path in sources.values())])
    require(imported.returncode == 0, imported.stdout + imported.stderr)
    selected = list(sources.values()) if include_missing else [
        sources["b040_root.cpp"], sources["b040_known.cpp"],
        sources["b040_leaf.cpp"]]
    extracted = run([str(context.facts_tool), "extract", "-v", "0", "-o",
                     str(context.facts_database_path), "-c",
                     str(context.files_database_path), *(str(path) for path in selected)])
    require(extracted.returncode == 0, extracted.stdout + extracted.stderr)
    if metadata:
        covered = ("b040_root.cpp", "b040_known.cpp", "b040_boundary.hpp")
        if include_missing:
            covered += ("b040_leaf.cpp", "b040_missing.cpp")
        with sqlite3.connect(context.files_database_path) as database:
            database.executemany(
                "UPDATE file SET indexed=1,indexed_at='2026-09-06T00:00:00Z' WHERE name=?",
                ((name,) for name in covered),
            )


@given("an isolated B-040 pair has a declaration-only project boundary")
def b040_partial_pair(context: FactsToolContext) -> None:
    prepare_b040(context, include_missing=False, metadata=True)


@given("an isolated B-040 pair has complete known project coverage")
def b040_complete_pair(context: FactsToolContext) -> None:
    prepare_b040(context, include_missing=True, metadata=True)


@then("B-040 records complete traversal with the missing definition edge and no further calls")
def b040_reproduction(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    result = cg.run_graph(context, "--function", "b040_fixture::root")
    run_id, status = cg.completion(result)
    require(result.returncode == 0 and status == "complete", result.stdout + result.stderr)
    require(cg.run_row(facts, run_id)["truncation_reason"] is None, run_id)
    edge_pairs = cg.edge_names(facts, run_id)
    require(any(target == "b040_fixture::missing" for _, target in edge_pairs),
            str(edge_pairs))
    require(not any(source == "b040_fixture::missing" for source, _ in edge_pairs),
            str(edge_pairs))


@then("B-040 records the external definition edge and nothing beyond it")
def b040_external(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    result = cg.run_graph(context, "--function", "b040_fixture::root")
    run_id, status = cg.completion(result)
    require(result.returncode == 0 and status == "complete", result.stdout + result.stderr)
    edge_pairs = cg.edge_names(facts, run_id)
    require(any(target == "b040_external::unavailable" for _, target in edge_pairs),
            str(edge_pairs))
    require(not any(source == "b040_external::unavailable" for source, _ in edge_pairs),
            str(edge_pairs))


@then("B-040 reports depth truncation as max_depth in the persisted run")
def b040_depth(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    result = cg.run_graph(context, "--function", "b040_fixture::root", "--max-depth", "1")
    run_id, status = cg.completion(result)
    require(result.returncode == 0 and status == "truncated", result.stdout + result.stderr)
    require(cg.run_row(facts, run_id)["truncation_reason"] == "max_depth", run_id)
    require(cg.frontier(facts, run_id), "expected a frontier row")
