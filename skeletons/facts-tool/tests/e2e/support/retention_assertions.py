import sqlite3
from pathlib import Path


def commands(c):
    shown = {entry["file"]: entry for entry in c.stored()}
    assert len(shown) == 2, shown
    for source, directory, base in zip(c.sources, c.directories, c.base):
        entry = shown[str(source)]
        expected = base + c.cli
        actual = entry["arguments"]
        if actual[1:2] == ["--driver-mode=g++"]:
            expected.insert(1, "--driver-mode=g++")
        assert len(actual) == len(expected), (actual, expected)
        for index, (observed, literal) in enumerate(zip(actual, expected)):
            allowed = {literal}
            # Either the literal JSON -I operand or its resolved equivalent
            # preserves meaning under the unchanged compilation directory.
            if index and expected[index - 1] == "-I":
                allowed.add(str((directory / literal).resolve()))
            assert observed in allowed, (index, observed, allowed)
        assert Path(entry["directory"]) == directory, entry
    assert c.snapshot() == c.original


def effects(c, family):
    yaml_active = c.yaml and not c.cli and not getattr(c, "runtime", False)
    with sqlite3.connect(c.db) as db:
        files = dict(db.execute("SELECT id,name FROM file"))
    if family == "extract":
        with sqlite3.connect(c.d.root / "extract.db") as db:
            symbols = set(row[0] for row in db.execute("SELECT qualified_name FROM symbol"))
        for unit in ("A", "B"):
            assert "JsonOnly" + unit in symbols, symbols
            assert ("Both" + unit in symbols) == (yaml_active and c.version == "YAML"), symbols
            assert ("NewBoth" + unit in symbols) == (yaml_active and c.version == "NEW"), symbols
            assert ("Cli" + unit in symbols) == bool(c.cli), symbols
            assert ("Runtime" + unit in symbols) == bool(getattr(c, "runtime", False)), symbols
        return
    names = set(files.values())
    if family == "dependency":
        with sqlite3.connect(c.d.root / "dependency.db") as db:
            edges = {(files[a], files[b]) for a, b in db.execute(
                "SELECT src_file_id,dst_file_id FROM include_dependency")}
        assert ("unit_a.cpp", "json_a.hpp") in edges, edges
        assert ("unit_a.cpp", "json words") in edges, edges
        names = {b for _, b in edges}
        if yaml_active:
            assert (c.version + " header.hpp", c.version + " marker.hpp") in edges, edges
            old = "NEW" if c.version == "YAML" else "YAML"
            assert (old + " header.hpp", old + " marker.hpp") not in edges, edges
    for probe in ("both_a.hpp", "both_b.hpp"):
        assert (probe in names) == yaml_active, (probe, names)
    for probe in ("cli_a.hpp", "cli_b.hpp"):
        assert (probe in names) == bool(c.cli), (probe, names)
