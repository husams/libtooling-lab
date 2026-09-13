from __future__ import annotations

from pathlib import Path

from pytest_bdd import parsers, then
from support.variable_flow import kind_matches, nodes, read_run, read_runs


def _matching(graph, *, kind=None, name=None, function=None, line=None):
    return tuple(
        node for node in nodes(graph)
        if (kind is None or node.kind == kind)
        and (name is None or node.name == name)
        and (function is None or node.function_usr.endswith(function))
        and (line is None or node.location.line == line)
    )


def _one(graph, **criteria):
    matches = _matching(graph, **criteria)
    assert len(matches) == 1, (criteria, [(node.kind, node.name, node.location.line)
                                         for node in matches])
    return matches[0]


def _exact_edge(graph, source, target, kind, callsite=None):
    matches = tuple(
        edge for edge in graph.edges
        if edge.source == source.id and edge.target == target.id
        and edge.kind == kind
        and (callsite is None or edge.callsite == callsite.id)
    )
    assert len(matches) == 1, (source, target, kind, callsite, matches)
    return matches[0]


@then("the variable-flow graph contains local reads, writes, and updates")
def local_effects(context):
    graph = read_run(context)
    assert _one(graph, kind="write", name="result", line=12)
    assert _one(graph, kind="update", name="result", line=18)
    assert _one(graph, kind="write", name="result", line=20)
    assert _one(graph, kind="read", name="result", line=25)
    assert _matching(graph, kind="parameter", name="value")
    kinds = {node.kind for node in nodes(graph)}
    assert any(kind_matches(kind, "read") for kind in kinds), kinds
    assert any(kind_matches(kind, "write", "definition") for kind in kinds), kinds
    assert any(kind_matches(kind, "update") for kind in kinds), kinds


@then("the graph has reaching writes from both branch and loop statements")
def branch_loop_writes(context):
    graph = read_run(context)
    branch_update = _one(graph, kind="update", name="result", line=18)
    branch_write = _one(graph, kind="write", name="result", line=20)
    loop_read = _one(graph, kind="read", name="result", line=23)
    loop_write = _one(graph, kind="write", name="result", line=23)
    returned = _one(graph, kind="read", name="result", line=25)
    _exact_edge(graph, branch_update, loop_read, "data")
    _exact_edge(graph, branch_write, loop_read, "data")
    _exact_edge(graph, loop_write, returned, "data")


@then("the graph includes cross-translation-unit producer and callee relationships")
def cross_tu_relationships(context):
    graph = read_run(context)
    assert len({node.function_usr for node in nodes(graph)}) >= 2
    assert any(Path(node.location.file).name == "variable_flow_calls.cpp" for node in nodes(graph))
    helper_call = _one(graph, kind="call", name="variable_flow::helper", line=12)
    helper_write = _one(graph, kind="write", name="result", line=12)
    _exact_edge(graph, helper_call, helper_write, "call-result", helper_call)
    produce_call = _one(graph, kind="call", name="variable_flow::produce", line=18)
    produce_return = _one(graph, kind="return", name="variable_flow::produce")
    helper_return = _one(graph, kind="return", name="variable_flow::helper")
    _exact_edge(graph, produce_return, produce_call, "return", produce_call)
    _exact_edge(graph, helper_return, helper_call, "return", helper_call)


@then("reference updates reach the caller while value copies stay separate")
def reference_vs_copy(context):
    graph = read_run(context)
    reference_call = _one(graph, kind="call",
                          name="variable_flow::update_reference", line=13)
    caller_read = _one(graph, kind="read", name="result", line=13)
    reference_parameter = _one(graph, kind="parameter", name="value", line=13)
    _exact_edge(graph, caller_read, reference_parameter, "argument-ref",
                reference_call)
    assert not any(
        edge.kind == "argument-ref" and edge.target == reference_parameter.id
        and edge.source == reference_call.id for edge in graph.edges
    ), "reference binding must be sourced by the caller read"

    callee_effects = tuple(
        node for node in nodes(graph)
        if node.name == "value" and node.location.line == 14
        and node.kind in {"write", "update"}
    )
    assert len(callee_effects) == 1, callee_effects
    callee_effect = callee_effects[0]
    caller_effect = _one(graph, kind="write", name="result", line=13)
    _exact_edge(graph, reference_call, caller_effect, "effect", reference_call)
    _exact_edge(graph, callee_effect, caller_effect, "reference-effect",
                reference_call)
    later_read = _one(graph, kind="read", name="result", line=17)
    _exact_edge(graph, caller_effect, later_read, "data")

    copy_call = _one(graph, kind="call", name="variable_flow::copy_value", line=14)
    copy_read = _one(graph, kind="read", name="result", line=14)
    copy_parameter = _one(graph, kind="parameter", name="value", line=18)
    _exact_edge(graph, copy_read, copy_parameter, "argument-copy", copy_call)
    assert not any(
        edge.kind == "argument-ref" and edge.target == copy_parameter.id
        and edge.callsite == copy_call.id for edge in graph.edges
    )
    assert not _matching(graph, kind="write", name="result", line=14)
    assert not any(
        edge.kind == "reference-effect" and edge.target == copy_parameter.id
        for edge in graph.edges
    )


@then("the unrelated call and variable are excluded from the dependency graph")
def unrelated_excluded(context):
    graph = read_run(context)
    assert not [node for node in nodes(graph) if node.name == "unrelated"]
    assert not [node for node in nodes(graph) if node.name == "noise"]


@then("depth zero contains only the selected function")
def depth_zero(context):
    graph = read_run(context)
    assert nodes(graph)
    assert {node.function_usr for node in nodes(graph)} == {nodes(graph)[0].function_usr}
    assert all(node.depth == 0 for node in nodes(graph))


@then(parsers.parse("the graph contains no nodes deeper than {depth:d}"))
def bounded_depth(context, depth):
    graph = read_run(context)
    assert all(node.depth <= depth for node in nodes(graph))
    assert all(boundary.depth <= depth for boundary in graph.boundaries)


@then("the unlimited graph reaches the producer definition")
def unlimited_reaches_definition(context):
    graph = read_run(context)
    assert any(Path(node.location.file).name == "variable_flow_calls.cpp" for node in nodes(graph))
    assert {"variable_flow::helper", "variable_flow::produce"} <= {
        node.name for node in nodes(graph) if node.kind == "call"
    }
    assert _one(graph, kind="function", name="variable_flow::produce")
    assert _one(graph, kind="return", name="variable_flow::produce")


@then("both helper callsites have linked return nodes")
def helper_return_links(context):
    graph = read_run(context)
    helper_calls = tuple(node for node in nodes(graph)
                         if node.kind == "call" and
                         node.name == "variable_flow::helper")
    assert len(helper_calls) == 2
    helper_return = _one(graph, kind="return", name="variable_flow::helper")
    for call in helper_calls:
        links = tuple(edge for edge in graph.edges
                      if edge.kind == "return" and edge.source == helper_return.id
                      and edge.target == call.id and edge.callsite == call.id)
        assert len(links) == 1, (call, links)


@then("both variable-flow runs are readable and ordered")
def repeated_runs(context):
    saved = read_runs(context)
    assert len(saved) == 2, saved
    assert saved[0].run_id == context.variable_flow_first_run_id
    assert saved[1].run_id > saved[0].run_id
    assert all(saved_run.status == "complete" for saved_run in saved)


@then("the run records an explicit external boundary")
def external_boundary(context):
    graph = read_run(context)
    call = _one(graph, kind="call", name="variable_flow::external_value", line=37)
    boundaries = tuple(boundary for boundary in graph.boundaries
                       if boundary.node == call.id)
    assert len(boundaries) == 1, boundaries
    assert boundaries[0].reason == "external", boundaries[0]


@then("the run records an explicit indirect boundary")
def indirect_boundary(context):
    graph = read_run(context)
    call = _one(graph, kind="call", name="indirect", line=41)
    boundaries = tuple(boundary for boundary in graph.boundaries
                       if boundary.node == call.id)
    assert len(boundaries) == 1, boundaries
    assert boundaries[0].reason == "indirect", boundaries[0]
    assert graph.status == "partial"


@then("the invalid variable-flow request publishes no run")
def no_invalid_publication(context):
    assert context.variable_flow_result.returncode != 0
    assert not (context.run_root_path / "invalid.db").exists()


@then("the parse error publishes no variable-flow run")
def no_parse_publication(context):
    assert context.variable_flow_result.returncode != 0
    assert not (context.run_root_path / "parse-error.db").exists()


@then("the explicit flow output is readable")
def explicit_output_readable(context):
    assert context.variable_flow_result.returncode == 0
    assert read_runs(context)


@then("the per-source facts inputs are unchanged")
def per_source_inputs_unchanged(context):
    assert context.variable_flow_result.returncode == 0
    assert read_runs(context)
    for path, before in context.variable_flow_per_source_facts_before.items():
        assert path.read_bytes() == before, path


@then("the alien output is rejected without changing the input facts database")
def alien_output_rejected(context):
    assert context.variable_flow_result.returncode != 0
    assert context.facts_database_path.read_bytes() == context.variable_flow_facts_before


@then("the input facts database is unchanged and the default flow database is readable")
def default_output_readable(context):
    assert context.facts_database_path.read_bytes() == context.variable_flow_facts_before
    assert context.variable_flow_output == context.facts_database_path.with_suffix(
        ".variable-flow.db"
    )
    assert read_runs(context)
