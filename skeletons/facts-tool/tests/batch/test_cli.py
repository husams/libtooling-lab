import json
import subprocess

import pytest

from conftest import records


def invoke(batch, *args):
    command, env, _, _ = batch
    return subprocess.run(
        command + list(map(str, args)),
        env=env,
        capture_output=True,
        text=True,
        timeout=15,
    )


@pytest.mark.parametrize("jobs", ["0", "-1", "nope"])
def test_invalid_jobs_launch_nothing(batch, jobs):
    result = invoke(batch, "-j", jobs, batch[2][0])
    assert result.returncode != 0
    assert not records(batch[3])


def test_missing_sources_launch_nothing(batch):
    assert invoke(batch).returncode != 0
    assert not records(batch[3])


def test_missing_tool_is_actionable(batch):
    batch[2][0].parent.joinpath("facts-tool").unlink()
    result = invoke(batch, batch[2][0])
    assert result.returncode != 0
    assert "facts-tool" in result.stderr and "PATH" in result.stderr


def test_arguments_are_preserved_without_shell(batch):
    extra = '-DNAME="hello $(touch nope)"'
    result = invoke(
        batch,
        "-c",
        "project words.db",
        "--config",
        "config words.yaml",
        "--extra-arg=" + extra,
        "-v",
        "2",
        batch[2][0],
    )
    assert result.returncode == 0, result.stderr
    args = records(batch[3])[0]["args"]
    assert "project words.db" in args and "config words.yaml" in args
    assert extra in args or "--extra-arg=" + extra in args
    assert args[-1] == str(batch[2][0])


def test_file_list_and_duplicates(batch, tmp_path):
    listing = tmp_path / "sources.txt"
    listing.write_text("\n".join(map(str, batch[2])) + "\n\n")
    result = invoke(batch, "--files-from", listing, batch[2][0])
    assert result.returncode == 0, result.stderr
    assert len(records(batch[3])) == 5


def test_compdb_relative_paths_and_same_basenames(batch, tmp_path):
    other = tmp_path / "other"
    other.mkdir()
    (other / batch[2][0].name).touch()
    db = tmp_path / "compile_commands.json"
    db.write_text(
        json.dumps(
            [
                {"directory": ".", "file": batch[2][0].name},
                {"directory": "other", "file": batch[2][0].name},
            ]
        )
    )
    result = invoke(batch, "-p", db)
    assert result.returncode == 0, result.stderr
    assert len(list((tmp_path / "out").glob("*.db"))) == 2


def test_failure_does_not_hide_success(batch, tmp_path):
    failed = tmp_path / "fail.cpp"
    failed.touch()
    result = invoke(batch, "-j", 2, failed, *batch[2])
    assert result.returncode != 0
    assert len(records(batch[3])) == 6
    logs = list((tmp_path / "out").glob("*.log"))
    assert logs and all("child diagnostic" in path.read_text() for path in logs)


def test_stdin_file_list(batch):
    command, env, sources, trace = batch
    result = subprocess.run(
        command + ["--files-from", "-"],
        env=env,
        input="\n".join(map(str, sources)),
        capture_output=True,
        text=True,
        timeout=15,
    )
    assert result.returncode == 0, result.stderr
    assert len(records(trace)) == len(sources)


def test_invalid_source_prevents_partial_launch(batch, tmp_path):
    result = invoke(batch, batch[2][0], tmp_path / "absent.cpp")
    assert result.returncode != 0
    assert not records(batch[3])


@pytest.mark.parametrize("content", ["not json", "{}", '[{"file": 42}]'])
def test_bad_compdb_is_actionable(batch, tmp_path, content):
    db = tmp_path / "compile_commands.json"
    db.write_text(content)
    result = invoke(batch, "-p", db)
    assert result.returncode != 0
    assert "Traceback" not in result.stderr
    assert not records(batch[3])
