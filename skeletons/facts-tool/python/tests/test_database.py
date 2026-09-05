from pathlib import Path

import pytest

from facts_tool import FactsToolError, open_codebase


def test_pair_opens_and_closes(paired_databases: tuple[Path, Path]) -> None:
    facts, project = paired_databases
    before = (facts.read_bytes(), project.read_bytes())
    with open_codebase(facts_db=facts, project_db=project) as codebase:
        assert codebase.provenance.pairing == "unverifiable"
    assert (facts.read_bytes(), project.read_bytes()) == before


def test_same_file_is_rejected(paired_databases: tuple[Path, Path]) -> None:
    facts, _ = paired_databases
    with pytest.raises(FactsToolError, match="E_DATABASE_ROLE"):
        open_codebase(facts_db=facts, project_db=facts)


def test_missing_path_is_not_created(tmp_path: Path) -> None:
    missing = tmp_path / "missing.sqlite"
    with pytest.raises(FactsToolError, match="E_DATABASE"):
        open_codebase(facts_db=missing, project_db=tmp_path / "other.sqlite")
    assert not missing.exists()


def test_corrupt_database_is_rejected(
    paired_databases: tuple[Path, Path], tmp_path: Path
) -> None:
    corrupt = tmp_path / "corrupt.sqlite"
    corrupt.write_text("not sqlite", encoding="utf-8")
    with pytest.raises(FactsToolError, match="E_DATABASE"):
        open_codebase(facts_db=corrupt, project_db=paired_databases[1])
