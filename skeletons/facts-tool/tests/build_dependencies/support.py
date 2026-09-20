"""Small standalone CMake consumer and observable build assertions."""
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = Path(__file__).parent / "consumer"


def run(command, *, env=None, cwd=None):
    result = subprocess.run(command, text=True, capture_output=True, env=env,
                            cwd=cwd, timeout=90, check=False)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout + result.stderr


class Consumer:
    def __init__(self, directory, cmake, cxx, boost, json):
        self.build = directory / "consumer build"
        self.cmake, self.cxx = cmake, cxx
        self.boost, self.json = boost.resolve(), json.resolve()
        assert (self.boost / "boost/beast.hpp").is_file()
        assert (self.json / "nlohmann/json.hpp").is_file()

    def configure(self, *options):
        return run([
            self.cmake, "-S", str(FIXTURE), "-B", str(self.build),
            "-Werror=dev", f"-DFACTS_ROOT={ROOT}",
            f"-DCMAKE_CXX_COMPILER={self.cxx}",
            "-DFETCHCONTENT_FULLY_DISCONNECTED=ON", *options,
        ])

    def execute(self, *options):
        output = self.configure(*options)
        run([self.cmake, "--build", str(self.build), "--parallel", "2"])
        assert run([str(self.build / "api-consumer")]).strip() == '{"status":"ok"}'
        return output


def package(prefix, name, version, target, include):
    directory = prefix / name
    directory.mkdir(parents=True)
    (directory / f"{name}Config.cmake").write_text(
        f"add_library({target} INTERFACE IMPORTED)\n"
        f'set_target_properties({target} PROPERTIES '
        f'INTERFACE_INCLUDE_DIRECTORIES "{include.as_posix()}")\n'
    )
    (directory / f"{name}ConfigVersion.cmake").write_text(
        f'set(PACKAGE_VERSION "{version}")\n'
        "if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)\n"
        "  set(PACKAGE_VERSION_COMPATIBLE FALSE)\n"
        "else()\n"
        "  set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
        "endif()\n"
    )
    return directory
