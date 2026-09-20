"""The real DEPS_ONLY script must produce a usable cache for an offline build."""
import os
import shutil

from support import ROOT, run


def test_rhel_cached_headers_reused_and_override_updated(consumer, sources, tmp_path):
    root = tmp_path / "isolated project"
    scripts = root / "scripts"
    scripts.mkdir(parents=True)
    shutil.copy2(ROOT / "scripts/build-rhel9.sh", scripts)
    shutil.copytree(ROOT / "scripts/dependencies", scripts / "dependencies")
    shutil.copytree(ROOT / "cmake", root / "cmake")
    # Dependency installers regard CMakeLists.txt as their existing-source marker.
    cached = tmp_path / "other cached sources"
    cached.mkdir()
    (cached / "CMakeLists.txt").write_text("# Cached; DEPS_ONLY does not compile.\n")
    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    (bin_dir / "cmake").symlink_to(consumer.cmake)
    boost, json = sources
    env = dict(os.environ, PATH=f"{bin_dir}{os.pathsep}{os.environ['PATH']}",
               DEPS_ONLY="1", SKIP_DEPS="1", YAML_SOURCE_DIR=str(cached),
               LIBGIT2_SOURCE_DIR=str(cached), BOOST_SOURCE_DIR=str(boost),
               JSON_SOURCE_DIR=str(json), API_HEADER_CACHE_DIR=str(tmp_path / "cache"))
    command = ["bash", str(scripts / "build-rhel9.sh")]
    output = run(command, env=env)
    assert "DEPS_ONLY" in output
    cache = tmp_path / "cache/configure/api-header-cache.cmake"
    assert str(boost) in cache.read_text()
    assert str(json / "include") in cache.read_text()
    run(command, env=env)
    # Changing an override must replace the previous CMake cache entry.
    replacement = tmp_path / "replacement boost"
    replacement.mkdir()
    (replacement / "boost").symlink_to(consumer.boost / "boost", target_is_directory=True)
    run(command, env=dict(env, BOOST_SOURCE_DIR=str(replacement)))
    assert str(replacement) in cache.read_text()
    assert f"[==[{boost}]==]" not in cache.read_text()
    consumer.execute("-C", str(cache))
