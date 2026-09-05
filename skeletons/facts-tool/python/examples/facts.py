from common import database_paths

from facts_tool import open_codebase
from facts_tool.queryplan import out, start, symbol

facts, project = database_paths("Inspect parameters and templates")
with open_codebase(facts_db=facts, project_db=project) as cb:
    queries = (
        start(symbol("app::run")) | out("has_parameter"),
        start(symbol("app::Box")) | out("has_template_parameter"),
        start(symbol("app::Box<int>")) | out("has_template_argument"),
    )
    for query in queries:
        print(cb.executor.run(query.plan).to_json())
