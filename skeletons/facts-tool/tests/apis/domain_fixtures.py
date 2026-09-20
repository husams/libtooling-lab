"""Isolated real repositories for native resource API integration tests."""
import pytest

from domain_http import index_ready
from domain_project import DomainProject
from server import isolated_environment


@pytest.fixture
def domain_project(executable, compiler, tmp_path):
    root = tmp_path / "catalog"
    project = DomainProject(executable, root, compiler, isolated_environment(root))
    project.add("alpha")
    project.add("beta")
    return project


@pytest.fixture
def domain_server(domain_project, server_factory):
    server = server_factory(*domain_project.options())
    index_ready(server.api)
    return server
