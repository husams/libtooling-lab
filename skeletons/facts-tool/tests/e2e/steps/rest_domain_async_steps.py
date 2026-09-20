"""Real SQLite locks prove that domain work does not occupy HTTP workers."""
import sqlite3
import time

from pytest_bdd import then, when

from domain_http import completed, index_ready, submit, symbols


@when("I start the repository-aware server while the catalog is locked")
def locked_start(domain_catalog, rest_server):
    with sqlite3.connect(domain_catalog.database) as lock:
        lock.execute("BEGIN EXCLUSIVE")
        try:
            rest_server.start(domain_catalog.options())
            started = time.monotonic()
            domain_catalog.health = rest_server.api.request("GET", "/health")
            domain_catalog.index_response = rest_server.api.request("GET", "/v1/index")
            domain_catalog.early_lookup = rest_server.api.request(
                "GET", "/v1/symbols?qualified_name=alpha%3A%3Aanswer")
            domain_catalog.response_time = time.monotonic() - started
        finally:
            lock.rollback()


@then("health and index status respond before the lock is released")
def responds(domain_catalog):
    assert domain_catalog.health[0] == 200
    status, index = domain_catalog.index_response
    assert status == 200 and index["state"] in {"queued", "running"}, index
    assert domain_catalog.early_lookup[0] == 503
    assert domain_catalog.response_time < 2


@then("global indexing finishes after the lock is released")
def indexed(rest_server):
    assert index_ready(rest_server.api)["symbols"] > 0
    assert symbols(rest_server.api, "alpha::answer")["items"]


@when("I submit typed extraction while the catalog is locked")
def locked_extraction(domain_catalog, rest_server):
    with sqlite3.connect(domain_catalog.database) as lock:
        lock.execute("BEGIN EXCLUSIVE")
        try:
            started = time.monotonic()
            domain_catalog.job = submit(rest_server.api, "/v1/extractions",
                                        {"path": "src/main.cpp", "repo": "alpha"})
            domain_catalog.health = rest_server.api.request("GET", "/health")
            domain_catalog.response_time = time.monotonic() - started
        finally:
            lock.rollback()


@then("the extraction is accepted and health responds before the lock is released")
def accepted(domain_catalog):
    assert domain_catalog.job["id"] and domain_catalog.health[0] == 200
    assert domain_catalog.response_time < 2


@then("the accepted extraction and global indexing finish after the lock is released")
def finishes(domain_catalog, rest_server):
    assert completed(rest_server.api, domain_catalog.job)["operation"] == "extract"
    assert index_ready(rest_server.api)["symbols"] > 0
    assert symbols(rest_server.api, "alpha::answer")["items"]
