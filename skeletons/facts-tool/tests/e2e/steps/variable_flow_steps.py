from __future__ import annotations

from pytest_bdd import given, parsers, then, when
from support.variable_flow import prepare, run, run_id, status


@given("the isolated variable-flow fixture project")
def isolated_fixture(context):
    prepare(context)


@given("the variable-flow fixture is extracted")
def extracted_fixture(context):
    if not hasattr(context, "variable_flow_sources"):
        prepare(context)


@when(parsers.parse('variable flow is analysed for function "{function}" and variable "{variable}"'))
def analyse_default(context, function, variable):
    run(context, "--function", function, "--variable", variable)


@when(parsers.parse('variable flow is analysed for function "{function}" and variable "{variable}" at line {line:d}'))
def analyse_line(context, function, variable, line):
    run(context, "--function", function, "--variable", variable, "--line", str(line))


@when(parsers.parse('variable flow is analysed for function "{function}" and variable "{variable}" with maximum depth {depth:d}'))
def analyse_depth(context, function, variable, depth):
    run(context, "--function", function, "--variable", variable,
        "--max-depth", str(depth), output=context.run_root_path / f"flow-depth-{depth}.db")


@when(parsers.parse('variable flow is analysed for function "{function}" and variable "{variable}" using stored compile commands'))
def analyse_stored(context, function, variable):
    run(context, "--function", function, "--variable", variable)


@when(parsers.parse('variable flow is analysed for function "{function}" and variable "{variable}" with sources'))
def analyse_sources(context, function, variable):
    run(context, "--function", function, "--variable", variable, sources=True,
        output=context.run_root_path / "flow-with-sources.db")


@then("the variable-flow run is complete")
def complete(context):
    assert context.variable_flow_result.returncode == 0
    assert status(context) == "complete"


@then(parsers.parse('the variable-flow run has status "{expected}"'))
def expected_status(context, expected):
    assert status(context) == expected


@when(parsers.parse('variable flow is analysed for function "{function}" and variable "{variable}" repeatedly'))
def analyse_repeated(context, function, variable):
    output = context.run_root_path / "flow-repeated.db"
    run(context, "--function", function, "--variable", variable, output=output)
    context.variable_flow_first_run_id = run_id(context)
    run(context, "--function", function, "--variable", variable, output=output)


@when(parsers.parse('variable flow is analysed with invalid function "{function}" and variable "{variable}"'))
def invalid_selection(context, function, variable):
    run(context, "--function", function, "--variable", variable,
        output=context.run_root_path / "invalid.db")


@when("variable flow is analysed with a source parse error")
def parse_error(context):
    context.variable_flow_sources[0].write_text("int broken( { return 1; }\n",
                                                encoding="utf-8")
    run(context, "--function", "broken", "--variable", "missing",
        output=context.run_root_path / "parse-error.db", sources=True)


@when("the input facts database is snapshotted and variable flow uses its default output")
def default_output(context):
    context.variable_flow_facts_before = context.facts_database_path.read_bytes()
    run(context, "--function", "variable_flow::root", "--variable", "seed")


@when("variable flow is analysed with an explicit output and no YAML config")
def explicit_output_without_yaml(context):
    context.variable_flow_explicit_output = context.run_root_path / "explicit-flow.db"
    run(context, "--function", "variable_flow::root", "--variable", "seed",
        output=context.variable_flow_explicit_output, config=False)


@when("variable flow is analysed with a per-source facts template and explicit output")
def explicit_output_with_per_source_template(context):
    per_source_root = context.run_root_path / "per-source-facts"
    per_source_root.mkdir()
    facts_inputs = {}
    for source in context.variable_flow_sources:
        facts_path = per_source_root / f"{source.stem}.db"
        extracted = context.run([
            str(context.facts_tool), "extract", "--output", str(facts_path),
            "--conf", str(context.files_database_path), str(source),
        ])
        assert extracted.returncode == 0, extracted.stdout + extracted.stderr
        facts_inputs[facts_path] = facts_path.read_bytes()
    config = context.run_root_path / "variable-flow-per-source.yaml"
    config.write_text(
        f"conf_template: '{context.files_database_path}'\n"
        "facts_template: '{project_root}/per-source-facts/{filename}.db'\n",
        encoding="utf-8",
    )
    context.variable_flow_per_source_facts_before = facts_inputs
    context.variable_flow_per_source_config = config
    context.variable_flow_per_source_output = context.run_root_path / "explicit-per-source-flow.db"
    run(context, "--function", "variable_flow::root", "--variable", "result",
        output=context.variable_flow_per_source_output, config_path=config)


@when("variable flow is asked to use the input facts database as output")
def alien_output(context):
    context.variable_flow_facts_before = context.facts_database_path.read_bytes()
    run(context, "--function", "variable_flow::root", "--variable", "seed",
        output=context.facts_database_path)
