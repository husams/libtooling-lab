from __future__ import annotations

from pathlib import Path

from pytest_bdd import given, parsers, then, when
from support.match_results import MATCHERS, import_sources, invoke


@given("an isolated two-source match results fixture")
def fixture(context):
    context.prepare()
    root = context.run_root_path
    context.match_header = root / "common.hpp"
    context.match_header.write_text(
        "#pragma once\nstruct MatchBase {};\n"
        "struct MatchRecord : MatchBase { int field; };\n"
        "inline int shared_match(int value) { return value + 1; }\n")
    context.match_sources = [root / "alpha.cpp", root / "beta.cpp"]
    for path, name in zip(context.match_sources, ["result_alpha", "result_beta"]):
        path.write_text('#include "common.hpp"\n'
                        f"int {name}(int value) {{ return shared_match(value); }}\n")
    import_sources(context)


@when(parsers.parse('a structured matcher runs for "{contract}"'))
@given(parsers.parse('a structured matcher runs for "{contract}"'))
def structured(context, contract):
    result = invoke(context, MATCHERS[contract], relation=contract == "relation")
    assert result.returncode == 0, result.stdout + result.stderr


@then(parsers.parse('the SDK reads located "{contract}" bindings for both translation units'))
def located(context, contract):
    from facts_tool import MatchResults, load_match_results

    results = MatchResults.from_json(context.match_completed.stdout)
    assert results.complete and results.facts_committed and results.index_committed
    assert len(results) == 2
    assert {Path(row.translation_unit).resolve() for row in results} == {
        path.resolve() for path in context.match_sources}
    expected = {"symbol": {"symbol"}, "call": {"call", "callee"},
                "relation": {"source", "target"}, "expression": {"expression"}}
    for row in results:
        assert set(row.bindings) == expected[contract]
        for binding in row.bindings.values():
            assert binding.location.line > 0 and binding.location.column > 0
            assert binding.location.offset >= 0 and binding.range.size > 0
            assert Path(binding.location.path).exists()
    if contract == "symbol":
        assert len({row.bindings["symbol"].usr for row in results}) == 1
        assert all(Path(row.bindings["symbol"].location.path).resolve() ==
                   context.match_header.resolve() for row in results)
    saved = context.run_root_path / "match-results.json"
    saved.write_text(context.match_completed.stdout)
    assert load_match_results(saved).to_dict() == results.to_dict()


@then("matching reports each translation unit once")
def single_pass(context):
    assert context.match_completed.stderr.count("Processing file ") == 2


@when("a located symbol matcher runs in text mode")
def text_match(context):
    result = invoke(context, MATCHERS["symbol"], text=True)
    assert result.returncode == 0, result.stdout + result.stderr


@then("symbol output contains its header path and coordinates")
def text_location(context):
    text = context.match_completed.stdout
    assert "symbol kind=function name=shared_match" in text
    assert f"{context.match_header.resolve()}:4:12" in text


@then("the SDK reads an empty successful match collection")
def empty(context):
    from facts_tool import MatchResults

    result = MatchResults.from_json(context.match_completed.stdout)
    assert result.complete and len(result) == 0
    assert len(result.sources) == 2
