import sqlite3
from pytest_bdd import then
from support.callable_snapshot import snapshot, semantics
from support.database import query


@then("callable facts survive reruns and reversed translation units")
def stable_callables(context):
    before = snapshot(context.facts_database_path)
    context.run_tool()
    assert snapshot(context.facts_database_path) == before
    context.sources = tuple(reversed(context.sources))
    context.run_tool()
    assert snapshot(context.facts_database_path) == before
    expected = semantics(context.facts_database_path)
    context.facts_database = context.run_root_path / "reversed.sqlite"
    context.run_tool()
    assert semantics(context.facts_database_path) == expected


@then("callable facts survive migration and re-extraction")
def migrated_callables(context):
    before = snapshot(context.facts_database_path)
    with sqlite3.connect(context.facts_database_path) as db:
        db.execute("ALTER TABLE symbol DROP COLUMN is_volatile")
        db.execute("PRAGMA user_version=8")
        db.execute("UPDATE symbol SET is_const=0,ref_qualifier='none',is_noexcept=0")
    context.run_tool()
    assert snapshot(context.facts_database_path) == before
    assert query(context.facts_database_path, "PRAGMA user_version") == [(9,)]
    context.run_tool()
    assert snapshot(context.facts_database_path) == before


def output(context, action, name=None):
    command = [str(context.facts_tool), "symbol", action]
    if name:
        command.append(name)
    result = context._run(command + ["--facts", str(context.facts_database_path),
                                    "--conf", str(context.files_database_path)])
    assert result.returncode == 0, context.last_output
    return result.stdout


@then("callable list and detail render stored qualifiers and parameter defaults")
def render_callables(context):
    before = snapshot(context.facts_database_path)
    listing = output(context, "list")
    detail = output(context, "show", "qualifiers::Cv::split")
    expected = "qualifiers::Cv::split(const int& value = 7) const volatile & noexcept"
    assert expected in listing, listing
    assert expected in detail, detail
    for name in ("qualifiers::conditional", "qualifiers::Specs::conditional",
                 "qualifiers::Special::operator bool", "qualifiers::Special::Special"):
        rendered = output(context, "show", name)
        assert "noexcept" in rendered, rendered
        assert "(" in rendered.splitlines()[0], rendered
    for name in ("qualifiers::constant", "qualifiers::immediate", "qualifiers::variadic"):
        rendered = output(context, "show", name)
        assert any(flag in rendered for flag in ("constexpr", "consteval", "variadic"))
    assert snapshot(context.facts_database_path) == before
