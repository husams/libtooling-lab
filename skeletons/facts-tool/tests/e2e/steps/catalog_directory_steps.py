from __future__ import annotations

from pytest_bdd import then
from support.catalog import Catalog
from support.database import require


@then("only the selected directory and its file rows are removed")
def directory_removed(catalog: Catalog) -> None:
    after = catalog.snapshot()
    require(after['component'] == catalog.before['component'], "directory removal changed components")
    require(after['directory'] == [row for row in catalog.before['directory'] if row[0] != catalog.deep_id],
            "directory removal affected sibling directories")
    require(after['file'] == [row for row in catalog.before['file'] if row[1] != catalog.deep_id],
            "directory removal affected sibling files")
