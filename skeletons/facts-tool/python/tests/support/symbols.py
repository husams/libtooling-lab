import sqlite3

from .symbol_factory import COLUMNS, symbol


def add_symbols(db: sqlite3.Connection) -> dict[str, int]:
    names = (
        "run",
        "save",
        "persist",
        "box",
        "field",
        "method",
        "box_int",
        "color",
        "red",
        "relation_source",
        "relation_target",
        "diamond_source",
        "diamond_left",
        "diamond_right",
        "diamond_end",
        "cycle_a",
        "cycle_b",
    )
    ids = {name: (1 << 32) + index for index, name in enumerate(names, 1)}
    rows = (
        symbol(
            ids["run"], 1, 12, "c:@F@run#", "app::run", is_definition=1, is_noexcept=1
        ),
        symbol(ids["save"], 1, 12, "c:@F@save#", "app::save", is_definition=1),
        symbol(ids["persist"], 1, 12, "c:@F@persist#", "app::persist"),
        symbol(ids["box"], 2, 7, "c:@S@Box", "app::Box", is_definition=1),
        symbol(ids["field"], 4, 14, "c:@S@Box@FI@value", "app::Box::value"),
        symbol(
            ids["method"],
            1,
            16,
            "c:@S@Box@F@flush#",
            "app::Box::flush()",
            is_virtual=1,
            is_pure=1,
        ),
        symbol(ids["box_int"], 2, 7, "c:@S@Box>#I", "app::Box<int>"),
        symbol(ids["color"], 3, 5, "c:@E@Color", "app::Color"),
        symbol(ids["red"], 6, 15, "c:@E@Color@Red", "app::Color::Red"),
        symbol(
            ids["relation_source"],
            5,
            2,
            "c:@F@relationSource#",
            "fixture::relationSource",
        ),
        symbol(
            ids["relation_target"],
            5,
            2,
            "c:@F@relationTarget#",
            "fixture::relationTarget",
        ),
        *(
            symbol(ids[name], 5, 2, f"c:@N@{name}", f"fixture::{name}")
            for name in names[-6:]
        ),
        symbol(1, 5, 11, "c:@T@int", "int", is_external=1),
    )
    columns = ",".join(COLUMNS)
    placeholders = ",".join("?" for _ in COLUMNS)
    db.executemany(f"INSERT INTO symbol({columns}) VALUES({placeholders})", rows)
    return ids
