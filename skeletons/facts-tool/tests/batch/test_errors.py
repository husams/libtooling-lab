from conftest import records
from test_cli import invoke


def test_unreadable_text_list_has_no_traceback(batch, tmp_path):
    listing = tmp_path / "bad-list"
    listing.write_bytes(b"\xff\xfe")
    result = invoke(batch, "--files-from", listing)
    assert result.returncode != 0
    assert "Traceback" not in result.stderr
    assert not records(batch[3])


def test_unlaunchable_tool_has_no_traceback(batch):
    tool = batch[2][0].parent / "facts-tool"
    tool.write_text("#!/missing/interpreter\n")
    result = invoke(batch, batch[2][0])
    assert result.returncode != 0
    assert "Traceback" not in result.stderr
    assert not records(batch[3])
