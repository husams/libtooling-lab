from __future__ import annotations

import statistics
import time

from pytest_bdd import then, when

from steps.matched_symbol_index_steps import match_caller
from steps.native_matcher_workflow_steps import run
from support.database import require
from support.scenario import FactsToolContext


def measure(context: FactsToolContext, label: str) -> list[float]:
    values = []
    for run_number in range(6):
        output = context.run_root_path / f"s026-{label}-{run_number}.sqlite"
        start = time.perf_counter()
        result = run([str(context.facts_tool), "extract", "-v", "0", "--conf",
                      str(context.files_database), "--output", str(output),
                      str(context.targeted_match_source)])
        elapsed = time.perf_counter() - start
        require(result.returncode == 0, result.stdout + result.stderr)
        if run_number:
            values.append(elapsed)
    return values


@when("S-026 extraction timing runs one warm-up and five measurements per state")
def time_extraction(context: FactsToolContext) -> None:
    baseline = measure(context, "empty-index")
    require(match_caller(context).returncode == 0, "cannot populate candidate index")
    candidate = measure(context, "populated-index")
    context.s026_timings = {"empty": baseline, "populated": candidate}


@then("the raw extraction timings and medians are recorded without a speed claim")
def timings_recorded(context: FactsToolContext) -> None:
    require(all(len(values) == 5 for values in context.s026_timings.values()), str(context.s026_timings))
    medians = {name: statistics.median(values) for name, values in context.s026_timings.items()}
    print(f"S-026 extraction timings raw={context.s026_timings} medians={medians}")
