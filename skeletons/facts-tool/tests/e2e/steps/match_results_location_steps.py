from __future__ import annotations

from pathlib import Path

from pytest_bdd import given, parsers, then, when
from support.match_results import MATCHERS, import_sources, invoke


@given("a matcher fixture with macro remapping and implicit allocation")
def remapped(context):
    context.prepare()
    source = context.run_root_path / "remapped.cpp"
    source.write_text('#define DEFINE(name) int name() { return 1; }\n'
                      '#line 700 "logical.cpp"\nDEFINE(remapped)\n'
                      'void allocation() { auto *p = new int; delete p; }\n')
    context.match_sources = [source]
    import_sources(context)


@when("the macro symbol is returned as JSON")
def macro_match(context):
    result = invoke(context,
                    'functionDecl(hasName("remapped"))'
                    '.bind("symbol")')
    assert result.returncode == 0, result.stdout + result.stderr


@then("remapped coordinates identify the physical expansion")
def remapped_locations(context):
    from facts_tool import MatchResults

    results = MatchResults.from_json(context.match_completed.stdout)
    symbols = [row.bindings["symbol"] for row in results]
    remapped = next(symbol for symbol in symbols if symbol.name == "remapped")
    assert Path(remapped.location.path).resolve() == context.match_sources[0].resolve()
    assert (remapped.location.line, remapped.location.column) == (3, 1)
    text = context.match_sources[0].read_text()
    assert remapped.location.offset == text.index("DEFINE(remapped)")


@when("an unsupported implicit symbol is matched")
def implicit_match(context):
    invoke(context, 'functionDecl(hasName("operator new")).bind("symbol")')


@then("the location failure is explicit and publishes no JSON")
def unsupported_location(context):
    assert context.match_completed.returncode != 0
    assert "invalid source location" in context.match_completed.stderr
    assert context.match_completed.stdout == ""


@when(parsers.parse('a located "{contract}" matcher runs in text mode'))
def occurrence_text(context, contract):
    matcher = MATCHERS["call"]
    relation = False
    if contract == "uses":
        matcher = ('functionDecl(hasAnyName("result_alpha","result_beta"),'
                   'forEachDescendant(declRefExpr(to(functionDecl('
                   'hasName("shared_match")).bind("target"))).bind("site")))'
                   '.bind("source")')
        relation = "Uses"
    result = invoke(context, matcher, text=True, relation=relation)
    assert result.returncode == 0, result.stdout + result.stderr


@then("text output identifies the occurrence in each source")
def occurrence_locations(context):
    for source in context.match_sources:
        line = source.read_text().splitlines()[1]
        column = line.index("shared_match(") + 1
        assert f"{source.resolve()}:2:{column}" in context.match_completed.stdout
