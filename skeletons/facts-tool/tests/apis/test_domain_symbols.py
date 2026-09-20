"""Startup builds a searchable global index from real existing fact databases."""
import sqlite3

from domain_http import index_ready, symbols


def test_startup_indexes_distinct_fact_databases(domain_server, domain_project):
    state = index_ready(domain_server.api)
    assert state["files"] >= 2 and state["symbols"] >= 8, state
    assert state["error"] is None and isinstance(state["updated_at"], int), state
    for name in ("alpha", "beta"):
        item, = symbols(domain_server.api, f"{name}::answer")["items"]
        assert item["qualified_name"] == f"{name}::answer"
        assert item["kind"] == "function" and item["usr"], item
        assert item["repo"] == name and item["clone"] == f"primary-{name}", item
        assert item["component"] == f"{name}-core", item
        assert item["path"] == str(domain_project.sources[name]), item
        assert item["file_id"] in {row[0] for row in domain_project.rows("SELECT id FROM file")}


def test_exact_qualified_names_and_typed_filters(domain_server):
    api = domain_server.api
    item, = symbols(api, "alpha::answer")["items"]
    for filters in ({"kind": "function"}, {"usr": item["usr"]}, {"repo": "alpha"},
                    {"component": "alpha-core"}, {"kind": "function", "repo": "alpha"}):
        assert symbols(api, "alpha::answer", **filters)["items"] == [item]
    for name, filters in (("answer", {}), ("alpha::ans", {}), ("alpha::answer", {"repo": "beta"}),
                          ("alpha::answer", {"kind": "class"}),
                          ("alpha::answer", {"usr": "c:@F@not_registered#"}),
                          ("alpha::answer", {"component": "beta-core"})):
        assert symbols(api, name, **filters)["items"] == []
    widget, = symbols(api, "alpha::Widget", kind="class")["items"]
    assert widget["qualified_name"] == "alpha::Widget"


def test_symbols_use_definition_file_ids_for_headers(domain_server, domain_project):
    item, = symbols(domain_server.api, "alpha::helper")["items"]
    assert item["path"] == str(domain_project.sources["alpha"].parent / "common.hpp")
    assert domain_project.rows("SELECT name FROM file WHERE id=?", (item["file_id"],)) == [
        ("common.hpp",)]


def test_global_search_has_cursor_pagination(domain_server):
    first = symbols(domain_server.api, "shared::same", limit=1)
    assert len(first["items"]) == 1 and first["next_cursor"], first
    second = symbols(domain_server.api, "shared::same", limit=1, cursor=first["next_cursor"])
    assert len(second["items"]) == 1 and second["next_cursor"] is None, second
    assert first["items"][0]["file_id"] != second["items"][0]["file_id"]
    assert {first["items"][0]["repo"], second["items"][0]["repo"]} == {"alpha", "beta"}


def test_index_is_persisted_in_the_project_database(domain_server, domain_project):
    index_ready(domain_server.api)
    with sqlite3.connect(domain_project.database.as_uri() + "?mode=ro", uri=True) as db:
        columns = {row[1] for row in db.execute("PRAGMA table_info(global_symbol_index)")}
        assert {"qualified_name", "kind", "usr", "file_id"}.issubset(columns), columns
        rows = db.execute("SELECT qualified_name FROM global_symbol_index WHERE kind='function'").fetchall()
        assert {("alpha::answer",), ("beta::answer",)}.issubset(set(rows))
