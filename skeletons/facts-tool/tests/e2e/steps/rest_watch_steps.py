"""Real inotify events must publish fresh symbols through the REST interface."""
import subprocess

from pytest_bdd import parsers, then, when
from support.rest_http import eventually
from support.rest_project import wait_cycle, watch_status, write_commands


@when(parsers.parse('I save an added function using "{kind}"'))
def edit(rest_server, kind):
    rest_server.previous_cycle = watch_status(rest_server)["cycles"]
    rest_server.added_name = "fresh_header" if kind == "header edit" else "fresh_source"
    path = rest_server.header if kind == "header edit" else rest_server.source
    updated = path.read_text() + f"\ninline int {rest_server.added_name}() {{ return 71; }}\n"
    if kind == "atomic replacement":
        replacement = path.with_suffix(".tmp")
        replacement.write_text(updated)
        replacement.replace(path)
    else:
        path.write_text(updated)


@then("inotify automatically reimports and reindexes the new function")
def refreshed(rest_server):
    wait_cycle(rest_server, rest_server.previous_cycle)
    symbols = rest_server.api.run([], "symbol/list")["stdout"]
    assert rest_server.added_name in symbols, symbols
    matcher = f'functionDecl(hasName("{rest_server.added_name}")).bind("symbol")'
    match = rest_server.api.run(["--matcher", matcher, str(rest_server.source)], "match")
    assert rest_server.added_name in match["stdout"] + match["stderr"], match


@when("I add a nested translation unit to the compilation database")
def nested(rest_server, pytestconfig):
    rest_server.previous_cycle = watch_status(rest_server)["cycles"]
    directory = rest_server.project / "new" / "nested"
    directory.mkdir(parents=True)
    rest_server.nested_source = directory / "new.cpp"
    rest_server.nested_source.write_text("int nested_initial() { return 1; }\n")
    write_commands(rest_server.project, pytestconfig.getoption("--compiler"),
                   [rest_server.source, rest_server.nested_source])
    wait_cycle(rest_server, rest_server.previous_cycle)
    assert "nested_initial" in rest_server.api.run([], "symbol/list")["stdout"]


@when("I edit that newly watched translation unit")
def edit_nested(rest_server):
    rest_server.previous_cycle = watch_status(rest_server)["cycles"]
    rest_server.nested_source.write_text("int nested_changed() { return 2; }\n")


@then("the nested function is updated without restarting the server")
def nested_updated(rest_server):
    wait_cycle(rest_server, rest_server.previous_cycle)
    symbols = rest_server.api.run([], "symbol/list")["stdout"]
    assert "nested_changed" in symbols
    match = rest_server.api.run(["--matcher", 'functionDecl().bind("symbol")',
                                 str(rest_server.nested_source)], "match")
    current = match["stdout"] + match["stderr"]
    assert "nested_changed" in current and "nested_initial" not in current


@when("I corrupt the watched compilation database")
def corrupt(rest_server):
    rest_server.previous_failures = watch_status(rest_server)["failures"]
    (rest_server.project / "compile_commands.json").write_text("invalid JSON")


@then("watch status reports the reimport failure while HTTP stays available")
def watch_failure(rest_server):
    def failed():
        state = watch_status(rest_server)
        return state if state["failures"] > rest_server.previous_failures else None
    state = eventually(failed)
    assert state["last_error"]
    assert rest_server.api.request("GET", "/health")[0] == 200


@then("the project Git commit is unchanged")
def same_commit(rest_server):
    current = subprocess.check_output(["git", "-C", str(rest_server.project), "rev-parse", "HEAD"])
    assert current == rest_server.commit
