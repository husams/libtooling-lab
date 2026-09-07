"""Case builders for the `analyse call-graph` outcome matrix (B-042)."""
import subprocess

from support.call_graph_matrix_cancel import (
    cancel_before_traversal, cancel_during_recovery)
from support.call_graph_matrix_db_cases import (
    database_empty_all, database_missing_facts, failed_after_traversal)
from support.callgraph_run import command, run_count


def _run(context, *args, conf=True, verbosity=0):
    argv = command(context, *args, conf=conf, verbosity=verbosity)
    return subprocess.run(argv, capture_output=True, text=True, env=context.recovery_env)


def _usage_case(*extra):
    def case(context, verbosity):
        return _run(context, "--function", "root", *extra, verbosity=verbosity)
    return case


def _configuration_no_project(context, verbosity):
    return _run(context, "--recover-missing", "--function", "root",
               conf=False, verbosity=verbosity)


def _recovery_failure(context, verbosity):
    context.recovery_sources[1].write_text("#error S025_RECOVERY_FAILURE\n")
    return _run(context, "--function", "root", "--recover-missing", verbosity=verbosity)


def _commit_failure(context, verbosity):
    setup = _run(context, "--function", "root", "--recover-missing")
    assert setup.returncode == 0, setup.stderr
    context.matrix_run_before = run_count(context.facts_database_path)
    context.facts_database_path.chmod(0o444)
    try:
        return _run(context, "--function", "root", verbosity=verbosity)
    finally:
        context.facts_database_path.chmod(0o644)


CASES = {
    "complete": lambda c, v: _run(c, "--function", "root", "--recover-missing", verbosity=v),
    "truncated": lambda c, v: _run(c, "--function", "root", "--max-nodes", "1", verbosity=v),
    "help": lambda c, v: _run(c, "--help", verbosity=v),
    "usage_format": _usage_case("--format", "json"),
    "usage_output": _usage_case("--output", "x.mmd"),
    "usage_unknown_option": _usage_case("--bogus-flag"),
    "usage_missing_root": lambda c, v: _run(c, verbosity=v),
    "usage_to_with_all": lambda c, v: _run(c, "--all", "--to", "leaf", verbosity=v),
    "usage_invalid_budget": _usage_case("--max-depth", "0"),
    "configuration_no_project": _configuration_no_project,
    "database_missing_facts": database_missing_facts,
    "database_empty_all": database_empty_all,
    "recovery_failure": _recovery_failure,
    "failed_after_traversal": failed_after_traversal,
    "cancel_before_traversal": cancel_before_traversal,
    "cancel_during_recovery": cancel_during_recovery,
    "commit_failure": _commit_failure,
}


def run_case(context, outcome, verbosity):
    if outcome != "commit_failure":
        context.matrix_run_before = run_count(context.facts_database_path)
    return CASES[outcome](context, verbosity)
