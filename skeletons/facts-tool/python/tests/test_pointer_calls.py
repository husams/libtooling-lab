import sqlite3

import pytest
from support.pointer_call_seed import add_pointer_run

from facts_tool import CallGraphPointerCall, FactsToolError, open_codebase
from facts_tool.catalog_relations import relation_id


def test_pointer_calls_are_separate_paged_snapshots(schema13_pair):
    facts, project, _ = schema13_pair
    source, target = add_pointer_run(facts)
    with sqlite3.connect(facts) as db:
        db.execute("DELETE FROM callgraph_pointer_call_site")
        db.execute("DELETE FROM symbol WHERE id=?", (target,))
    before = facts.read_bytes(), project.read_bytes()
    with open_codebase(facts_db=facts, project_db=project) as cb:
        run = cb.callgraphs.get(1, limit=1)
        assert run.edges.total == 0 and not run.edges.items
        assert run.pointer_calls.total == 2 and run.pointer_calls.next_cursor == 1
        assert run.truncated and run.status == "complete"
        pointer = run.pointer_calls[0]
        assert isinstance(pointer, CallGraphPointerCall)
        assert (pointer.source_id, pointer.target_id) == (source, target)
        assert (pointer.target_name, pointer.target_usr) == ("fp", "fp-usr")
        assert pointer.signature == "void (*)(int)" and pointer.expression == "fp"
        assert (pointer.offset, pointer.line, pointer.column) == (200, 20, 3)
        assert pointer.file and pointer.file.endswith("main.cpp")
        assert run.to_dict()["pointer_calls"]["items"][0]["kind"] == "pointer-call"
        next_page = cb.callgraphs.get(1, limit=1, cursors={"pointer_calls": 1})
        indirect = next_page.pointer_calls[0]
        assert indirect.target_id is None and indirect.target_usr is None
        assert indirect.expression == "factory()" and indirect.signature == "void (*)()"
        assert next_page.pointer_calls.complete and not next_page.truncated
        empty = cb.callgraphs.get(1, cursors={"pointer_calls": 2}).pointer_calls
        assert empty.total == 2 and not empty and empty.complete
    assert (facts.read_bytes(), project.read_bytes()) == before


def test_pointer_call_alias_preserves_function_edge_distinction():
    assert relation_id("pointer-call") == relation_id("pointer_calls") == 24
    assert relation_id("calls") == 1 and relation_id("dispatch_calls") == 18


def test_schema14_requires_pointer_snapshot_columns(schema13_pair):
    facts, project, _ = schema13_pair
    add_pointer_run(facts)
    with sqlite3.connect(facts) as db:
        db.execute("ALTER TABLE callgraph_run_pointer_call_site DROP COLUMN signature")
    with pytest.raises(FactsToolError, match="signature"):
        open_codebase(facts_db=facts, project_db=project)
