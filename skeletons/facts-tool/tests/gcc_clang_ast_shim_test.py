from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "gcc-clang-ast-shim.py"
FIXTURE = ROOT / "tests" / "fixtures" / "gcc-clang-ast-shim" / "gcc-failing-tu-fixture.json"


def load_shim():
    spec = importlib.util.spec_from_file_location("gcc_clang_ast_shim", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError(f"cannot load {SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class GccClangAstShimTest(unittest.TestCase):
    def test_transforms_only_known_gcc_units(self) -> None:
        shim = load_shim()
        entries = json.loads(FIXTURE.read_text())

        transformed, manifest = shim.transform(entries)

        self.assertEqual(len(manifest), 2)
        self.assertEqual(transformed[0]["command"].count("-D__noreturn__="), 1)
        self.assertEqual(
            transformed[1]["arguments"].count(
                "-D__builtin_ia32_rdseed_si_step=__builtin_ia32_rdseed32_step"
            ),
            1,
        )
        self.assertEqual(transformed[2], entries[2])

    def test_cli_writes_compile_database_and_manifest(self) -> None:
        shim = load_shim()
        with tempfile.TemporaryDirectory(prefix="facts-tool-gcc-shim-") as directory:
            root = Path(directory)
            source = root / "compile_commands.json"
            output = root / "out" / "compile_commands.json"
            manifest = root / "out" / "manifest.json"
            source.write_text(FIXTURE.read_text())

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--input",
                    str(source),
                    "--output",
                    str(output),
                    "--manifest",
                    str(manifest),
                ],
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("shimmed 2 of 3 compile commands", result.stdout)
            self.assertEqual(len(json.loads(output.read_text())), 3)
            self.assertEqual(len(json.loads(manifest.read_text())), 2)


if __name__ == "__main__":
    unittest.main()
