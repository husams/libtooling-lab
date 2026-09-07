from typing import Any

from .callgraph_page import CallGraphPage


def page_dict(page: CallGraphPage[Any]) -> dict[str, Any]:
    return {
        "items": [row.to_dict() for row in page.items],
        "total": page.total,
        "next_cursor": page.next_cursor,
        "complete": page.complete,
    }
