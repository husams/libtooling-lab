from common import database_paths

from facts_tool import open_codebase
from facts_tool.queryplan import codebase, eq, nodes, order_by, select, start

facts, project = database_paths("List persisted functions")
with open_codebase(facts_db=facts, project_db=project) as cb:
    query = (
        start(codebase())
        | nodes(eq("kind", "function"))
        | select(("name", "is_noexcept", "file", "line"))
        | order_by(("name",))
    )
    print(cb.executor.run(query.plan).to_json())
