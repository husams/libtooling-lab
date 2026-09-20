"""Small v2 protocol fixtures independent of native execution."""

REPOSITORY = {
    "id": "repo-1",
    "name": "example",
    "kind": "git",
    "remote_url": None,
    "active_clone_id": "clone-1",
    "component_count": 1,
    "source_count": 1,
    "indexed_source_count": 1,
    "clones": [{"id": "clone-1", "label": "main", "path": "/src/example"}],
}
SYMBOL = {
    "symbol_id": "sym/1",
    "qualified_name": "example::Widget",
    "kind": "class",
    "usr": "c:@S@Widget",
    "repository": "example",
    "component": "core",
    "definition": {"file_id": "1", "path": "widget.cpp", "line": 1, "column": 2},
}
EXTRACTION = {
    "files_selected": 1,
    "files_processed": 1,
    "files_skipped": 0,
    "files_failed": 0,
    "coverage": "complete",
    "symbols_written": 3,
    "diagnostics": [],
    "files": [{"file_id": "1", "path": "widget.cpp", "symbol_count": 3}],
    "index_revision": "rev-3",
}


def job(state="queued", operation="extract", result=None):
    return {
        "id": "job-1",
        "operation": operation,
        "state": state,
        "created_at": 1,
        "started_at": None,
        "finished_at": None,
        "result": result,
        "error": None,
    }


WATCH = {
    "enabled": True,
    "running": True,
    "active": False,
    "pending": False,
    "scanning": False,
    "ready": True,
    "source": "project_database",
    "clones": [
        {
            "repository_id": "repo-1",
            "repository": "example",
            "clone_id": "clone-1",
            "label": "main",
            "path": "/src/example",
            "active": True,
            "excluded": "",
        }
    ],
    "notices": [],
    "directories": ["/src/example"],
    "events": 0,
    "cycles": 1,
    "failures": 0,
    "overflows": 0,
    "last_error": "",
    "latest_jobs": [],
    "import_mode": "reimport",
    "backend": "inotify",
    "watched_directories": 1,
    "warnings": [],
}
INDEX = {
    "state": "ready",
    "pending": False,
    "files": 1,
    "symbols": 3,
    "error": None,
    "updated_at": 2,
    "index_revision": "2",
}
