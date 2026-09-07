"""Outcome-matrix scenario outline wiring (B-042)."""
from pytest_bdd import when, then, parsers
from support.call_graph_matrix_cases import run_case
from support.call_graph_matrix_assert import assert_case
from support.callgraph_run import run_count


@when(parsers.parse("S-025 runs the {outcome} outcome case at verbosity {verbosity:d}"))
def run_outcome(context, outcome, verbosity):
    context.matrix_outcome = outcome
    context.matrix_verbosity = verbosity
    context.matrix_result = run_case(context, outcome, verbosity)


@then("the outcome matches its documented shape")
def outcome_shape(context):
    assert_case(context.matrix_outcome, context.matrix_verbosity, context.matrix_result)


@then(parsers.parse("the outcome exit code is {exit:d}"))
def outcome_exit(context, exit):
    assert context.matrix_result.returncode == exit, context.matrix_result


@then(parsers.parse("a run row is {presence} for the outcome case"))
def outcome_run_presence(context, presence):
    after = run_count(context.facts_database_path)
    before = context.matrix_run_before
    if presence == "present":
        assert after == before + 1, (before, after)
    else:
        assert after == before, (before, after)
