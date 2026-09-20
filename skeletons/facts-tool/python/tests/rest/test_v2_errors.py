import httpx
import pytest

from facts_tool.rest.v2.files import Files
from facts_tool.rest.v2.wire import ResourceNotFound


def test_missing_resource_is_specific_exception():
    transport = httpx.MockTransport(
        lambda _: httpx.Response(
            404, json={"error": {"code": "not_found", "message": "Missing file"}}
        )
    )
    with (
        httpx.Client(base_url="http://test", transport=transport) as http,
        pytest.raises(ResourceNotFound, match="Missing file"),
    ):
        Files(http).get("missing")
