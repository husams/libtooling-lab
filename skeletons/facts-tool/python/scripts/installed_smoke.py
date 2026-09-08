import json
import sys
from pathlib import Path

from facts_tool import open_codebase
from facts_tool.queryplan import out, start, symbol

facts, project = map(Path, sys.argv[1:3])
graph = len(sys.argv) > 3 and sys.argv[3] == "graph"
evidence = len(sys.argv) > 3 and sys.argv[3] == "evidence"
with open_codebase(facts_db=facts, project_db=project) as cb:
    result = cb.executor.run((start(symbol("app::run")) | out("calls")).plan)
    assert [row["name"] for row in result] == ["save"]
    if graph:
        assert cb.callgraphs.latest().status == "complete"
    if evidence:
        expressions = cb.evidence.expressions("app::run")
        writes = cb.evidence.field_writes("app::Box::value")
        regions = cb.evidence.source_sections("app::run", include_text=True)
        assert expressions.rows and writes.rows and regions.rows[0]["text"]
        assert expressions.provenance.facts.schema.user_version == 13
    print(json.dumps({"package": "facts-tool-query", "query": "pass"}))
