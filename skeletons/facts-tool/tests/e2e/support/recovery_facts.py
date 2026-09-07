"""Facts/project-store lookups shared by S-021 recovery step modules."""
import json
import sqlite3


def query(database, sql, parameters=()):
    with sqlite3.connect(database) as connection:
        return connection.execute(sql, parameters).fetchall()


_TU_SQL = ("SELECT file.id FROM file JOIN directory ON directory.id="
           "file.directory_id JOIN component ON component.id="
           "directory.component_id WHERE component.name=? AND "
           "file.driver IS NOT NULL")


def component_tu(context, component, name=None):
    """The registered TU (file id) for `component`, optionally by file name."""
    sql, params = _TU_SQL, [component]
    if name is not None:
        sql += " AND file.name=?"
        params.append(name)
    rows = query(context.files_database_path, sql, params)
    assert len(rows) == 1, (component, name, rows)
    return rows[0][0]


def component_arguments(context, component, name=None):
    """The registered compile_options (parsed JSON) for a component's TU."""
    file_id = component_tu(context, component, name)
    rows = query(context.files_database_path,
                 "SELECT compile_options FROM file WHERE id=?", (file_id,))
    return json.loads(rows[0][0])


def facts_snapshot(facts):
    """Snapshot of the user-fact tables recovery must never mutate silently."""
    return {name: query(facts, f"SELECT * FROM {name} ORDER BY 1,2,3,4")
            for name in ("symbol", "relation", "relation_site", "definition")}


def has_definition(facts, qualified_name):
    rows = query(facts, "SELECT 1 FROM definition d JOIN symbol s ON "
                 "s.id=d.symbol_id WHERE s.qualified_name=?", (qualified_name,))
    return bool(rows)


def is_external(facts, qualified_name):
    rows = query(facts, "SELECT is_external FROM symbol WHERE qualified_name=?",
                 (qualified_name,))
    return bool(rows) and bool(rows[0][0])
