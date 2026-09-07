from __future__ import annotations

import signal
import subprocess
import time

from pytest_bdd import given, then
from steps.call_graph_scope_steps import run
from support import callgraph_run as cg
from support.database import require
from support.scenario import FactsToolContext


@given("a large generated call graph is extracted")
def large_graph(context: FactsToolContext) -> None:
    context.prepare()
    source = context.run_root_path / "large.cpp"
    count = 5000
    functions = [f"int f{count}(){{return 0;}}"]
    functions.extend(f"int f{i}(){{return f{i + 1}();}}"
                     for i in range(count - 1, -1, -1))
    source.write_text("namespace budget_fixture {\n" + "\n".join(functions) +
                      "\nint root(){return f0();}\n}\n", encoding="utf-8")
    imported = run([str(context.facts_tool), "import", "-v", "0", "-c",
                    str(context.files_database_path), "--extra-arg=-std=c++23", str(source)])
    require(imported.returncode == 0, imported.stdout + imported.stderr)
    extracted = run([str(context.facts_tool), "extract", "-v", "0", "-o",
                     str(context.facts_database_path), "-c",
                     str(context.files_database_path), str(source)])
    require(extracted.returncode == 0, extracted.stdout + extracted.stderr)


def large_command(context: FactsToolContext, *extra: str) -> list[str]:
    return cg.command(context, "--function", "budget_fixture::root", *extra, conf=False)


@then("the monotonic time budget reports a coherent partial result")
def time_budget(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    result = run(large_command(context, "--time-limit-ms", "1"))
    run_id, status = cg.completion(result)
    require(result.returncode == 0 and status == "truncated", result.stdout + result.stderr)
    require(cg.run_row(facts, run_id)["truncation_reason"] == "time_limit", run_id)
    require(cg.edges(facts, run_id), "expected a non-empty partial edge set")
    require(cg.frontier(facts, run_id), "expected a non-empty frontier")


@then("SIGINT reports a coherent cancelled result and exit 130")
def cancellation(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    process = subprocess.Popen(large_command(context), stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True)
    time.sleep(0.2)
    process.send_signal(signal.SIGINT)
    stdout, stderr = process.communicate(timeout=30)
    require(process.returncode == 130, f"exit={process.returncode}\n{stdout}{stderr}")
    lines = stdout.splitlines()
    require(len(lines) == 1, stdout)
    match = cg.COMPLETION.match(lines[0])
    require(match is not None, stdout)
    run_id, status = int(match.group(1)), match.group(2)
    require(status == "cancelled", status)
    stderr_lines = stderr.splitlines()
    require(stderr_lines == [f"facts-tool: cancelled; run {run_id} keeps the "
                             "last usable generation"], stderr)
    row = cg.run_row(facts, run_id)
    require(row["status"] == "cancelled" and row["truncation_reason"] == "cancelled",
            str(row))
    frontier = cg.frontier(facts, run_id)
    require(any(reason == "cancelled" for _, reason in frontier), str(frontier))
