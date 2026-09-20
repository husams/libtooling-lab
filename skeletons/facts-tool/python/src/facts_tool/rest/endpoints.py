"""Format generated routes with each path parameter encoded as one segment."""

from urllib.parse import quote

from .generated.routes import ROUTES


def endpoint(operation: str, **parameters: str) -> tuple[str, str]:
    method, path = ROUTES[operation]
    return method, path.format(**{
        name: quote(value, safe="") for name, value in parameters.items()
    })
