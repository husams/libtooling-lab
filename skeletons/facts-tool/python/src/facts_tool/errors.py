from typing import Never


class FactsToolError(Exception):
    """Stable public failure with a machine-readable code."""

    def __init__(self, code: str, message: str):
        self.code = code
        self.message = message
        super().__init__(f"{code}: {message}")


def fail(code: str, message: str) -> Never:
    raise FactsToolError(code, message)
