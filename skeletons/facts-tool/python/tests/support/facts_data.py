import sqlite3

from .symbols import add_symbols


def add_facts(db: sqlite3.Connection) -> None:
    ids = add_symbols(db)
    edges = (
        (ids["run"], ids["save"], 1, 0),
        (ids["save"], ids["persist"], 1, 0),
        (ids["run"], ids["box"], 7, 0),
        (ids["box"], ids["field"], 3, 0),
        (ids["box"], ids["method"], 3, 0),
        (ids["field"], ids["box"], 8, 0),
        (ids["method"], ids["box"], 9, 0),
        (ids["field"], 1, 20, 0),
        (ids["run"], 1, 21, 0),
        (ids["run"], ids["box"], 22, 0),
        (ids["box_int"], ids["box"], 5, 0),
        (ids["box_int"], 1, 23, 0),
        (ids["diamond_source"], ids["diamond_left"], 1, 0),
        (ids["diamond_source"], ids["diamond_right"], 1, 0),
        (ids["diamond_left"], ids["diamond_end"], 1, 0),
        (ids["diamond_right"], ids["diamond_end"], 1, 0),
        (ids["cycle_a"], ids["cycle_b"], 1, 0),
        (ids["cycle_b"], ids["cycle_a"], 1, 0),
        *(
            (ids["relation_source"], ids["relation_target"], kind, 0)
            for kind in range(1, 24)
        ),
    )
    db.executemany("INSERT INTO relation VALUES(?,?,?,?,'none',0,0,0,1)", edges)
    db.executemany(
        "INSERT INTO relation_site VALUES(?,?,?,?,?,?,?,?,?,?)",
        (
            (ids["run"], ids["save"], 1, 0, 1, 12, 4, 120, ids["box"], 1),
            (ids["run"], ids["save"], 1, 0, 1, 14, 4, 140, None, 0),
            (ids["save"], ids["persist"], 1, 0, 2, 8, 2, 80, None, 0),
        ),
    )
    db.execute("INSERT INTO definition VALUES(?,?,?,?)", (ids["run"], 2, 40, 30))
    db.execute("INSERT INTO callable_return_type VALUES(?,?)", (ids["run"], "int"))
    db.execute(
        "INSERT INTO parameter VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
        (ids["run"], 0, "box", ids["box"], 10, 10, 100, 95, 12, 0, 0, 0, 0, 1, 0, 1),
    )
    db.execute(
        "INSERT INTO parameter_default VALUES(?,?,?,?,?)",
        (ids["run"], 0, "{}", "none", None),
    )
    db.executemany(
        "INSERT INTO template_argument VALUES(?,?,?,?,?,?,?)",
        (
            (ids["box"], 0, "T", 0, 0, 0, 0),
            (ids["box"], 1, "Args", 1, 1, 1, 0),
        ),
    )
    db.executemany(
        "INSERT INTO template_parameter VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
        (
            (ids["box_int"], 0, "", 1, 0, 0, 0, 0, 0, 0, 0, 0),
            (ids["box_int"], 1, "7", 1, 0, 0, 0, 0, 1, 1, 1, 1),
        ),
    )
    db.execute(
        "UPDATE relation SET access='public',is_implicit=1,is_lexical=1,count=2 "
        "WHERE source_id=? AND destination_id=? AND kind=1",
        (ids["run"], ids["save"]),
    )
    db.execute("INSERT INTO enumeration VALUES(?,?,?,?)", (ids["color"], 1, 1, 1))
    db.execute("INSERT INTO enumerator VALUES(?,?,?)", (ids["red"], "1", "1"))
    db.execute(
        "INSERT INTO variable_initializer VALUES(?,?,?,?)",
        (ids["field"], "42", "integer", "42"),
    )
    db.execute("INSERT INTO include_dependency VALUES(?,?)", (1, 2))
