"""Real repository setup through native commands, before domain REST requests."""
import pytest
from pytest_bdd import given, when

from domain_http import index_ready
from domain_project import DomainProject


@pytest.fixture
def domain_catalog(rest_server, pytestconfig):
    return DomainProject(rest_server.executable, rest_server.root / "catalog",
                         pytestconfig.getoption("--compiler"), rest_server.environment)


@given("two real repositories with separate extracted fact databases")
def repositories(domain_catalog):
    domain_catalog.add("alpha")
    domain_catalog.add("beta")
    assert domain_catalog.facts["alpha"] != domain_catalog.facts["beta"]
    assert all(path.is_file() for path in domain_catalog.facts.values())


@given("the repository-aware server is running")
@when("the repository-aware server starts for the first time")
def start(domain_catalog, rest_server):
    rest_server.start(domain_catalog.options())
    domain_catalog.index = index_ready(rest_server.api)


@given("alpha has a registered inactive clone with different source content")
def inactive(domain_catalog):
    domain_catalog.inactive = domain_catalog.inactive_clone()
    source = domain_catalog.inactive
    source.write_text(source.read_text() + "namespace alpha { class SecondaryOnly {}; }\n")
    domain_catalog.active_before = domain_catalog.rows(
        "SELECT name,active_clone_id FROM repository ORDER BY id")
