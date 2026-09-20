"""Database contention cannot occupy the HTTP event loop or delay acceptance."""
import sqlite3
import time
from concurrent.futures import ThreadPoolExecutor

from domain_http import completed, index_ready, submit, symbols


def test_initial_index_does_not_block_health(domain_project, server_factory):
    lock = sqlite3.connect(domain_project.database)
    try:
        lock.execute("BEGIN EXCLUSIVE")
        server = server_factory(*domain_project.options())
        started = time.monotonic()
        status, state = server.api.request("GET", "/v1/index")
        assert status == 200 and state["state"] in {"queued", "running"}, state
        assert server.api.request("GET", "/health")[0] == 200
        assert server.api.request("GET", "/v1/symbols?qualified_name=alpha%3A%3Aanswer")[0] == 503
        assert server.api.request("POST", "/v1/extractions",
                                  {"file": {"path": "src/main.cpp", "repo": "alpha"}})[0] == 503
        assert time.monotonic() - started < 2
    finally:
        lock.rollback()
        lock.close()
    index_ready(server.api)
    assert symbols(server.api, "alpha::answer")["items"]


def test_initialized_operation_acceptance_is_async(domain_server, domain_project):
    with sqlite3.connect(domain_project.database) as lock:
        lock.execute("BEGIN EXCLUSIVE")
        try:
            started = time.monotonic()
            job = submit(domain_server.api, "/v1/extractions",
                         {"path": "src/main.cpp", "repo": "alpha"})
            assert domain_server.api.request("GET", "/health")[0] == 200
            assert domain_server.api.request("GET", "/v1/index")[0] == 200
            assert time.monotonic() - started < 2
        finally:
            lock.rollback()
    completed(domain_server.api, job)
    index_ready(domain_server.api)


def test_symbol_query_waits_off_the_http_event_loop(domain_server, domain_project):
    lock = sqlite3.connect(domain_project.database)
    with ThreadPoolExecutor(max_workers=1) as pool:
        try:
            lock.execute("BEGIN EXCLUSIVE")
            pending = pool.submit(symbols, domain_server.api, "alpha::answer")
            started = time.monotonic()
            assert domain_server.api.request("GET", "/health")[0] == 200
            assert domain_server.api.request("GET", "/v1/index")[0] == 200
            assert time.monotonic() - started < 2
        finally:
            lock.rollback()
            lock.close()
        assert pending.result(timeout=10)["items"]
