"""Catalog setup through supported CLI commands before native server startup."""
import sys

import pytest
from pytest_bdd import given, parsers
from support.watch_catalog import WatchCatalog


@pytest.fixture
def watch_catalog(rest_server, pytestconfig):
    assert sys.platform == "linux", "watch BDD requires the Linux inotify test environment"
    return WatchCatalog(rest_server, pytestconfig.getoption("--compiler"))


@given("two indexed repositories registered with custom names and clone labels")
def two_repositories(watch_catalog):
    watch_catalog.create("alpha")
    watch_catalog.create("beta")
    watch_catalog.before_identities = watch_catalog.identities()


@given("the catalog server starts without a monitored directory list")
def start(watch_catalog):
    watch_catalog.start()


@given("a registered plain source directory with a Git ignore file")
def plain_directory(watch_catalog):
    root = watch_catalog.create("alpha", ["main.cpp", "private.cpp"], git=False)
    (root / ".gitignore").write_text("private.cpp\n")
    watch_catalog.blocked = watch_catalog.sources["alpha"][1:]
    watch_catalog.before_blocked = watch_catalog.snapshot(watch_catalog.blocked)
    watch_catalog.start()


@given(parsers.parse('the catalog server excludes beta by "{selector}"'))
def exclude_repository(watch_catalog, selector):
    choices = {
        "repository": ("exclude_repositories", "beta"),
        "clone label": ("exclude_clones", "primary-beta"),
        "qualified clone": ("exclude_clones", "beta:primary-beta"),
        "clone path": ("exclude_clones", str(watch_catalog.roots["beta"])),
    }
    key, value = choices[selector]
    watch_catalog.policy[key] = [value]
    watch_catalog.start(["alpha"])


@given(parsers.parse('an indexed repository with YAML "{policy}" exclusions'))
def excluded_paths(watch_catalog, policy):
    paths = ["main.cpp", "generated/blocked.cpp", "nested/secret.cpp"]
    root = watch_catalog.create("alpha", paths)
    directory = str(root / "generated") if policy == "absolute directory" else "generated"
    watch_catalog.policy = {"exclude_directories": [directory],
                            "exclude_patterns": ["**/secret.cpp"]}
    watch_catalog.blocked = watch_catalog.sources["alpha"][1:]
    watch_catalog.before_blocked = watch_catalog.snapshot(watch_catalog.blocked)
    watch_catalog.start()
