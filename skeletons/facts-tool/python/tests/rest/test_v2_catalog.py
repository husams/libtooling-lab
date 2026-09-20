import json

import httpx

from facts_tool.rest.v2.catalog_models import CompilationCommand
from facts_tool.rest.v2.files import Files
from facts_tool.rest.v2.repositories import Repositories
from facts_tool.rest.v2.symbols import Symbols

from .v2_helpers import REPOSITORY, SYMBOL


def test_symbol_query_is_lazy_prefix_encoded_get_and_follows_cursor():
    requests = []

    def handle(request):
        requests.append(request)
        cursor = None if len(requests) == 2 else "opaque+/=&cursor"
        return httpx.Response(200, json={"items": [SYMBOL], "next_cursor": cursor})

    with httpx.Client(
        base_url="http://test", transport=httpx.MockTransport(handle)
    ) as http:
        symbols = Symbols(http).find("example::_Widget%", limit=1)
        assert requests == []
        iterator = iter(symbols)
        assert next(iterator).definition.file_id == "1"
        assert len(requests) == 1
        assert next(iterator).symbol_id == "sym/1"
        assert list(iterator) == []
    assert requests[0].method == "GET" and requests[0].content == b""
    assert requests[0].url.params["qualified_name"] == "example::_Widget%"
    assert requests[0].url.params["match"] == "prefix"
    assert requests[1].url.params["cursor"] == "opaque+/=&cursor"


def test_repository_patch_preserves_omitted_fields_and_explicit_null():
    requests = []

    def handle(request):
        requests.append(request)
        return httpx.Response(200, json=REPOSITORY)

    with httpx.Client(
        base_url="http://test", transport=httpx.MockTransport(handle)
    ) as http:
        repos = Repositories(http)
        assert repos.update("repo-1", active_clone_id="clone-2").name == "example"
        repos.update("repo-1", remote_url=None)
    assert all(r.method == "PATCH" for r in requests)
    assert json.loads(requests[0].content) == {"active_clone_id": "clone-2"}
    assert json.loads(requests[1].content) == {"remote_url": None}


def test_file_update_puts_compiler_settings_inside_file_and_delete_accepts_204():
    requests = []
    file = {
        "id": "1",
        "path": "widget.cpp",
        "name": "widget.cpp",
        "directory_id": "2",
        "component_id": "3",
        "component": "core",
        "repository_id": "4",
        "indexed": False,
        "compilation_command": {
            "driver": "clang++",
            "working_directory": "/src",
            "arguments": ["-g"],
        },
    }

    def handle(request):
        requests.append(request)
        return (
            httpx.Response(204)
            if request.method == "DELETE"
            else httpx.Response(200, json=file)
        )

    with httpx.Client(
        base_url="http://test", transport=httpx.MockTransport(handle)
    ) as http:
        files = Files(http)
        assert files.update(
            "1", compilation_command=CompilationCommand("clang++", "/src", ["-g"])
        ).compilation_command.arguments == ("-g",)
        files.delete("1")
    assert requests[0].url.path == "/api/v2/files/1"
    assert json.loads(requests[0].content) == {
        "compilation_command": file["compilation_command"]
    }
    assert requests[1].method == "DELETE"
