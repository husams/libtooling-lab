import asyncio
from contextlib import ExitStack

import pytest
from pytest_bdd import given, parsers, scenarios, then, when

from facts_tool.rest import AsyncClient, Client, DomainJob, FileSelector

scenarios("resources.feature")


@pytest.fixture
def resource_clients(domain_rest):
    url, _ = domain_rest
    with ExitStack() as stack:
        def create(kind):
            if kind == "sync":
                api = stack.enter_context(Client(url, token="integration-token"))
                return lambda name, *a, **kw: getattr(api, name)(*a, **kw)
            runner = stack.enter_context(asyncio.Runner())
            api = AsyncClient(url, token="integration-token")
            runner.run(api.__aenter__())
            stack.callback(lambda: runner.run(api.aclose()))
            return lambda name, *a, **kw: runner.run(getattr(api, name)(*a, **kw))
        yield create


@given(parsers.parse("an indexed project and a {kind} resource SDK client"),
       target_fixture="resource_call")
def resource_client(resource_clients, kind):
    return resource_clients(kind)


@when("I search for a fully qualified function name")
def find_symbol(resource_call, world):
    world["page"] = resource_call("find_symbols", "alpha::answer", kind="function")


@then("the SDK returns its typed repository and defining file identity")
def symbol_identity(world):
    assert len(world["page"].items) == 1
    symbol = world["page"].items[0]
    assert symbol.qualified_name == "alpha::answer" and symbol.kind == "function"
    assert symbol.repo == "alpha" and symbol.file_id > 0 and symbol.is_definition


@when("I match that file with a Clang DSL expression")
def match_file(resource_call, world):
    selector = FileSelector("main.cpp", repo="alpha", component="alpha-core")
    world["job"] = resource_call("match", selector,
                                 'functionDecl(hasName("alpha::answer")).bind("symbol")')


@then("the SDK receives a structured completed operation result")
def domain_result(resource_call, world):
    job = resource_call("wait", world["job"].id, timeout=20)
    assert isinstance(job, DomainJob) and job.succeeded and job.error is None
    assert job.result["match_count"] >= 1
    assert job.result["matches"][0]["bindings"]["symbol"]["name"] == "alpha::answer"
