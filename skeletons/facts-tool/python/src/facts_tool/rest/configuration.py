"""Validate connection settings without consulting the environment."""

import math
from urllib.parse import urlsplit, urlunsplit


def base_address(value: str) -> str:
    if not isinstance(value, str) or any(c.isspace() or ord(c) < 32 for c in value):
        raise ValueError("base_url must be an HTTP(S) URL without whitespace")
    if any(character in value for character in "\\?#"):
        raise ValueError("base_url cannot contain backslashes, a query, or a fragment")
    try:
        url = urlsplit(value)
        valid = (
            url.scheme in {"http", "https"}
            and bool(url.hostname)
            and url.username is None
            and url.password is None
            and (url.port is None or url.port > 0)
        )
    except ValueError as error:
        raise ValueError("Invalid base_url") from error
    if not valid:
        raise ValueError("base_url must be HTTP(S) with a host and no credentials")
    return urlunsplit((url.scheme, url.netloc, url.path.rstrip("/") + "/", "", ""))


def auth_headers(token: str | None) -> dict[str, str]:
    if token is None:
        return {}
    if not isinstance(token, str) or not token or any(
        ord(character) <= 32 or ord(character) >= 127 for character in token
    ):
        raise ValueError("token must be a nonempty ASCII value without whitespace")
    return {"Authorization": f"Bearer {token}"}


def seconds(value: float, name: str, *, allow_zero: bool = False) -> float:
    if (
        isinstance(value, bool)
        or not isinstance(value, int | float)
        or not math.isfinite(value)
        or value < 0
        or (value == 0 and not allow_zero)
    ):
        bound = "nonnegative" if allow_zero else "positive"
        raise ValueError(f"{name} must be finite and {bound}")
    return float(value)
