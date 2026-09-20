"""Global lookup observes real extracted definitions and persisted identities."""
from pytest_bdd import then

from domain_http import symbols


@then("fully qualified symbol searches find both repositories and defining files")
def symbols_across_repositories(domain_catalog, rest_server):
    for name in ("alpha", "beta"):
        item, = symbols(rest_server.api, f"{name}::answer")["items"]
        assert item["repo"] == name and item["path"] == str(domain_catalog.sources[name])
        assert item["clone"] == f"primary-{name}" and item["component"] == f"{name}-core"
        assert item["kind"] == "function" and item["usr"] and item["file_id"] > 0
        helper, = symbols(rest_server.api, f"{name}::helper")["items"]
        assert helper["path"] == str(domain_catalog.sources[name].parent / "common.hpp")
        assert helper["file_id"] != item["file_id"]
    assert domain_catalog.index["symbols"] >= 8 and domain_catalog.index["files"] >= 2


@then("symbol kind, USR, repository, and component filters are exact")
def exact_filters(rest_server):
    api = rest_server.api
    item, = symbols(api, "alpha::answer")["items"]
    assert symbols(api, "alpha::answer", kind="function", usr=item["usr"], repo="alpha",
                   component="alpha-core")["items"] == [item]
    assert not symbols(api, "answer")["items"]
    assert not symbols(api, "alpha::answer", kind="class")["items"]
    assert not symbols(api, "alpha::answer", repo="beta")["items"]
    assert not symbols(api, "alpha::answer", component="beta-core")["items"]
    assert symbols(api, "alpha::Widget", kind="class")["items"]


@then("the global symbol index is stored in the project database")
def persisted(domain_catalog):
    rows = domain_catalog.rows(
        "SELECT qualified_name,kind,usr,file_id FROM global_symbol_index "
        "WHERE qualified_name IN ('alpha::answer','beta::answer')")
    assert {row[0] for row in rows} == {"alpha::answer", "beta::answer"}
    assert all(kind == "function" and usr and file_id > 0 for _, kind, usr, file_id in rows)
