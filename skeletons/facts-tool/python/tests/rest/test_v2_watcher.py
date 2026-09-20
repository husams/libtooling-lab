import json

import httpx

from facts_tool.rest import Client, WatcherSettings


def test_watcher_partial_patch_and_full_put_use_distinct_http_methods():
    requests = []
    settings = {
        "enabled": True,
        "debounce_ms": 500,
        "exclude_repositories": [],
        "exclude_clones": [],
        "exclude_directories": [],
        "exclude_patterns": [],
    }

    def handle(request):
        requests.append(request)
        return httpx.Response(200, json=settings)

    with Client("http://test", transport=httpx.MockTransport(handle)) as api:
        api.watcher.update_settings(debounce_ms=750)
        api.watcher.replace_settings(WatcherSettings(**settings))
        assert api.watcher.settings().enabled is True
    assert [r.method for r in requests] == ["PATCH", "PUT", "GET"]
    assert json.loads(requests[0].content) == {"debounce_ms": 750}
    assert json.loads(requests[1].content) == settings
    assert requests[2].content == b""
