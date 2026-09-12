from __future__ import annotations

import json
import re

from pytest_bdd import then

from support.database import require
from support.entries import run, succeed

NAMES = ("left", "leaf", "boundary", "external", "indirect", "cycle_a")
FORMATS = ("text", "json")
CONF_VARIANTS = ((True, "conf"), (False, "noconf"))

# indexed_at/catalog_mtime are real wall-clock/filesystem timestamps recorded
# by a real extract; they are genuinely different on every run, unlike every
# other field here, so a byte-identical snapshot has to normalize an engaged
# value to a fixed placeholder on both sides rather than freeze one run's
# values. `null` (the "noconf"/never-indexed variants) is a real, stable
# value in its own right and must stay `null`, not get normalized away too.
_CATALOG_MTIME = re.compile(r'"catalog_mtime":(?!null)[-0-9.eE]+')
_INDEXED_AT = re.compile(r'"indexed_at":(?!null)"[^"]*"')


def _substitute(text: str, root: str) -> str:
    text = text.replace(root, "{root}")
    text = _CATALOG_MTIME.sub('"catalog_mtime":"{recorded}"', text)
    text = _INDEXED_AT.sub('"indexed_at":"{recorded}"', text)
    return text


@then("the call-graph-entry help and outputs are byte-identical to the snapshot")
def snapshot_matches(context):
    snapshot = json.loads((context.fixture_root / "entries" /
                            "call_graph_entry_snapshot.json").read_text())
    root = str(context.entry_root)
    help_result = succeed(run(context, "analyse", "call-graph-entry", "--help"))
    require(help_result.stdout == snapshot["help"], help_result.stdout)
    require(help_result.stderr == "" and help_result.returncode == 0, str(help_result))
    for name in NAMES:
        for fmt in FORMATS:
            for conf, label in CONF_VARIANTS:
                key = f"{name}-{fmt}-{label}"
                expected = snapshot["outputs"][key]
                arguments = ["analyse", "call-graph-entry", "-v", "0", "--facts",
                             context.facts_database_path, "--function",
                             f"s027::{name}", "--format", fmt]
                if conf:
                    arguments += ["--conf", context.files_database_path]
                result = run(context, *arguments)
                stdout = _substitute(result.stdout, root)
                stderr = _substitute(result.stderr, root)
                require(result.returncode == expected["exit"] and
                        stdout == expected["stdout"] and stderr == expected["stderr"],
                        f"{key}: exit={result.returncode} stdout={stdout!r} "
                        f"stderr={stderr!r} expected={expected!r}")
