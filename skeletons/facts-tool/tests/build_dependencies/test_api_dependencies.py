"""Exercise config packages, bare headers and disconnected source overrides."""
from support import package

NO_PACKAGES = (
    "-DCMAKE_DISABLE_FIND_PACKAGE_Boost=ON",
    "-DCMAKE_DISABLE_FIND_PACKAGE_nlohmann_json=ON",
)


def test_installed_config_packages(consumer, tmp_path):
    boost = package(tmp_path, "Boost", "1.83.0", "Boost::headers", consumer.boost)
    json = package(tmp_path, "nlohmann_json", "3.11.2",
                   "nlohmann_json::nlohmann_json", consumer.json)
    consumer.execute(f"-DBoost_DIR={boost}", f"-Dnlohmann_json_DIR={json}")


def test_headers_without_config_packages(consumer, sources):
    boost, json = sources
    output = consumer.execute(*NO_PACKAGES,
                              f"-DCMAKE_INCLUDE_PATH={boost};{json / 'include'}")
    assert f"API Boost headers: {boost}" in output
    assert f"API nlohmann_json headers: {json / 'include'}" in output


def test_offline_source_overrides(consumer, sources):
    boost, json = sources
    consumer.execute(*NO_PACKAGES,
                     f"-DFETCHCONTENT_SOURCE_DIR_BOOST={boost}",
                     f"-DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON={json}")


def test_offline_include_overrides(consumer, sources):
    boost, json = sources
    consumer.execute(*NO_PACKAGES, f"-DFACTS_BOOST_INCLUDE_DIR={boost}",
                     f"-DFACTS_JSON_INCLUDE_DIR={json / 'include'}")


def test_missing_system_headers_use_populated_download_cache(consumer, sources):
    boost, json = sources
    cache = consumer.build / "_deps"
    cache.mkdir(parents=True)
    (cache / "boost-src").symlink_to(boost, target_is_directory=True)
    (cache / "nlohmann_json-src").symlink_to(json, target_is_directory=True)
    ignored = f"{consumer.boost};{consumer.json};/usr/include;/usr/local/include"
    output = consumer.execute(*NO_PACKAGES, f"-DCMAKE_IGNORE_PATH={ignored}")
    assert "fetching pinned Boost" in output
    assert "fetching pinned 3.11.3" in output
    assert f"API Boost headers: {cache / 'boost-src'}" in output
    assert f"API nlohmann_json headers: {cache / 'nlohmann_json-src/include'}" in output
