"""Explicit unusable dependencies fail instead of silently changing source."""
import pytest


def test_old_boost_override_rejected(consumer, tmp_path):
    root = tmp_path / "old boost"
    (root / "boost").mkdir(parents=True)
    (root / "boost/version.hpp").write_text("#define BOOST_VERSION 107300\n")
    (root / "boost/asio.hpp").touch()
    (root / "boost/beast.hpp").touch()
    with pytest.raises(AssertionError, match="Boost >= 1.74") as error:
        consumer.configure(f"-DFETCHCONTENT_SOURCE_DIR_BOOST={root}")
    assert "FETCHCONTENT_SOURCE_DIR_BOOST" in str(error.value)
    assert "fetching pinned" not in str(error.value)


def test_old_json_override_rejected(consumer, tmp_path):
    root = tmp_path / "old json"
    (root / "nlohmann").mkdir(parents=True)
    (root / "nlohmann/json.hpp").write_text(
        "#define NLOHMANN_JSON_VERSION_MAJOR 3\n"
        "#define NLOHMANN_JSON_VERSION_MINOR 8\n"
        "#define NLOHMANN_JSON_VERSION_PATCH 0\n"
    )
    with pytest.raises(AssertionError, match="nlohmann_json >= 3.9") as error:
        consumer.configure(f"-DFACTS_BOOST_INCLUDE_DIR={consumer.boost}",
                           f"-DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON={root}")
    assert "FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON" in str(error.value)
    assert "fetching pinned" not in str(error.value)
