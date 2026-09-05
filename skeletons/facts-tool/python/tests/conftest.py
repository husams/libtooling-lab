import sqlite3
from pathlib import Path

import pytest
from support.facts_data import add_facts
from support.project_data import add_project


def _schema(name: str) -> str:
    return (Path(__file__).parent / "fixtures" / name).read_text(encoding="utf-8")


@pytest.fixture
def paired_databases(tmp_path: Path) -> tuple[Path, Path]:
    facts = tmp_path / "facts.sqlite"
    project = tmp_path / "project.sqlite"
    with sqlite3.connect(facts) as db:
        db.executescript(_schema("facts_schema.sql"))
        add_facts(db)
    with sqlite3.connect(project) as db:
        db.executescript(_schema("project_schema.sql"))
        add_project(db, tmp_path / "checkout λ with spaces")
    return facts, project
