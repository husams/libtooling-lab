"""Representative native domain API records for transport-boundary tests."""


def domain_record(state="queued", **changes):
    return {
        "id": "domain-7", "operation": "extract", "state": state,
        "created_at": 1750000000000, "result": None, "error": None,
    } | changes


def symbol_record():
    return {
        "qualified_name": "example::Widget", "kind": "class", "usr": "c:@S@Widget",
        "is_definition": True, "file_id": 21, "path": "/checkout/widget.hpp",
        "repo": "example",
        "component": "core", "clone": "main",
    }


def index_record():
    return {
        "state": "ready", "pending": False, "files": 2, "symbols": 7,
        "error": None, "updated_at": 1750000000010,
    }
