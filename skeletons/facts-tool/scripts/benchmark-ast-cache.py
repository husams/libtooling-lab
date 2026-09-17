#!/usr/bin/env python3
"""Measure committed multi-TU AST-cache reuse and assert zero repeated front-end work."""
from __future__ import annotations

import argparse
from collections import Counter
from contextlib import closing
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import sqlite3
import statistics
import subprocess
import sys
import time

SENTINEL = "FACTS_CACHE_FRONTEND_SENTINEL"
CACHE_TABLES = ("ast_cache_snapshot", "ast_cache_input", "ast_cache_include",
                "ast_cache_revision", "ast_cache_artifact",
                "driver_include_probe", "driver_include_path")


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True,
                        help="C++ compiler; g++ additionally verifies driver-probe reuse")
    parser.add_argument("--output", type=Path, required=True,
                        help="new directory for fixture, logs, databases, and results.json")
    parser.add_argument("--translation-units", type=int, default=12)
    parser.add_argument("--declarations", type=int, default=64,
                        help="shared template declarations, in addition to standard headers")
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument("--build-description", default="unspecified",
                        help="record build mode, optimization flags and linked Clang version")
    parser.add_argument("--observe", action="store_true",
                        help="collect a failing baseline without stopping at regressions")
    args = parser.parse_args()
    if args.translation_units < 2 or args.runs < 1 or args.declarations < 1:
        parser.error("need >=2 translation units, >=1 run and >=1 declaration")
    args.binary = args.binary.resolve(strict=True)
    args.compiler = args.compiler.resolve(strict=True)
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    return args


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def isolated_environment(output):
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("FACTS_TOOL_", "XDG_", "GIT_"))}
    env.update(XDG_CONFIG_HOME=str(output / "user-config"),
               XDG_DATA_HOME=str(output / "user-data"), FACTS_TOOL_TIMING="1")
    return env


def git(root, env, *args):
    result = subprocess.run(
        ["git", "-c", "user.name=Facts Cache Benchmark",
         "-c", "user.email=facts-cache@example.invalid",
         "-c", "core.hooksPath=/dev/null", "-c", "commit.gpgSign=false",
         "-C", str(root), *args], env=env, capture_output=True, text=True,
        check=True, timeout=30)
    return result.stdout.strip()


def create_fixture(args, env):
    root = args.output / "project"
    root.mkdir()
    build = root / "build"
    build.mkdir()
    headers = root / "include" / "detail"
    headers.mkdir(parents=True)
    templates = "\n".join(
        f"template<class T> struct BenchRecord{i} {{ T value; T get() const {{ return value + {i}; }} }};"
        for i in range(args.declarations))
    (headers / "records.hpp").write_text(
        "#pragma once\n#include <array>\n#include <utility>\n#include <type_traits>\n"
        + templates + "\ninline int bench_adjust(int value) { return value + 1; }\n")
    (root / "include" / "shared.hpp").write_text(
        '#pragma once\n#include "detail/records.hpp"\n')
    driver = args.compiler
    # The real GNU driver is called only for platform include discovery. This
    # records actual subprocess work; the in-process Clang parser is separately
    # observed by source diagnostics and the dirty-input poison control.
    gnu = "g++" in args.compiler.name and "clang++" not in args.compiler.name
    probe_log = args.output / "compiler-probes.jsonl"
    if gnu:
        driver = args.output / "driver" / "g++"
        driver.parent.mkdir()
        driver.write_text(
            f"#!{sys.executable}\nimport json, os, sys\n"
            f"with open({str(probe_log)!r}, 'a') as log:\n"
            "    log.write(json.dumps(sys.argv[1:]) + '\\n')\n"
            f"os.execv({str(args.compiler)!r}, [{str(args.compiler)!r}, *sys.argv[1:]])\n")
        driver.chmod(0o755)
    commands = []
    sources = []
    for index in range(args.translation_units):
        relative = Path("src") / f"module_{index:02}" / "basic" / "unit.cpp"
        source = root / relative
        source.parent.mkdir(parents=True)
        next_index = index + 1
        declaration = (f"int bench_worker_{next_index}(int);\n"
                       if next_index < args.translation_units else "")
        expression = (f"bench_worker_{next_index}(bench_adjust(input))"
                      if next_index < args.translation_units else "bench_adjust(input)")
        body = (f'#pragma message("{SENTINEL}_{index:02}")\n'
                '#include "shared.hpp"\n' + declaration
                + f"std::array<int, 4> bench_values_{index} = {{1, 2, 3, 4}};\n"
                + f"int bench_worker_{index}(int input) {{ return {expression}; }}\n")
        if index == 0:
            body += "int bench_entry() { int seed = 7; return bench_worker_0(seed); }\n"
        source.write_text(body)
        sources.append(source)
        compiled = str(Path("..") / relative)
        commands.append({"directory": str(build), "file": compiled,
                         "arguments": [str(driver), "-std=c++23", "-I../include",
                                       "-c", compiled, "-o", f"unit_{index}.o"]})
    (root / "compile_commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    (root / ".gitignore").write_text("*.db\n*.db-*\n.facts-tool/\nsource-link.cpp\n")
    git(root, env, "init", "--quiet")
    git(root, env, "add", "src", "include", "compile_commands.json", ".gitignore")
    git(root, env, "commit", "--quiet", "-m", "Committed multi-TU benchmark input")
    for enabled in (True, False):
        (args.output / ("enabled.json" if enabled else "disabled.json")).write_text(
            json.dumps({"ast_cache": enabled,
                        "ast_cache_dir": str(root / ".facts-tool" / "ast-cache"),
                        "facts_template": str(root / "facts.db")}))
    return root, sources, probe_log, gnu


def cache_snapshot(root):
    artifacts = {str(path.relative_to(root)): {"sha256": digest(path),
                 "mtime_ns": path.stat().st_mtime_ns, "bytes": path.stat().st_size}
                 for path in sorted((root / ".facts-tool" / "ast-cache").glob("*.ast"))}
    tables = {}
    database = root / "project.db"
    if database.exists():
        with closing(sqlite3.connect(database)) as connection:
            present = {row[0] for row in connection.execute(
                "SELECT name FROM sqlite_master WHERE type='table'")}
            tables = {table: sorted(connection.execute(f'SELECT * FROM "{table}"').fetchall(), key=repr)
                      for table in CACHE_TABLES if table in present}
    return {"artifacts": artifacts, "metadata": tables}


def probe_count(path):
    return len(path.read_text().splitlines()) if path.exists() else 0


def cache_events(text):
    return dict(Counter(f"{family}:{event}" for family, event in re.findall(
        r"(?:^|\n)(ast-cache|dependency-cache): (hit|miss|stored|unavailable)\b", text)))


def frontend_counts(text):
    # Diagnostic source excerpts repeat the string: count only diagnostic lines.
    return dict(Counter(re.findall(
        rf"(?:warning|error): ({SENTINEL}_\d+)(?:\s|$)", text)))


def command(args, root, family, enabled=True, selectors=()):
    facts = root / "facts.db"
    commands = {
        "import": ["import", "--facts", str(facts), "-p", str(root)],
        "extract-force": ["extract", "--force", "--output", str(facts)],
        "extract-unchanged": ["extract", "--output", str(facts)],
        "match": ["match", "--facts", str(facts), "--matcher",
                  'functionDecl(matchesName("^bench_")).bind("symbol")'],
        "variable-flow": ["analyse", "variable-flow", "--function", "bench_entry",
                          "--variable", "seed", "--output", str(root / "flow.db")],
        "callgraph-recovery": ["analyse", "call-graph", "--function", "bench_entry",
                               "--recover-missing", "--facts", str(root / "recovery.db")],
        "dependency": ["analyse", "dependency", "--output", str(facts)],
    }
    if family == "dependency" and not selectors:
        selectors = sorted((root / "src").rglob("*.cpp"))
    return [str(args.binary), *commands[family], "--conf", str(root / "project.db"),
            "--config", str(args.output / ("enabled.json" if enabled else "disabled.json")),
            "-v", "1", *map(str, selectors)]


def check(record, condition, description):
    if not condition:
        record.setdefault("violations", []).append(description)


def restore_recovery(root):
    """Create a fresh destination from a closed, read-only SQLite seed."""
    destination = root / "recovery.db"
    with closing(sqlite3.connect((root / "recovery-seed.db").as_uri() + "?mode=ro", uri=True)) as source:
        seed_runs = source.execute("SELECT count(*) FROM callgraph_run").fetchone()[0]
        if seed_runs:
            raise RuntimeError("recovery seed contains prior graph runs")
        # All harness connections are explicitly closed. These files belong
        # exclusively to this generated benchmark fixture, and the previous
        # CLI process has exited before we remove its database and sidecars.
        for suffix in ("", "-wal", "-shm", "-journal"):
            Path(str(destination) + suffix).unlink(missing_ok=True)
        with closing(sqlite3.connect(destination)) as target:
            source.backup(target)
            restored_runs = target.execute("SELECT count(*) FROM callgraph_run").fetchone()[0]
            if restored_runs:
                raise RuntimeError("restored recovery destination is not fresh")
    return {"seed_runs": seed_runs, "restored_runs": restored_runs}


def measure(args, root, probe_log, records, label, family, enabled=True,
            selectors=(), expect_warm=True, expected_status=0, cwd=None):
    # Restore incomplete facts before EVERY invocation, forcing actual AST
    # recovery instead of timing a database-only query after the first run.
    recovery_restore = restore_recovery(root) if family == "callgraph-recovery" else None
    before = cache_snapshot(root)
    probes_before = probe_count(probe_log)
    argv = command(args, root, family, enabled, selectors)
    started = time.perf_counter()
    result = subprocess.run(argv, cwd=cwd or root, env=isolated_environment(args.output),
                            capture_output=True, text=True, timeout=600)
    elapsed = time.perf_counter() - started
    stem = args.output / "logs" / label
    stem.parent.mkdir(exist_ok=True)
    diagnostics = result.stderr + result.stdout
    stem.with_suffix(".log").write_text(diagnostics)
    record = {"label": label, "family": family, "enabled": enabled,
              "command": argv, "cwd": str(cwd or root), "wall_s": elapsed, "returncode": result.returncode,
              "frontend_activity": dict(Counter(re.findall(r"(?:^|\n)frontend: ([a-z-]+) ", diagnostics))),
              "frontend_passes": frontend_counts(diagnostics),
              "driver_probes": probe_count(probe_log) - probes_before,
              "events": cache_events(diagnostics),
              "phases_ms": {name: float(value) for name, value in re.findall(
                  r"facts-tool timing: (.*): ([\d.e+\-]+) ms", result.stderr)},
              "cache_unchanged": before == cache_snapshot(root), "violations": []}
    if recovery_restore is not None:
        record["recovery_restore"] = recovery_restore
        with closing(sqlite3.connect(root / "recovery.db")) as connection:
            record["recovery_runs"] = connection.execute("SELECT run_id,status FROM callgraph_run").fetchall()
        check(record, record["recovery_runs"] == [(1, "complete")],
              "fresh recovery invocation did not produce exactly complete run 1")
    check(record, result.returncode == expected_status,
          f"expected exit {expected_status}, got {result.returncode}; see {stem.with_suffix('.log')}")
    if enabled and expect_warm:
        check(record, not record["frontend_passes"], "warm command preprocessed source input")
        check(record, not record["frontend_activity"], "warm command repeated frontend activity")
        check(record, record["driver_probes"] == 0, "warm command executed compiler driver probes")
        check(record, record["cache_unchanged"], "warm command rewrote AST artifact or dependency metadata")
        check(record, not any(key.endswith((":miss", ":stored", ":unavailable"))
                              for key in record["events"]), "warm cache miss, rewrite, or unavailable event")
        selected = len({str(((cwd or root) / path).resolve()) for path in selectors}) if selectors else args.translation_units
        expected_asts = 0 if family in ("extract-unchanged", "dependency") else selected
        check(record, record["events"].get("ast-cache:hit", 0) == expected_asts,
              f"expected {expected_asts} AST hits, observed {record['events'].get('ast-cache:hit', 0)}")
        if family in ("import", "extract-force", "extract-unchanged", "dependency"):
            check(record, record["events"].get("dependency-cache:hit", 0) == selected,
                  f"expected {selected} dependency hits, observed {record['events'].get('dependency-cache:hit', 0)}")
        if family == "dependency":
            check(record, not any(key.startswith("ast-cache:") for key in record["events"]),
                  "dependency-only command loaded or created an AST")
    records.append(record)
    with (args.output / "measurements.jsonl").open("a") as log:
        log.write(json.dumps(record) + "\n")
    print(f"{label}: {elapsed:.4f}s, frontend={sum(record['frontend_passes'].values())}, "
          f"probes={record['driver_probes']}, violations={len(record['violations'])}", flush=True)
    if record["violations"] and not args.observe:
        raise RuntimeError(f"measurement verification failed: {label}")
    return record


def seed_recovery(args, root, sources, env):
    for family, selected in (("extract-force", sources[:1]), ("match", sources[1:])):
        argv = command(args, root, family, selectors=selected)
        argv[argv.index(str(root / "facts.db"))] = str(root / "recovery-seed.db")
        result = subprocess.run(argv, cwd=root, env=env, capture_output=True,
                                text=True, timeout=600)
        (args.output / "logs" / f"recovery-seed-{family}.log").write_text(result.stderr + result.stdout)
        if result.returncode:
            raise RuntimeError("recovery fixture setup failed; inspect recovery-seed logs")


def semantic_checks(root, record, translation_units):
    with closing(sqlite3.connect(root / "facts.db")) as connection:
        names = {row[0] for row in connection.execute("SELECT qualified_name FROM symbol WHERE is_definition=1")}
        expected = {"bench_entry", *(f"bench_worker_{index}" for index in range(translation_units))}
        check(record, expected <= names, "extraction/match omitted fixture function definitions")
        check(record, connection.execute("SELECT count(*) FROM include_dependency").fetchone()[0] > 0,
              "dependency graph is empty")
    with closing(sqlite3.connect(root / "flow.db")) as connection:
        check(record, connection.execute("SELECT status FROM variable_flow_run ORDER BY run_id DESC LIMIT 1").fetchone()
              == ("complete",), "variable flow did not complete")
        functions = connection.execute("SELECT count(*) FROM variable_flow_node WHERE kind='function' "
                                       "AND run_id=(SELECT max(run_id) FROM variable_flow_run)").fetchone()[0]
        check(record, functions >= translation_units + 2,
              "variable flow omitted functions from the cross-TU chain")
    with closing(sqlite3.connect(root / "recovery.db")) as connection:
        check(record, connection.execute("SELECT count(*) FROM callgraph_run_recovery").fetchone()[0] > 0,
              "callgraph did not exercise recovery")
        check(record, connection.execute("SELECT status FROM callgraph_run ORDER BY run_id DESC LIMIT 1").fetchone()
              == ("complete",), "callgraph run did not complete")
        check(record, not connection.execute("SELECT 1 FROM callgraph_run_recovery WHERE outcome='failed'").fetchall(),
              "callgraph recovery contains failures")
        edges = set(connection.execute("SELECT s.qualified_name,d.qualified_name FROM callgraph_run_edge e "
                                       "JOIN symbol s ON s.id=e.source_id JOIN symbol d ON d.id=e.destination_id"))
        expected = {("bench_entry", "bench_worker_0"),
                    *((f"bench_worker_{index}", f"bench_worker_{index + 1}")
                      for index in range(translation_units - 1))}
        check(record, expected <= edges, "callgraph recovery omitted cross-TU chain edges")


def poison_inputs(sources, header):
    originals = {path: path.read_text() for path in [*sources, header]}
    for path, text in originals.items():
        path.write_text("#error FACTS_CACHE_POISON_MUST_NOT_BE_PREPROCESSED\n" + text)
    return originals


def summaries(records):
    groups = {}
    for record in records:
        if record["label"].startswith("timed-"):
            key = f"{record['family']}:{'cached' if record['enabled'] else 'disabled'}"
            groups.setdefault(key, []).append(record)
    return {key: {"runs": len(values), "median_s": statistics.median(row["wall_s"] for row in values),
                  "p95_s": sorted(row["wall_s"] for row in values)[math.ceil(.95 * len(values)) - 1],
                  "frontend_passes": sum(sum(row["frontend_passes"].values()) for row in values),
                  "frontend_activity": dict(sum((Counter(row["frontend_activity"]) for row in values), Counter())),
                  "driver_probes": sum(row["driver_probes"] for row in values),
                  "cache_misses": sum(row["events"].get("ast-cache:miss", 0)
                                      + row["events"].get("dependency-cache:miss", 0) for row in values)}
            for key, values in groups.items()}


def main():
    args = arguments()
    env = isolated_environment(args.output)
    root, sources, probes, gnu = create_fixture(args, env)
    records = []
    inputs = {"binary": digest(args.binary), "compiler": digest(args.compiler),
              "harness": digest(Path(__file__))}
    report = {"platform": platform.platform(), "machine": platform.machine(),
              "cpu_count": os.cpu_count(), "binary": str(args.binary),
              "build_description": args.build_description,
              "compiler": str(args.compiler), "input_sha256": inputs,
              "translation_units": len(sources), "shared_template_declarations": args.declarations,
              "standard_headers": ["array", "utility", "type_traits"],
              "gnu_driver_instrumented": gnu, "warmups": 1, "p95_method": "nearest rank",
              "notes": ["Elapsed whole-process wall time; snapshot/hash validation is outside timer.",
                        "Synthetic committed fixture, not a production codebase or RHEL qualification.",
                        "Runs must occur without concurrent builds/test jobs for useful timings.",
                        "Cache hits still perform commit checks, artifact hashing/loading and command-specific analysis.",
                        "Pragma diagnostics count actual preprocessing passes; poison control checks the same-commit policy.",
                        "Callgraph incomplete facts are restored before every timed run."], "records": records}
    try:
        cold = measure(args, root, probes, records, "cold-import", "import", expect_warm=False)
        if cold["returncode"]:
            raise RuntimeError("cold import failed; benchmark setup is not usable")
        check(cold, not gnu or cold["driver_probes"] > 0, "GNU probe wrapper positive control was not exercised")
        check(cold, cold["events"].get("ast-cache:stored") == len(sources),
              "cold import did not prepare exactly one AST per TU")
        check(cold, len(cold["frontend_passes"]) == len(sources)
              and set(cold["frontend_passes"].values()) == {1},
              "cold import did not preprocess each TU exactly once")
        report["prepared_cache"] = cache_snapshot(root)
        dependency_paths = {row[1] for row in report["prepared_cache"]["metadata"].get("ast_cache_input", [])}
        report["fixture_counts"] = {"unique_dependency_files": len(dependency_paths),
            "external_dependency_files": sum(not Path(path).is_relative_to(root) for path in dependency_paths),
            "project_headers": len(tuple((root / "include").rglob("*.hpp"))),
            "ast_artifacts": len(report["prepared_cache"]["artifacts"]),
            "serialized_ast_bytes": sum(row["bytes"] for row in report["prepared_cache"]["artifacts"].values()),
            "source_bytes": sum(path.stat().st_size for path in sources)}
        report["initial_git_head"] = git(root, env, "rev-parse", "HEAD")
        # First consumers use the exact ASTs import prepared, before timing warmups.
        for family in ("extract-force", "match", "variable-flow", "dependency"):
            measure(args, root, probes, records, "first-" + family, family)
        seed_recovery(args, root, sources, env)
        measure(args, root, probes, records, "first-callgraph-recovery", "callgraph-recovery")
        semantic_checks(root, records[-1], len(sources))
        # Reproduce nested relative selectors and their canonical equivalents.
        selector_cases = {"absolute": ([sources[0]], root),
                          "relative": ([sources[0].relative_to(root)], root),
                          "dot-relative": (["./" + str(sources[0].relative_to(root))], root),
                          "duplicates": ([sources[0], sources[0]], root),
                          "overlap": ([sources[0], sources[0].relative_to(root)], root),
                          "all-sources": ([], root),
                          "directory-invocation": ([sources[0].name], sources[0].parent)}
        for selector, (paths, invocation) in selector_cases.items():
            for family in ("extract-force", "match", "variable-flow", "dependency"):
                measure(args, root, probes, records, f"selector-{selector}-{family}", family,
                        selectors=paths, cwd=invocation)
        families = ("import", "extract-force", "extract-unchanged", "match",
                    "variable-flow", "callgraph-recovery", "dependency")
        for iteration in range(args.runs + 1):
            # Alternate cached/disabled order; exclude one warmup per case.
            for family in families:
                for enabled in ((True, False) if iteration % 2 == 0 else (False, True)):
                    prefix = "warmup" if iteration == 0 else "timed"
                    label = f"{prefix}-{iteration}-{family}-{'cached' if enabled else 'disabled'}"
                    record = measure(args, root, probes, records, label, family, enabled)
                    if not enabled and family != "extract-unchanged":
                        check(record, bool(record["frontend_passes"]),
                              "disabled positive control did not exercise preprocessing")
            semantic_checks(root, records[-1], len(sources))
        # Same-commit source AND transitive header poison must be invisible to
        # cached preprocessing/AST consumers. Disabled processing must fail.
        header = root / "include" / "detail" / "records.hpp"
        originals = poison_inputs(sources, header)
        try:
            for family in families:
                measure(args, root, probes, records, "poison-" + family, family)
            failed = measure(args, root, probes, records, "poison-disabled-control",
                             "extract-force", enabled=False, expected_status=1)
            log = (args.output / "logs" / "poison-disabled-control.log").read_text()
            check(failed, "FACTS_CACHE_POISON_MUST_NOT_BE_PREPROCESSED" in log,
                  "disabled poison control did not encounter invalid current source")
        finally:
            for path, text in originals.items():
                path.write_text(text)
        # A source commit, shared-header commit and empty commit each invalidate
        # the recorded repository generation. Re-import must do the work once;
        # the immediate AST/dependency consumers must already be warm afterward.
        for label, path in (("source-commit", sources[0]), ("header-commit", header),
                            ("empty-commit", None)):
            if path:
                path.write_text(path.read_text() + f"\n// {label}\n")
                git(root, env, "add", str(path.relative_to(root)))
            git(root, env, "commit", "--quiet", "--allow-empty", "-m", label)
            refreshed = measure(args, root, probes, records, label + "-import", "import", expect_warm=False)
            check(refreshed, refreshed["events"].get("ast-cache:stored") == len(sources),
                  "new repository commit did not rebuild all affected TU snapshots")
            check(refreshed, len(refreshed["frontend_passes"]) == len(sources)
                  and set(refreshed["frontend_passes"].values()) == {1},
                  "commit refresh did not preprocess each TU exactly once")
            for family in ("extract-force", "match", "variable-flow", "dependency"):
                measure(args, root, probes, records, label + "-first-" + family, family)
        check(records[-1], digest(args.binary) == inputs["binary"]
              and digest(args.compiler) == inputs["compiler"]
              and digest(Path(__file__)) == inputs["harness"], "binary/compiler/harness changed during benchmark")
    except (Exception, KeyboardInterrupt) as error:
        report["error"] = str(error) or "Interrupted"
    finally:
        report["summary"] = summaries(records)
        report["violations"] = [{"label": row["label"], "detail": detail}
                                for row in records for detail in row["violations"]]
        report["passed"] = not report.get("error") and not report["violations"]
        (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps({"passed": report["passed"], "violations": len(report["violations"]),
                          "error": report.get("error"), "results": str(args.output / "results.json")}), flush=True)
    return 0 if report["passed"] or args.observe else 1


if __name__ == "__main__":
    raise SystemExit(main())
