"""Render HTTPX clients and endpoint constants from the OpenAPI contract."""

from pathlib import Path

from .python_domain import resource_clients
from .python_methods import methods
from .python_routes import routes


def render_python(spec: dict) -> dict[str, str]:
    package = "python/src/facts_tool/rest/"
    templates = Path(__file__).parent / "templates"
    clients = {
        package + name: (templates / (name + ".in")).read_text().replace(
            "    # @operations", methods(spec, asynchronous=name.startswith("async"))
        )
        for name in ("client.py", "async_client.py")
    }
    return clients | resource_clients(spec) | {
        package + "generated/__init__.py": '"""Generated OpenAPI endpoint metadata."""\n',
        package + "generated/routes.py": routes(spec),
    }
