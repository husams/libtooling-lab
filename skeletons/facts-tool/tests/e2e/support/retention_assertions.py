import sqlite3
from pathlib import Path


def commands(c):
    shown = {entry["file"]: entry for entry in c.stored()}
    assert len(shown) == 2, shown
    for source, directory, base in zip(c.sources, c.directories, c.base):
        entry = shown[str(source)]
        # Existing persistence expands -I paths through portable component
        # labels; readback resolves them absolutely, preserving their meaning.
        expected = list(base)
        for index, token in enumerate(base[:-1]):
            if token == "-I":
                expected[index + 1] = str((directory / base[index + 1]).resolve())
        actual = entry["arguments"]
        if actual[1:2] == ["--driver-mode=g++"]:
            expected.insert(1, "--driver-mode=g++")
        assert actual == expected + c.cli, (actual, expected + c.cli)
        assert Path(entry["directory"]) == directory, entry
    assert c.snapshot() == c.original


def effects(c, family):
    with sqlite3.connect(c.db) as db:
        files = dict(db.execute("SELECT id,name FROM file"))
    if family == "extract":
        with sqlite3.connect(c.d.root / "extract.db") as db:
            symbols = set(row[0] for row in db.execute("SELECT qualified_name FROM symbol"))
        for unit in ("A", "B"):
            assert "JsonOnly" + unit in symbols, symbols
            assert ("Both" + unit in symbols) == (c.yaml and c.version == "YAML"), symbols
            assert ("NewBoth" + unit in symbols) == (c.yaml and c.version == "NEW"), symbols
            assert ("Cli" + unit in symbols) == bool(c.cli), symbols
        assert "LeakedFromA" not in symbols
        return
    names = set(files.values())
    if family == "dependency":
        with sqlite3.connect(c.d.root / "dependency.db") as db:
            edges = {(files[a], files[b]) for a, b in db.execute(
                "SELECT src_file_id,dst_file_id FROM include_dependency")}
        assert ("unit_a.cpp", "json_a.hpp") in edges, edges
        assert ("unit_a.cpp", "json words") in edges, edges
        names = {b for _, b in edges}
        if c.yaml:
            assert (c.version + " header.hpp", c.version + " marker.hpp") in edges, edges
            old = "NEW" if c.version == "YAML" else "YAML"
            assert (old + " header.hpp", old + " marker.hpp") not in edges, edges
    for probe in ("both_a.hpp", "both_b.hpp"):
        assert (probe in names) == c.yaml, (probe, names)
    for probe in ("cli_a.hpp", "cli_b.hpp"):
        assert (probe in names) == bool(c.cli), (probe, names)
