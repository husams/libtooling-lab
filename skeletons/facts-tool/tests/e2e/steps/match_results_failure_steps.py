from __future__ import annotations

from pytest_bdd import then, when
from support.match_results import MATCHERS, invoke, symbol_snapshot


@when("a later translation unit fails after an earlier new symbol matches")
def failed_later(context):
    context.match_prior = symbol_snapshot(context, "shared_match")
    with context.match_sources[0].open("a") as out:
        out.write("int fresh_only() { return 42; }\n")
    with context.match_sources[1].open("a") as out:
        out.write("int broken( {\n")
    invoke(context, 'functionDecl(hasName("fresh_only")).bind("symbol")')


@then("matching fails without publishing JSON or the earlier new symbol")
def rollback(context):
    result = context.match_completed
    assert result.returncode != 0
    assert result.stdout == ""
    assert symbol_snapshot(context, "fresh_only") is None
    assert symbol_snapshot(context, "shared_match") == context.match_prior


@when("a nonmatching source includes a new unregistered header")
def unknown_include(context):
    context.match_prior = symbol_snapshot(context, "shared_match")
    (context.run_root_path / "new.hpp").write_text("// newly added include\n")
    with context.match_sources[1].open("a") as out:
        out.write('#include "new.hpp"\n')
    invoke(context, MATCHERS["empty"])


@then("matching fails with incomplete registration and preserves prior symbols")
def incomplete(context):
    result = context.match_completed
    assert result.returncode != 0
    assert "project configuration is incomplete" in result.stderr
    assert "new.hpp" in result.stderr
    assert result.stdout == ""
    assert symbol_snapshot(context, "shared_match") == context.match_prior
