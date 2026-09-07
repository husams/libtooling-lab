from __future__ import annotations

import json
import signal
import subprocess
import time

from pytest_bdd import given, then
from steps.call_graph_scope_steps import run
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
    return [str(context.facts_tool), "analyse", "call-graph", "-v", "0", "-f",
            str(context.facts_database_path), "--format", "json", "--function",
            "budget_fixture::root", *extra]


@then("the monotonic time budget reports a coherent partial result")
def time_budget(context: FactsToolContext) -> None:
    output = run(large_command(context, "--time-limit-ms", "1"))
    require(output.returncode == 0, output.stdout + output.stderr)
    value = json.loads(output.stdout)
    require(value["truncation"]["reason"] == "time_limit" and value["nodes"] and
            value["truncation"]["frontier"], str(value))


@then("SIGINT reports a coherent cancelled result and exit 130")
def cancellation(context: FactsToolContext) -> None:
    process = subprocess.Popen(large_command(context), stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True)
    time.sleep(0.2)
    process.send_signal(signal.SIGINT)
    stdout, stderr = process.communicate(timeout=30)
    require(process.returncode == 130,
            f"exit={process.returncode}\n{stdout}{stderr}")
    value = json.loads(stdout)
    require(value["truncation"]["reason"] == "cancelled" and not value["complete"],
            str(value))
