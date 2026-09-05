from common import database_paths

from facts_tool import open_codebase
from facts_tool.queryplan import codebase, glob, nodes, out, select, start, view

facts, project = database_paths("Inspect project files and includes")
with open_codebase(facts_db=facts, project_db=project) as cb:
    files = (
        start(codebase())
        | view("file")
        | nodes()
        | select(("id", "path", "driver", "compile_options"))
    )
    includes = (
        start(codebase())
        | view("file")
        | nodes(glob("path", "*main.cpp"))
        | out("includes")
    )
    print(cb.executor.run(files.plan).to_json())
    print(cb.executor.run(includes.plan).to_json())
