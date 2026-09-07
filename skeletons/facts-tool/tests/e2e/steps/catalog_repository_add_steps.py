from __future__ import annotations

import sqlite3
from pytest_bdd import then, when
from support.catalog import Catalog
from support.database import require


def _vendor(catalog: Catalog) -> list[tuple]:
    return catalog.rows(
        "SELECT r.kind,r.remote_url,c.path,c.label FROM repository r "
        "LEFT JOIN clone c ON c.id=r.active_clone_id WHERE r.name='vendor'")


@then("the vendor repository is registered with its checkout active")
def vendor_registered(catalog: Catalog) -> None:
    require(_vendor(catalog) == [("repo", "https://example.invalid/vendor.git",
                                  str(catalog.external), "main")],
            f"repository registration did not persist name, remote, and active clone: "
            f"{_vendor(catalog)}")


@then("the vendor repository has no label and no remote")
def vendor_defaults(catalog: Catalog) -> None:
    require(_vendor(catalog) == [("repo", None, str(catalog.external), None)],
            f"omitted label or remote was not stored as NULL: {_vendor(catalog)}")


@then("the catalog output shows the vendor checkout as the active clone")
def vendor_shown(catalog: Catalog) -> None:
    active = [line for line in catalog.stdout.splitlines()
              if line.startswith("*") and line.endswith(str(catalog.external))]
    require(len(active) == 1, f"show did not list the checkout as active:\n{catalog.stdout}")


@then("only the vendor repository and its clone were added")
def only_vendor_added(catalog: Catalog) -> None:
    after = catalog.snapshot()
    for table in ("component", "directory", "file"):
        require(after[table] == catalog.before[table], f"{table} rows changed")
    added = {table: [row for row in after[table] if row not in catalog.before[table]]
             for table in ("repository", "clone")}
    require(all(row in after[table] for table in ("repository", "clone")
                for row in catalog.before[table]), "existing repository rows changed")
    require(len(added["repository"]) == 1 and added["repository"][0][1] == "vendor",
            f"expected exactly one new repository: {added}")
    require(len(added["clone"]) == 1 and added["clone"][0][1] == added["repository"][0][0],
            f"expected exactly one clone owned by the new repository: {added}")


@then("the vendor-lib component belongs to the vendor repository")
def vendor_component(catalog: Catalog) -> None:
    rows = catalog.rows(
        "SELECT c.path,c.kind,r.name,cl.path FROM component c "
        "JOIN repository r ON r.id=c.repository_id "
        "JOIN clone cl ON cl.id=r.active_clone_id WHERE c.name='vendor-lib'")
    require(rows == [(".", "repo", "vendor", str(catalog.external))],
            f"component was not attached to the new repository's active clone: {rows}")


@when("I register the vendor repository against a new configuration")
def register_fresh(catalog: Catalog) -> None:
    context = catalog.context
    context.files_database = context.run_root_path / "fresh-project.sqlite"
    require(not context.files_database_path.exists(), "fresh configuration already exists")
    catalog.run("repo add vendor {external-root} --label main")
    require(context.last_returncode == 0, context.last_output)


@then("the new configuration holds the vendor repository with its active clone and no files")
def fresh_configuration(catalog: Catalog) -> None:
    uri = catalog.context.files_database_path.as_uri() + "?mode=ro"
    with sqlite3.connect(uri, uri=True) as connection:
        repositories = connection.execute(
            "SELECT r.name,c.path,c.label FROM repository r "
            "JOIN clone c ON c.id=r.active_clone_id").fetchall()
        # A fresh schema seeds one placeholder external component; the new
        # repository must own no component and no file yet.
        counts = connection.execute(
            "SELECT (SELECT count(*) FROM repository),(SELECT count(*) FROM clone),"
            "(SELECT count(*) FROM component WHERE repository_id IS NOT NULL),"
            "(SELECT count(*) FROM file)").fetchall()
    require(repositories == [("vendor", str(catalog.external), "main")],
            f"fresh configuration lacks the registered repository: {repositories}")
    require(counts == [(1, 1, 0, 0)], f"fresh configuration holds unexpected rows: {counts}")
