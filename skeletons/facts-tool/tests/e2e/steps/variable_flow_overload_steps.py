from __future__ import annotations

from pytest_bdd import given, parsers, then, when
from support.variable_flow import read_run, run
from support.variable_flow_overloads import prepare


def _same_type(actual: str, expected: str) -> bool:
    # Clang's builtin bool spelling is target-printing-policy dependent.
    return actual == expected or {actual, expected} == {"bool", "_Bool"}


@given("an isolated generated overload variable-flow project")
def generated_overload_project(context):
    prepare(context)


@when(parsers.parse('overload variable flow is analysed for function "{function}"'))
def analyse_overload(context, function):
    run(
        context,
        "--function",
        function,
        "--variable",
        "result",
        output=context.run_root_path / "overloads.variable-flow.db",
    )


@when("overload variable flow is analysed with the captured legacy USR")
def analyse_captured_legacy_usr(context):
    analyse_overload(context, context.overload_legacy_function_usr)


@then(
    parsers.parse(
        'the selected overload has root variable type "{root_type}" at source line '
        '{line:d} and parameter type "{parameter_type}"'
    )
)
def selected_overload_evidence(context, root_type, line, parameter_type):
    run_record = read_run(context)
    graph = run_record.graph
    root_nodes = graph.nodes(variable_usr=run_record.root_variable)
    assert root_nodes
    assert run_record.root_function
    assert run_record.root_variable
    assert all(node.type == root_type for node in root_nodes)
    local_writes = graph.nodes(
        kind="write",
        name="result",
        variable_usr=run_record.root_variable,
        function_usr=run_record.root_function,
    )
    assert len(local_writes) == 1, local_writes
    assert local_writes[0].location.line == line
    parameters = graph.nodes(
        kind="parameter",
        name="value",
        function_usr=run_record.root_function,
    )
    assert len(parameters) == 1, parameters
    assert _same_type(parameters[0].type, parameter_type)


@then("overload variable flow succeeds")
def overload_flow_succeeds(context):
    assert context.variable_flow_result.returncode == 0
    assert context.variable_flow_completion is not None


@then("the overload flow is rejected without publishing a run")
def overload_flow_rejected(context):
    assert context.variable_flow_result.returncode != 0
    assert context.variable_flow_completion is None
    assert not context.variable_flow_output.exists()


@then(parsers.parse('the overload rejection reports ambiguity for "{function}"'))
def overload_ambiguity_diagnostic(context, function):
    diagnostic = context.variable_flow_result.stderr.lower()
    assert "ambiguous" in diagnostic, context.variable_flow_result.stderr
    assert function in context.variable_flow_result.stderr


@then("the overload flow captures a nonempty root function USR")
def capture_legacy_usr(context):
    record = read_run(context)
    context.overload_legacy_function_usr = record.root_function
    context.overload_legacy_root_variable = record.root_variable
    assert context.overload_legacy_function_usr


@then("the captured USR selects the same root variable")
def captured_usr_selects_same_root(context):
    record = read_run(context)
    assert record.root_function == context.overload_legacy_function_usr
    assert record.root_variable == context.overload_legacy_root_variable
